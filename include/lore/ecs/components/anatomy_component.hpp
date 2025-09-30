#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <unordered_map>
#include <string>

namespace lore::ecs {

/**
 * @brief Organ-based health system for biological entities
 *
 * NO HITPOINTS! Health is determined by organ function.
 * - Organs have function levels (0.0-1.0)
 * - Zero function = organ failure
 * - Critical organ failure = death
 * - Damaged organs = reduced capabilities
 *
 * Simple but realistic:
 * - Brain damage = death
 * - Heart damage = death
 * - Lung damage = reduced stamina
 * - Limb damage = reduced mobility/actions
 * - Blood loss = death if severe
 *
 * NOT over-engineered: No complex organ interdependencies,
 * no cell-level simulation, just functional organs.
 */
struct AnatomyComponent {
    /// Body regions for hit location
    enum class BodyPart : uint8_t {
        Head,       ///< Brain, eyes, ears
        Torso,      ///< Heart, lungs, stomach
        LeftArm,    ///< Left arm
        RightArm,   ///< Right arm
        LeftLeg,    ///< Left leg
        RightLeg    ///< Right leg
    };

    /// Individual organ state with physical properties for ballistics
    struct Organ {
        float function = 1.0f;      ///< How well it works (0.0=destroyed, 1.0=healthy)
        float bleeding = 0.0f;      ///< Blood loss rate (0.0-1.0 per second)
        bool is_critical = false;   ///< Immediate death if destroyed?

        // Physical properties for ballistics (realistic organ modeling)
        math::Vec3 position{0.0f};  ///< Position relative to body center (meters)
        float radius = 0.05f;       ///< Bounding sphere radius (meters)
        float mass_kg = 0.3f;       ///< Organ mass in kilograms

        // Energy-based damage tracking (NO HITPOINTS)
        float accumulated_energy_j = 0.0f;  ///< Total kinetic energy absorbed (Joules)
        float energy_threshold_j = 50.0f;   ///< Energy required to destroy organ

        /**
         * @brief Check if organ is functional (>30% function)
         */
        bool is_functional() const {
            return function > 0.3f;
        }

        /**
         * @brief Check if organ is destroyed
         */
        bool is_destroyed() const {
            return function <= 0.0f;
        }

        /**
         * @brief Check if organ is damaged
         */
        bool is_damaged() const {
            return function < 1.0f;
        }

        /**
         * @brief Get bounding sphere for raycasting
         */
        math::geometry::Sphere get_bounding_sphere() const {
            return math::geometry::Sphere{position, radius};
        }
    };

    // Organs with realistic positions and masses (NOT over-engineered, just realistic)
    std::unordered_map<std::string, Organ> organs = {
        {"brain", {
            1.0f, 0.0f, true,                      // function, bleeding, critical
            math::Vec3{0.0f, 1.6f, 0.0f},          // 1.6m above center (head position)
            0.08f, 1.4f,                           // 8cm radius, 1.4kg mass
            0.0f, 30.0f                            // 0 accumulated energy, 30J threshold (brain is fragile)
        }},
        {"heart", {
            1.0f, 0.0f, true,
            math::Vec3{-0.05f, 1.2f, 0.1f},        // Left of center, chest height, slightly forward
            0.06f, 0.3f,                           // 6cm radius, 300g mass
            0.0f, 30.0f                            // 30J threshold (heart is fragile)
        }},
        {"lungs", {
            1.0f, 0.0f, false,
            math::Vec3{0.0f, 1.2f, 0.0f},          // Center chest
            0.15f, 1.1f,                           // 15cm radius (large organ), 1.1kg total
            0.0f, 50.0f                            // 50J threshold
        }},
        {"stomach", {
            1.0f, 0.0f, false,
            math::Vec3{0.0f, 1.0f, 0.1f},          // Below chest, slightly forward
            0.08f, 0.4f,                           // 8cm radius, 400g mass
            0.0f, 40.0f                            // 40J threshold
        }},
        {"liver", {
            1.0f, 0.0f, false,
            math::Vec3{0.1f, 1.0f, 0.05f},         // Right side, below chest
            0.09f, 1.5f,                           // 9cm radius, 1.5kg mass (largest organ)
            0.0f, 50.0f                            // 50J threshold
        }},
        {"left_arm", {
            1.0f, 0.0f, false,
            math::Vec3{-0.3f, 1.2f, 0.0f},         // Left side, shoulder height
            0.05f, 3.0f,                           // 5cm radius (bones/muscles), 3kg mass
            0.0f, 100.0f                           // 100J threshold (limbs are tougher)
        }},
        {"right_arm", {
            1.0f, 0.0f, false,
            math::Vec3{0.3f, 1.2f, 0.0f},          // Right side, shoulder height
            0.05f, 3.0f,
            0.0f, 100.0f
        }},
        {"left_leg", {
            1.0f, 0.0f, false,
            math::Vec3{-0.15f, 0.5f, 0.0f},        // Left side, mid-leg height
            0.06f, 10.0f,                          // 6cm radius, 10kg mass (legs are heavy)
            0.0f, 150.0f                           // 150J threshold (legs are very tough)
        }},
        {"right_leg", {
            1.0f, 0.0f, false,
            math::Vec3{0.15f, 0.5f, 0.0f},         // Right side, mid-leg height
            0.06f, 10.0f,
            0.0f, 150.0f
        }}
    };

