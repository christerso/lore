#include "lore/ecs/systems/combustion_system.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>

namespace lore::ecs {

namespace {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> uniform_dist(0.0f, 1.0f);
}

CombustionSystem::CombustionSystem() {
    config_ = Config{};
}

void CombustionSystem::initialize(const Config& config) {
    config_ = config;
    accumulated_time_ = 0.0f;
    spread_check_timer_ = 0.0f;
    stats_ = Stats{};
}

void CombustionSystem::update(World& world, float delta_time_s) {
    if (delta_time_s <= 0.0f) return;

    // Accumulate time for fixed timestep
    accumulated_time_ += delta_time_s;
    spread_check_timer_ += delta_time_s;
    const float fixed_dt = 1.0f / config_.update_rate_hz;

    if (accumulated_time_ < fixed_dt) {
        return;
    }

    // Reset per-frame stats
    stats_.fires_active = 0;
    stats_.fires_started = 0;
    stats_.fires_extinguished = 0;
    stats_.spread_attempts = 0;
    stats_.successful_spreads = 0;
    stats_.total_heat_generated_j = 0.0f;
    stats_.total_oxygen_consumed_mol = 0.0f;
    current_smoke_particles_ = 0;
    current_ember_particles_ = 0;

    // Build spatial grid
    if (config_.use_spatial_partitioning) {
        build_spatial_grid(world);
    }

    // Check fire spread periodically
    bool do_spread_check = spread_check_timer_ >= config_.spread_check_interval_s;
    if (do_spread_check) {
        spread_check_timer_ = 0.0f;
    }

    // Process all burning entities
    std::vector<Entity> to_remove;  // Fires to extinguish

    world.view<CombustionComponent>([&](Entity entity, CombustionComponent& combustion) {
        if (!combustion.is_active) {
            to_remove.push_back(entity);
            return;
        }

        stats_.fires_active++;

        // Get position (TODO: from transform)
        math::Vec3 position{0.0f, 0.0f, 0.0f};

        // Process this fire
        process_fire(world, entity, position, combustion, fixed_dt);

        // Check for fire spread
        if (config_.enable_fire_spread && do_spread_check) {
            check_fire_spread(world, entity, position, combustion);
        }

        // Check if fire should be extinguished
        if (!combustion.is_active || combustion.fuel_remaining_kg <= 0.0f) {
            to_remove.push_back(entity);
        }
    });

    // Remove extinguished fires
    for (Entity entity : to_remove) {
        auto* combustion = world.get_component<CombustionComponent>(entity);
        if (combustion && config_.log_extinguishments) {
            std::cout << "[CombustionSystem] Fire extinguished on entity "
                      << static_cast<uint32_t>(entity)
                      << " (fuel: " << combustion->fuel_remaining_kg << "kg, "
                      << "temp: " << combustion->flame_temperature_k << "K)\n";
        }

        world.remove_component<CombustionComponent>(entity);
        stats_.fires_extinguished++;
    }

    accumulated_time_ -= fixed_dt;
}

bool CombustionSystem::ignite(
    World& world,
    Entity entity,
    float ignition_temp_k
) {
    // Already burning?
    if (world.has_component<CombustionComponent>(entity)) {
        return false;
    }

    // Check if entity has thermal properties
    auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    if (!thermal) {
        return false;
    }

    // Check if entity has combustible chemical composition
    auto* chemical = world.get_component<ChemicalCompositionComponent>(entity);
    if (!chemical || !chemical->is_combustible) {
        return false;
    }

    // Create combustion component
    CombustionComponent combustion = CombustionComponent::ignite(
        ignition_temp_k,
        thermal->mass_kg
    );

    // Configure combustion from chemical properties
    combustion.oxygen_consumption_mol_s = chemical->calculate_oxygen_consumption(
        combustion.fuel_consumption_rate_kg_s
    );
    combustion.config.smoke_particle_spawn_rate = chemical->config.smoke_density_factor * 10.0f;
    combustion.config.soot_production_rate = chemical->config.soot_production_rate;
    combustion.config.combustion_efficiency = chemical->combustion_efficiency;

    world.add_component(entity, combustion);

    // Update thermal temperature
    thermal->current_temperature_k = std::max(
        thermal->current_temperature_k,
        ignition_temp_k
    );

    if (config_.log_ignitions) {
        std::cout << "[CombustionSystem] Ignited entity " << static_cast<uint32_t>(entity)
                  << " at " << ignition_temp_k << "K\n";
    }

    stats_.fires_started++;
    return true;
}

bool CombustionSystem::apply_suppression(
    World& world,
    Entity entity,
    float suppression_amount_kg,
    float effectiveness
) {
    auto* combustion = world.get_component<CombustionComponent>(entity);
    if (!combustion || !combustion->is_active) {
        return true;  // Already extinguished
    }

    return combustion->apply_suppression(suppression_amount_kg, effectiveness);
}

void CombustionSystem::extinguish(World& world, Entity entity) {
    auto* combustion = world.get_component<CombustionComponent>(entity);
    if (combustion) {
        combustion->is_active = false;
    }
}

bool CombustionSystem::is_burning(const World& world, Entity entity) const {
    const auto* combustion = world.get_component<CombustionComponent>(entity);
    return combustion && combustion->is_active;
}

float CombustionSystem::get_flame_temperature(const World& world, Entity entity) const {
    const auto* combustion = world.get_component<CombustionComponent>(entity);
    return combustion ? combustion->flame_temperature_k : 0.0f;
}

void CombustionSystem::build_spatial_grid(World& world) {
    spatial_grid_.clear();

    world.view<CombustionComponent>([&](Entity entity, const CombustionComponent& combustion) {
        if (!combustion.is_active) return;

        // TODO: Get actual position
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        int64_t key = get_spatial_key(position);
        spatial_grid_[key].entities.push_back(entity);
    });
}

int64_t CombustionSystem::get_spatial_key(const math::Vec3& position) const {
    int32_t x = static_cast<int32_t>(std::floor(position.x / config_.spatial_grid_cell_size_m));
    int32_t y = static_cast<int32_t>(std::floor(position.y / config_.spatial_grid_cell_size_m));
    int32_t z = static_cast<int32_t>(std::floor(position.z / config_.spatial_grid_cell_size_m));

    int64_t key = 0;
    key |= (static_cast<int64_t>(x) & 0xFFFF) << 0;
    key |= (static_cast<int64_t>(y) & 0xFFFF) << 16;
    key |= (static_cast<int64_t>(z) & 0xFFFF) << 32;
    return key;
}

std::vector<Entity> CombustionSystem::get_nearby_entities(
    const math::Vec3& position,
    float radius_m
) const {
    std::vector<Entity> nearby;

    int32_t cell_radius = static_cast<int32_t>(std::ceil(radius_m / config_.spatial_grid_cell_size_m));

    int32_t base_x = static_cast<int32_t>(std::floor(position.x / config_.spatial_grid_cell_size_m));
    int32_t base_y = static_cast<int32_t>(std::floor(position.y / config_.spatial_grid_cell_size_m));
    int32_t base_z = static_cast<int32_t>(std::floor(position.z / config_.spatial_grid_cell_size_m));

    for (int32_t dx = -cell_radius; dx <= cell_radius; ++dx) {
        for (int32_t dy = -cell_radius; dy <= cell_radius; ++dy) {
            for (int32_t dz = -cell_radius; dz <= cell_radius; ++dz) {
                math::Vec3 cell_pos{
                    (base_x + dx) * config_.spatial_grid_cell_size_m,
                    (base_y + dy) * config_.spatial_grid_cell_size_m,
                    (base_z + dz) * config_.spatial_grid_cell_size_m
                };

                int64_t key = get_spatial_key(cell_pos);
                auto it = spatial_grid_.find(key);
                if (it != spatial_grid_.end()) {
                    nearby.insert(nearby.end(), it->second.entities.begin(), it->second.entities.end());
                }
            }
        }
    }

    return nearby;
}

void CombustionSystem::process_fire(
    World& world,
    Entity entity,
    const math::Vec3& position,
    CombustionComponent& combustion,
    float delta_time_s
) {
    // Determine LOD level
    LODLevel lod = config_.enable_lod ? get_lod_level(0.0f) : LODLevel::High;  // TODO: calculate distance

    if (lod == LODLevel::None) {
        return;  // Too far to simulate
    }

    // Update combustion state
    float oxygen_available = config_.ambient_oxygen_concentration;
    if (config_.enable_oxygen_consumption) {
        // TODO: Track local oxygen depletion
        oxygen_available *= config_.oxygen_depletion_rate;
    }

    float heat_released_j = combustion.update_combustion(
        delta_time_s,
        oxygen_available
    );

    // Apply multipliers
    heat_released_j *= config_.heat_release_multiplier;
    combustion.fuel_consumption_rate_kg_s *= config_.fuel_consumption_mult;

    // Log fuel depletion warning
    if (config_.log_fuel_depletion && combustion.fuel_remaining_kg < 1.0f) {
        std::cout << "[CombustionSystem] Entity " << static_cast<uint32_t>(entity)
                  << " low on fuel: " << combustion.fuel_remaining_kg << "kg remaining\n";
    }

    // Generate heat
    if (config_.enable_heat_generation && heat_released_j > 0.0f) {
        generate_heat(world, entity, combustion, heat_released_j);
    }

    // Spawn particles (reduced at lower LOD)
    if (config_.enable_particle_generation) {
        float particle_mult = 1.0f;
        if (lod == LODLevel::Medium) particle_mult = 0.5f;
        else if (lod == LODLevel::Low) particle_mult = 0.1f;

        spawn_particles(world, entity, position, combustion, delta_time_s * particle_mult);
    }

    // Update stats
    stats_.total_heat_generated_j += heat_released_j;
    stats_.total_oxygen_consumed_mol += combustion.oxygen_consumption_mol_s * delta_time_s;
}

void CombustionSystem::check_fire_spread(
    World& world,
    Entity entity,
    const math::Vec3& position,
    const CombustionComponent& combustion
) {
    // Get nearby entities
    float spread_radius = combustion.ignition_radius_m * config_.spread_multiplier;
    std::vector<Entity> nearby = config_.use_spatial_partitioning
        ? get_nearby_entities(position, spread_radius)
        : std::vector<Entity>{};  // TODO: fallback

    for (Entity target : nearby) {
        if (target == entity) continue;

        // Already burning?
        if (world.has_component<CombustionComponent>(target)) {
            continue;
        }

        // Check if can spread to target
        // TODO: Get actual target position
        math::Vec3 target_pos{0.0f, 0.0f, 0.0f};

        if (!can_spread_to(world, entity, target, position, target_pos)) {
            continue;
        }

        stats_.spread_attempts++;

        // Probability check
        float spread_chance = config_.ignition_probability * combustion.config.spread_probability_per_second;
        if (uniform_dist(gen) > spread_chance) {
            continue;
        }

        // Get target thermal properties
        auto* target_thermal = world.get_component<ThermalPropertiesComponent>(target);
        if (!target_thermal) continue;

        // Check if target can ignite
        float distance = (position - target_pos).length();
        if (!combustion.can_ignite_neighbor(distance, target_thermal->ignition_temperature_k)) {
            continue;
        }

        // Ignite target!
        if (ignite(world, target, combustion.flame_temperature_k * 0.8f)) {
            stats_.successful_spreads++;

            if (config_.log_ignitions) {
                std::cout << "[CombustionSystem] Fire spread from entity "
                          << static_cast<uint32_t>(entity)
                          << " to entity " << static_cast<uint32_t>(target)
                          << " (distance: " << distance << "m)\n";
            }
        }
    }
}

void CombustionSystem::generate_heat(
    World& world,
    Entity entity,
    const CombustionComponent& combustion,
    float heat_released_j
) {
    auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    if (!thermal) return;

    // Add heat to entity
    thermal->add_thermal_energy(heat_released_j, thermal->mass_kg);
}

void CombustionSystem::spawn_particles(
    World& world,
    Entity entity,
    const math::Vec3& position,
    const CombustionComponent& combustion,
    float delta_time_s
) {
    // Check particle budget
    if (current_smoke_particles_ >= config_.max_smoke_particles &&
        current_ember_particles_ >= config_.max_ember_particles) {
        return;
    }

    // Calculate particle spawn counts
    float smoke_spawn_rate = combustion.smoke_generation_rate *
                            combustion.config.smoke_particle_spawn_rate *
                            config_.particle_spawn_rate_mult;
    float ember_spawn_rate = combustion.ember_generation_rate *
                            combustion.config.ember_particle_spawn_rate *
                            config_.particle_spawn_rate_mult;

    uint32_t smoke_count = static_cast<uint32_t>(smoke_spawn_rate * delta_time_s);
    uint32_t ember_count = static_cast<uint32_t>(ember_spawn_rate * delta_time_s);

    // Clamp to budget
    smoke_count = std::min(smoke_count, config_.max_smoke_particles - current_smoke_particles_);
    ember_count = std::min(ember_count, config_.max_ember_particles - current_ember_particles_);

    // TODO: Actually spawn particles in particle system
    // For now just track counts
    current_smoke_particles_ += smoke_count;
    current_ember_particles_ += ember_count;

    stats_.smoke_particles_spawned += smoke_count;
    stats_.ember_particles_spawned += ember_count;
}

CombustionSystem::LODLevel CombustionSystem::get_lod_level(float distance_m) const {
    if (distance_m < config_.lod_distance_high_m) {
        return LODLevel::High;
    } else if (distance_m < config_.lod_distance_medium_m) {
        return LODLevel::Medium;
    } else if (distance_m < config_.lod_distance_low_m) {
        return LODLevel::Low;
    } else {
        return LODLevel::None;
    }
}

bool CombustionSystem::can_spread_to(
    World& world,
    Entity source,
    Entity target,
    const math::Vec3& source_pos,
    const math::Vec3& target_pos
) const {
    // Check distance
    float distance = (source_pos - target_pos).length();
    const auto* source_combustion = world.get_component<CombustionComponent>(source);
    if (!source_combustion || distance > source_combustion->ignition_radius_m) {
        return false;
    }

    // Check if target has thermal properties
    if (!world.has_component<ThermalPropertiesComponent>(target)) {
        return false;
    }

    // Check if target has combustible chemical composition
    const auto* chemical = world.get_component<ChemicalCompositionComponent>(target);
    if (!chemical || !chemical->is_combustible) {
        return false;
    }

    // TODO: Line of sight check if required
    if (config_.require_line_of_sight) {
        // Simplified: always true for now
        // Real implementation would raycast through world geometry
    }

    return true;
}

} // namespace lore::ecs