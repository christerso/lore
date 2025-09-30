#include <lore/world/tile_damage.hpp>
#include <lore/world/voronoi_fracture.hpp>
#include <lore/core/log.hpp>

#include <random>
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace lore::core;

namespace lore::world {

TileDamageSystem::TileDamageSystem(const TileDamageConfig& config)
    : config_(config)
{
    LOG_INFO(Game, "TileDamageSystem initialized");
}

// Calculate damage from impact
float TileDamageSystem::calculate_impact_damage(float impact_force, ImpactType impact_type) const {
    // Base damage: force/1000 (5000N = 5% health)
    float base_damage = impact_force / 1000.0f;

    // Impact type multipliers
    float type_multiplier = 1.0f;
    switch (impact_type) {
        case ImpactType::PointImpact:
            type_multiplier = 0.8f;  // Bullets: concentrated but less total energy
            break;
        case ImpactType::BluntForce:
            type_multiplier = 1.2f;  // Hammers: high damage
            break;
        case ImpactType::Explosion:
            type_multiplier = 1.5f;  // Explosions: massive damage
            break;
        case ImpactType::Cutting:
            type_multiplier = 1.0f;  // Axes: moderate damage
            break;
        case ImpactType::Crushing:
            type_multiplier = 1.3f;  // Heavy objects: high damage
            break;
        case ImpactType::Shearing:
            type_multiplier = 1.1f;  // Lateral force: above average
            break;
    }

    return base_damage * type_multiplier * config_.impact_damage_multiplier;
}

// Get state threshold
float TileDamageSystem::get_state_threshold(TileState current_state) const {
    switch (current_state) {
        case TileState::Pristine:   return config_.scratched_threshold;
        case TileState::Scratched:  return config_.cracked_threshold;
        case TileState::Cracked:    return config_.damaged_threshold;
        case TileState::Damaged:    return config_.failing_threshold;
        case TileState::Failing:    return config_.critical_threshold;
        case TileState::Critical:   return 0.0f;  // Special case: time-based
        case TileState::Collapsed:  return 0.0f;  // Terminal state
        default:                    return 0.0f;
    }
}

// Update damage state based on health
bool TileDamageSystem::update_damage_state(TileDamage& damage) {
    TileState old_state = damage.state;
    TileState new_state = old_state;

    // Check for state transitions based on health
    if (damage.health <= 0.0f) {
        new_state = TileState::Collapsed;
    } else if (damage.state == TileState::Critical) {
        // Critical state is time-based
        if (damage.time_until_collapse <= 0.0f) {
            new_state = TileState::Collapsed;
        }
    } else {
        // Health-based transitions
        if (damage.health <= config_.critical_threshold) {
            new_state = TileState::Critical;
        } else if (damage.health <= config_.failing_threshold) {
            new_state = TileState::Failing;
        } else if (damage.health <= config_.damaged_threshold) {
            new_state = TileState::Damaged;
        } else if (damage.health <= config_.cracked_threshold) {
            new_state = TileState::Cracked;
        } else if (damage.health <= config_.scratched_threshold) {
            new_state = TileState::Scratched;
        } else {
            new_state = TileState::Pristine;
        }
    }

    // State transition
    if (new_state != old_state) {
        damage.state = new_state;
        damage.time_in_current_state = 0.0f;

        // Initialize Critical state countdown
        if (new_state == TileState::Critical) {
            damage.time_until_collapse = config_.critical_state_duration;
            LOG_INFO(Game, "Tile entered Critical state: {:.1f}s until collapse",
                    damage.time_until_collapse);
        }

        LOG_DEBUG(Game, "Tile state transition: {} → {} (health: {:.1f}%)",
                 static_cast<int>(old_state), static_cast<int>(new_state), damage.health);

        return true;
    }

    return false;
}

// Calculate crack propagation direction
math::Vec3 TileDamageSystem::calculate_crack_direction(
    const TileDamage& damage,
    [[maybe_unused]] const math::Vec3& current_position,
    const math::Vec3& previous_direction) const
{
    // Base direction: blend stress direction with previous direction
    math::Vec3 stress_dir = damage.primary_stress_direction;

    // If no stress direction, use previous direction
    if (stress_dir.x == 0.0f && stress_dir.y == 0.0f && stress_dir.z == 0.0f) {
        stress_dir = previous_direction;
    }

    // Blend (70% stress, 30% previous for smooth curves)
    math::Vec3 base_dir = {
        0.7f * stress_dir.x + 0.3f * previous_direction.x,
        0.7f * stress_dir.y + 0.3f * previous_direction.y,
        0.7f * stress_dir.z + 0.3f * previous_direction.z
    };

    // Normalize
    float length = std::sqrt(base_dir.x * base_dir.x +
                            base_dir.y * base_dir.y +
                            base_dir.z * base_dir.z);
    if (length > 1e-6f) {
        base_dir.x /= length;
        base_dir.y /= length;
        base_dir.z /= length;
    } else {
        // Fallback: random direction
        base_dir = {1.0f, 0.0f, 0.0f};
    }

    // Add random perturbation (±15 degrees)
    static std::mt19937 rng(static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    ));
    std::uniform_real_distribution<float> dist(-0.26f, 0.26f);  // ±15 degrees in radians