    // Blood volume (separate from organs)
    float blood_volume = 1.0f;      ///< 1.0 = full, 0.0 = exsanguinated

    // Pain and shock
    float pain_level = 0.0f;        ///< 0.0-1.0 (affects accuracy, movement)
    float shock_level = 0.0f;       ///< 0.0-1.0 (can cause unconsciousness)

    /**
     * @brief Check if entity is alive (NO HITPOINTS!)
     *
     * Death occurs from:
     * - Critical organ failure (brain, heart)
     * - Severe blood loss (<20% blood volume)
     * - Extreme shock (>80% shock level)
     */
    bool is_alive() const {
        // Dead if critical organs destroyed
        if (organs.at("brain").is_destroyed()) return false;
        if (organs.at("heart").is_destroyed()) return false;

        // Dead if blood loss too severe
        if (blood_volume <= 0.2f) return false;

        // Dead if in extreme shock
        if (shock_level >= 0.8f) return false;

        return true;
    }

    /**
     * @brief Check if entity is conscious
     *
     * Unconscious from:
     * - Brain damage
     * - Severe pain
     * - Shock
     * - Blood loss
     */
    bool is_conscious() const {
        if (!is_alive()) return false;

        // Unconscious if brain damaged
        if (organs.at("brain").function < 0.5f) return false;

        // Unconscious from pain/shock
        if (pain_level >= 0.7f) return false;
        if (shock_level >= 0.6f) return false;

        // Unconscious from blood loss
        if (blood_volume < 0.4f) return false;

        return true;
    }

    /**
     * @brief Check if entity can walk
     */
    bool can_walk() const {
        float leg_function = (organs.at("left_leg").function +
                             organs.at("right_leg").function) / 2.0f;
        return leg_function > 0.3f; // Need at least one functional leg
    }

    /**
     * @brief Get movement speed multiplier (based on leg function)
     */
    float get_move_speed_multiplier() const {
        if (!can_walk()) return 0.0f;

        float leg_function = (organs.at("left_leg").function +
                             organs.at("right_leg").function) / 2.0f;

        // Pain reduces movement
        float pain_penalty = 1.0f - (pain_level * 0.5f);

        return leg_function * pain_penalty;
    }

    /**
     * @brief Check if entity can use two-handed items
     */
    bool can_use_two_handed() const {
        return organs.at("left_arm").is_functional() &&
               organs.at("right_arm").is_functional();
    }

    /**
     * @brief Check if entity can use left arm
     */
    bool can_use_left_arm() const {
        return organs.at("left_arm").is_functional();
    }

    /**
     * @brief Check if entity can use right arm
     */
    bool can_use_right_arm() const {
        return organs.at("right_arm").is_functional();
    }