    float perturbation_angle = dist(rng);
    float cos_angle = std::cos(perturbation_angle);
    float sin_angle = std::sin(perturbation_angle);

    // Rotate around arbitrary perpendicular axis
    math::Vec3 perp = {
        base_dir.y,
        -base_dir.x,
        0.0f
    };

    float perp_len = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
    if (perp_len > 1e-6f) {
        perp.x /= perp_len;
        perp.y /= perp_len;
        perp.z /= perp_len;
    }

    // Rodrigues' rotation formula (simplified for small angles)
    math::Vec3 result = {
        base_dir.x * cos_angle + perp.x * sin_angle,
        base_dir.y * cos_angle + perp.y * sin_angle,
        base_dir.z * cos_angle + perp.z * sin_angle
    };

    // Re-normalize
    float result_len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z);
    if (result_len > 1e-6f) {
        result.x /= result_len;
        result.y /= result_len;
        result.z /= result_len;
    }

    return result;
}

// Check if crack should branch
bool TileDamageSystem::should_crack_branch(const CrackPath& crack, float impact_force) const {
    // Higher impact force = more likely to branch
    float force_factor = std::min(1.0f, impact_force / 10000.0f);  // Normalize to 0-1
    float branch_chance = config_.crack_branch_probability * force_factor;

    // Longer cracks are less likely to branch
    float length_factor = std::max(0.0f, 1.0f - (crack.total_length / 2.0f));  // 2m max
    branch_chance *= length_factor;

    static std::mt19937 rng(static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    ));
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    return dist(rng) < branch_chance;
}

// Initiate crack from impact
void TileDamageSystem::initiate_crack(
    TileDamage& damage,
    const math::Vec3& origin,
    const math::Vec3& primary_direction,
    float impact_force)
{
    CrackPath crack;
    crack.origin = origin;
    crack.direction = primary_direction;
    crack.total_length = 0.0f;
    crack.time_since_creation = 0.0f;
    crack.is_active = true;

    // Propagation speed based on impact force
    float force_factor = std::min(1.0f, impact_force / 10000.0f);
    crack.propagation_speed = config_.min_propagation_speed +
        force_factor * (config_.max_propagation_speed - config_.min_propagation_speed);

    // Create initial segment (very small)
    CrackSegment initial_segment;
    initial_segment.start_position = origin;
    initial_segment.end_position = {
        origin.x + primary_direction.x * 0.01f,
        origin.y + primary_direction.y * 0.01f,
        origin.z + primary_direction.z * 0.01f
    };
    initial_segment.width = 0.001f;  // 1mm initially
    initial_segment.depth = 0.5f;
    initial_segment.generation = 0;

    crack.segments.push_back(initial_segment);
    damage.crack_paths.push_back(crack);

    LOG_DEBUG(Game, "Initiated crack at [{:.2f}, {:.2f}, {:.2f}] speed={:.2f}m/s",
             origin.x, origin.y, origin.z, crack.propagation_speed);
}

// Propagate cracks
void TileDamageSystem::propagate_cracks(TileDamage& damage, float delta_time) {
    for (auto& crack : damage.crack_paths) {
        if (!crack.is_active) continue;

        crack.time_since_creation += delta_time;

        // Get last segment
        if (crack.segments.empty()) {
            crack.is_active = false;
            continue;
        }

        CrackSegment& last_segment = crack.segments.back();

        // Propagate crack
        float propagation_distance = crack.propagation_speed * delta_time;

        // Calculate new direction
        math::Vec3 current_dir = crack.direction;
        math::Vec3 new_dir = calculate_crack_direction(damage, last_segment.end_position, current_dir);

        // Create new segment
        CrackSegment new_segment;
        new_segment.start_position = last_segment.end_position;
        new_segment.end_position = {
            new_segment.start_position.x + new_dir.x * propagation_distance,
            new_segment.start_position.y + new_dir.y * propagation_distance,
            new_segment.start_position.z + new_dir.z * propagation_distance
        };

        // Width increases with crack length and damage state
        float state_width_multiplier = static_cast<float>(damage.state) / 6.0f;  // 0.0-1.0
        new_segment.width = std::min(
            config_.max_crack_width,
            0.001f + crack.total_length * 0.01f + state_width_multiplier * 0.02f
        );

        new_segment.depth = 0.5f + state_width_multiplier * 0.5f;
        new_segment.generation = last_segment.generation;

        crack.segments.push_back(new_segment);
        crack.total_length += propagation_distance;
        crack.direction = new_dir;

        // Check for crack termination
        if (crack.total_length > 1.5f ||  // Max length
            crack.segments.size() > 50)   // Max segments
        {
            crack.is_active = false;
            LOG_DEBUG(Game, "Crack terminated: length={:.2f}m, segments={}",
                     crack.total_length, crack.segments.size());
        }

        // Check for branching
        if (should_crack_branch(crack, damage.last_impact_force)) {
            // Create branch crack
            math::Vec3 branch_dir = {
                new_dir.y - new_dir.z,
                new_dir.z - new_dir.x,
                new_dir.x - new_dir.y
            };

            float branch_len = std::sqrt(branch_dir.x * branch_dir.x +
                                        branch_dir.y * branch_dir.y +
                                        branch_dir.z * branch_dir.z);
            if (branch_len > 1e-6f) {
                branch_dir.x /= branch_len;
                branch_dir.y /= branch_len;
                branch_dir.z /= branch_len;

                initiate_crack(damage, new_segment.start_position, branch_dir,
                              damage.last_impact_force * 0.5f);

                LOG_DEBUG(Game, "Crack branched at {:.2f}m", crack.total_length);
            }
        }
    }
}