    /**
     * @brief Get stamina multiplier (based on lung function)
     */
    float get_stamina_multiplier() const {
        return organs.at("lungs").function;
    }

    /**
     * @brief Get perception multiplier (based on pain, consciousness)
     */
    float get_perception_multiplier() const {
        if (!is_conscious()) return 0.0f;

        // Pain reduces perception
        float pain_penalty = 1.0f - (pain_level * 0.3f);

        // Shock reduces perception
        float shock_penalty = 1.0f - (shock_level * 0.2f);

        return pain_penalty * shock_penalty;
    }

    /**
     * @brief Apply damage to a body part
     *
     * @param part Body part hit
     * @param damage Damage amount (0.0-1.0 scale)
     */
    void take_damage(BodyPart part, float damage) {
        // Map body part to organs
        switch (part) {
            case BodyPart::Head:
                apply_organ_damage("brain", damage);
                break;

            case BodyPart::Torso:
                // Torso hits can damage heart or lungs
                if (damage > 0.3f) {
                    apply_organ_damage("heart", damage * 0.5f);
                }
                apply_organ_damage("lungs", damage * 0.7f);
                break;

            case BodyPart::LeftArm:
                apply_organ_damage("left_arm", damage);
                break;

            case BodyPart::RightArm:
                apply_organ_damage("right_arm", damage);
                break;

            case BodyPart::LeftLeg:
                apply_organ_damage("left_leg", damage);
                break;

            case BodyPart::RightLeg:
                apply_organ_damage("right_leg", damage);
                break;
        }

        // Increase pain and shock
        pain_level += damage * 0.3f;
        shock_level += damage * 0.2f;

        // Clamp values
        pain_level = std::clamp(pain_level, 0.0f, 1.0f);
        shock_level = std::clamp(shock_level, 0.0f, 1.0f);
    }

    /**
     * @brief Apply damage to specific organ
     *
     * Legacy method for simple damage (0-1 scale)
     */
    void apply_organ_damage(const char* organ_name, float damage) {
        auto it = organs.find(organ_name);
        if (it == organs.end()) return;

        Organ& organ = it->second;

        // Reduce function
        organ.function -= damage;
        organ.function = std::clamp(organ.function, 0.0f, 1.0f);

        // Increase bleeding
        organ.bleeding += damage * 0.1f;
        organ.bleeding = std::clamp(organ.bleeding, 0.0f, 1.0f);
    }

    /**
     * @brief Apply energy-based damage to organ (physics-based)
     *
     * Uses kinetic energy transfer from projectile impact.
     * Organ function degrades based on accumulated energy vs threshold.
     *
     * @param organ_name Organ that was hit
     * @param energy_joules Kinetic energy transferred (from BiologicalPropertiesComponent)
     */
    void apply_energy_damage(const char* organ_name, float energy_joules) {
        auto it = organs.find(organ_name);
        if (it == organs.end()) return;

        Organ& organ = it->second;

        // Accumulate energy
        organ.accumulated_energy_j += energy_joules;

        // Function degrades based on energy ratio
        // ~50 Joules destroys soft tissue organs (heart, brain)
        // ~100-150 Joules required for limbs/bones
        float damage_ratio = organ.accumulated_energy_j / organ.energy_threshold_j;
        organ.function = std::clamp(1.0f - damage_ratio, 0.0f, 1.0f);

        // Bleeding increases with energy (vascular damage)
        // Critical organs bleed more (heart, liver, lungs are highly vascular)
        float bleeding_factor = organ.is_critical ? 0.002f : 0.0005f;
        organ.bleeding += energy_joules * bleeding_factor;
        organ.bleeding = std::clamp(organ.bleeding, 0.0f, 1.0f);

        // Increase pain and shock based on energy
        const float energy_to_pain = 0.01f;  // 100J = 1.0 pain
        pain_level += energy_joules * energy_to_pain;
        pain_level = std::clamp(pain_level, 0.0f, 1.0f);

        const float energy_to_shock = 0.005f; // 200J = 1.0 shock
        shock_level += energy_joules * energy_to_shock;
        shock_level = std::clamp(shock_level, 0.0f, 1.0f);
    }

    /**
     * @brief Check if projectile trajectory hits organs (ballistics raycast)
     *
     * Uses ray-sphere intersection against organ bounding spheres.
     * Returns list of organs hit in order along trajectory.
     *
     * This is where ballistics meets anatomy!
     *
     * @param entry_point Projectile entry point (world space)
     * @param direction Projectile direction (normalized)
     * @param penetration_depth Maximum penetration depth (meters)
     * @param entity_transform Entity's transform component for world-to-local conversion
     * @return Vector of organ names hit by trajectory
     *
     * @example
     * @code
     * auto& anatomy = entity.get<AnatomyComponent>();
     * auto& transform = entity.get<TransformComponent>();
     *
     * // Bullet entry point and direction
     * math::Vec3 entry = hit_position;
     * math::Vec3 dir = math::normalize(projectile_velocity);
     * float pen = 0.15f; // 15cm penetration
     *
     * auto hit_organs = anatomy.check_trajectory_hits(entry, dir, pen, transform);
     * // Result: ["lungs", "heart"] if trajectory passes through both
     * @endcode
     */
    std::vector<std::string> check_trajectory_hits(
        const math::Vec3& entry_point,
        const math::Vec3& direction,
        float penetration_depth,
        const math::Vec3& entity_position
    ) const {
        std::vector<std::string> hit_organs;

        // Convert entry point to local space (relative to entity)
        math::Vec3 local_entry = entry_point - entity_position;

        // Ray in local space
        math::geometry::Ray trajectory{local_entry, direction};

        // Check each organ
        for (const auto& [name, organ] : organs) {
            // Get organ bounding sphere
            math::geometry::Sphere organ_sphere = organ.get_bounding_sphere();

            // Check ray-sphere intersection
            float t_near, t_far;
            if (math::geometry::intersect_ray_sphere(trajectory, organ_sphere, t_near, t_far)) {
                // Check if penetration reaches this organ
                float distance_to_organ = t_near;

                if (distance_to_organ >= 0.0f && distance_to_organ <= penetration_depth) {
                    hit_organs.push_back(name);
                }
            }
        }

        // Sort by distance from entry point (closer organs first)
        std::sort(hit_organs.begin(), hit_organs.end(),
            [this, &local_entry](const std::string& a, const std::string& b) {
                float dist_a = math::length(organs.at(a).position - local_entry);
                float dist_b = math::length(organs.at(b).position - local_entry);
                return dist_a < dist_b;
            });

        return hit_organs;
    }

    /**
     * @brief Get organ by name (safe accessor)
     */
    const Organ* get_organ(const char* organ_name) const {
        auto it = organs.find(organ_name);
        return (it != organs.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Get organ by name (mutable accessor)
     */
    Organ* get_organ_mut(const char* organ_name) {
        auto it = organs.find(organ_name);
        return (it != organs.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Update anatomy per frame (bleeding, shock recovery)
     *
     * @param delta_time Time since last update (seconds)
     */
    void update(float delta_time) {
        // Process bleeding
        float total_bleeding = 0.0f;
        for (auto& [name, organ] : organs) {
            total_bleeding += organ.bleeding;

            // Reduce bleeding over time (clotting)
            organ.bleeding -= delta_time * 0.05f;
            organ.bleeding = std::max(0.0f, organ.bleeding);
        }

        // Blood loss from bleeding
        blood_volume -= total_bleeding * delta_time * 0.1f;
        blood_volume = std::clamp(blood_volume, 0.0f, 1.0f);

        // Pain fades over time (slowly)
        pain_level -= delta_time * 0.02f;
        pain_level = std::max(0.0f, pain_level);

        // Shock recovery (faster than pain)
        shock_level -= delta_time * 0.05f;
        shock_level = std::max(0.0f, shock_level);
    }

    /**
     * @brief Create healthy human anatomy
     */
    static AnatomyComponent create_human() {
        return AnatomyComponent{}; // Defaults are healthy human
    }
};

} // namespace lore::ecs