// Apply impact damage
void TileDamageSystem::apply_impact_damage(
    TileDamage& damage,
    const math::Vec3& impact_position,
    const math::Vec3& impact_direction,
    float impact_force,
    ImpactType impact_type)
{
    // Calculate damage amount
    float damage_amount = calculate_impact_damage(impact_force, impact_type);
    damage.health = std::max(0.0f, damage.health - damage_amount);
    damage.accumulated_damage += damage_amount;

    // Record impact
    damage.last_impact_position = impact_position;
    damage.last_impact_direction = impact_direction;
    damage.last_impact_force = impact_force;
    damage.time_since_last_impact = 0.0f;

    // Update stress direction
    damage.primary_stress_direction = impact_direction;
    damage.stress_magnitude = impact_force;

    LOG_DEBUG(Game, "Impact damage: {:.1f}% (health now: {:.1f}%)",
             damage_amount, damage.health);

    // Update state
    [[maybe_unused]] bool state_changed = update_damage_state(damage);

    // Initiate cracks for Cracked state and beyond
    if (damage.state >= TileState::Cracked) {
        // Number of cracks based on damage state and impact force
        int num_cracks = 1;
        if (damage.state >= TileState::Damaged) num_cracks = 2;
        if (damage.state >= TileState::Failing) num_cracks = 3;

        for (int i = 0; i < num_cracks; ++i) {
            // Vary crack direction slightly
            static std::mt19937 rng(static_cast<uint32_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()
            ));
            std::uniform_real_distribution<float> dist(-0.52f, 0.52f);  // ±30 degrees

            float angle_offset = dist(rng);
            math::Vec3 crack_dir = {
                impact_direction.x * std::cos(angle_offset) - impact_direction.y * std::sin(angle_offset),
                impact_direction.x * std::sin(angle_offset) + impact_direction.y * std::cos(angle_offset),
                impact_direction.z
            };

            // Normalize
            float len = std::sqrt(crack_dir.x * crack_dir.x +
                                crack_dir.y * crack_dir.y +
                                crack_dir.z * crack_dir.z);
            if (len > 1e-6f) {
                crack_dir.x /= len;
                crack_dir.y /= len;
                crack_dir.z /= len;
            }

            initiate_crack(damage, impact_position, crack_dir, impact_force);
        }
    }
}

// Apply continuous stress damage
void TileDamageSystem::apply_stress_damage(
    TileDamage& damage,
    const math::Vec3& stress_direction,
    float stress_magnitude,
    float delta_time)
{
    // Update stress tracking
    damage.primary_stress_direction = stress_direction;
    damage.stress_magnitude = stress_magnitude;

    // Calculate damage from stress (higher stress = more damage)
    // Normalize to ~1% per second at 10000 Pa
    float damage_amount = (stress_magnitude / 10000.0f) * config_.stress_damage_rate * delta_time;

    damage.health = std::max(0.0f, damage.health - damage_amount);
    damage.accumulated_damage += damage_amount;

    // Update state
    update_damage_state(damage);
}

// Get warning shake offset
math::Vec3 TileDamageSystem::get_warning_shake_offset(const TileDamage& damage, float current_time) const {
    if (damage.state != TileState::Critical) {
        return {0, 0, 0};
    }

    // Shake intensity increases as collapse approaches
    float time_factor = 1.0f - (damage.time_until_collapse / config_.critical_state_duration);
    float intensity = config_.warning_shake_amplitude * time_factor;

    // Random shake using sine waves at different frequencies
    float shake_x = intensity * std::sin(current_time * 12.0f + 0.0f);
    float shake_y = intensity * std::sin(current_time * 15.0f + 1.57f);
    float shake_z = intensity * std::sin(current_time * 18.0f + 3.14f);

    return {shake_x, shake_y, shake_z};
}

// Update system
void TileDamageSystem::update(std::vector<TileDamage>& damage_data, float delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    stats_ = Statistics{};  // Reset stats

    for (auto& damage : damage_data) {
        damage.time_in_current_state += delta_time;
        damage.time_since_last_impact += delta_time;

        // Update Critical state countdown
        if (damage.state == TileState::Critical) {
            damage.time_until_collapse -= delta_time;
            stats_.tiles_in_critical_state++;

            if (damage.time_until_collapse <= 0.0f) {
                update_damage_state(damage);  // Transition to Collapsed
            }
        }

        // Propagate active cracks
        if (!damage.crack_paths.empty()) {
            propagate_cracks(damage, delta_time);

            stats_.tiles_with_cracks++;
            for (const auto& crack : damage.crack_paths) {
                if (crack.is_active) {
                    stats_.active_crack_paths++;
                }
                stats_.total_crack_segments += static_cast<uint32_t>(crack.segments.size());
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.update_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
}

} // namespace lore::world
