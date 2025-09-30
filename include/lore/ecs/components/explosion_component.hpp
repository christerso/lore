#pragma once

#include "lore/math/vec3.hpp"
#include "lore/math/vec4.hpp"
#include <cstdint>
#include <vector>

namespace lore::ecs {

/**
 * @brief Explosion simulation with shockwave, blast damage, and volumetric effects
 *
 * Explosions are fundamentally different from continuous fires:
 * - Rapid pressure wave expansion
 * - Impulse-based damage (not continuous)
 * - Brief duration (0.1-2 seconds)
 * - Shockwave physics with overpressure
 * - Debris/fragmentation
 * - Flash + blast visual effects
 *
 * Types of explosions:
 * - Chemical (TNT, C4, gunpowder)
 * - Fuel-air (gasoline, propane)
 * - Nuclear (fireball + shockwave + radiation)
 * - Magic (custom colors, no physics limits)
 *
 * Integration:
 * - VolumetricFireComponent: Fireball visualization
 * - ThermalSystem: Heat flash
 * - StructuralSystem: Blast damage to buildings
 * - AnatomySystem: Shockwave trauma + burns
 *
 * Example usage:
 * @code
 * // Create grenade explosion
 * Entity grenade = world.create_entity();
 * world.add_component(grenade, ExplosionComponent::create_grenade_explosion());
 *
 * // Detonate!
 * explosion_system.trigger(world, grenade, explosion_position);
 *
 * // System processes:
 * // 1. Shockwave expansion (0-50ms)
 * // 2. Fireball formation (0-200ms)
 * // 3. Debris ejection (0-500ms)
 * // 4. Smoke plume (200ms-10s)
 * @endcode
 */
struct ExplosionComponent {
    /**
     * @brief Explosion type determines physics behavior
     */
    enum class ExplosionType {
        Chemical,    ///< TNT, C4, gunpowder (standard)
        FuelAir,     ///< Gasoline, propane (larger fireball, slower)
        Nuclear,     ///< Fireball + radiation + EMP
        Magic,       ///< Custom physics, any color
        Directed,    ///< Shaped charge (directional blast)
    };

    // Explosion state
    bool is_active = false;                  ///< Currently detonating?
    float detonation_time = 0.0f;           ///< When explosion started (seconds)
    float elapsed_time = 0.0f;              ///< Time since detonation (seconds)
    float total_duration_s = 2.0f;          ///< Total explosion duration

    // Explosion properties
    ExplosionType type = ExplosionType::Chemical;
    float tnt_equivalent_kg = 0.5f;         ///< TNT mass equivalent (kg)
    math::Vec3 epicenter{0.0f, 0.0f, 0.0f}; ///< Explosion origin (world space)

    // Shockwave parameters
    float shockwave_speed_m_s = 6000.0f;    ///< Initial shockwave velocity (Mach 17)
    float shockwave_radius_m = 0.0f;        ///< Current shockwave radius
    float max_blast_radius_m = 10.0f;       ///< Maximum damage radius
    float overpressure_peak_pa = 500000.0f; ///< Peak overpressure (500 kPa)
    float overpressure_decay = 2.0f;        ///< Pressure decay exponent

    // Fireball parameters
    float fireball_radius_m = 0.0f;         ///< Current fireball size
    float max_fireball_radius_m = 5.0f;     ///< Maximum fireball size
    float fireball_temperature_k = 3000.0f; ///< Peak temperature
    float fireball_duration_s = 0.5f;       ///< Fireball lifetime

    // Impulse and forces
    float impulse_magnitude_n_s = 10000.0f; ///< Total impulse at center
    float lift_coefficient = 0.5f;          ///< Upward force multiplier
    bool create_crater = true;              ///< Deform terrain?
    float crater_radius_m = 2.0f;           ///< Crater size
    float crater_depth_m = 0.5f;            ///< Crater depth

    // Fragmentation
    bool generate_fragments = true;         ///< Create debris/shrapnel
    uint32_t fragment_count = 50;           ///< Number of fragments
    float fragment_velocity_m_s = 100.0f;   ///< Initial fragment speed
    float fragment_mass_kg = 0.01f;         ///< Per-fragment mass

    // Visual effects
    math::Vec4 flash_color{1.0f, 0.9f, 0.7f, 1.0f}; ///< Initial flash (white-yellow)
    math::Vec4 fireball_color{1.0f, 0.3f, 0.0f, 1.0f}; ///< Fireball color (orange-red)
    math::Vec4 smoke_color{0.2f, 0.2f, 0.2f, 0.8f};    ///< Smoke color (dark gray)

    float flash_intensity = 100.0f;         ///< Initial light intensity
    float flash_duration_s = 0.05f;         ///< Flash duration (50ms)

    // Audio
    float sound_volume = 1.0f;              ///< Blast sound loudness
    float sound_distance_m = 1000.0f;       ///< Audible distance

    // Configuration
    struct Config {
        // === Shockwave ===
        float shockwave_speed_multiplier = 1.0f; ///< Global speed scale
        float overpressure_multiplier = 1.0f;    ///< Damage multiplier
        bool enable_terrain_deformation = true;  ///< Crater generation
        bool enable_shockwave_reflections = true; ///< Bounce off walls

        // === Fireball ===
        float fireball_size_multiplier = 1.0f;   ///< Size scale
        float fireball_temp_multiplier = 1.0f;   ///< Temperature scale
        bool use_volumetric_fireball = true;     ///< GPU volumetric vs sprite

        // === Fragmentation ===
        bool enable_fragmentation = true;        ///< Create debris
        float fragment_damage_multiplier = 1.0f; ///< Shrapnel damage
        bool fragments_cause_fire = false;       ///< Hot fragments ignite
        uint32_t max_fragments_per_explosion = 200; ///< Fragment budget

        // === Smoke ===
        bool generate_smoke_plume = true;        ///< Create smoke after blast
        float smoke_duration_s = 10.0f;          ///< Smoke lifetime
        float smoke_rise_speed_m_s = 2.0f;       ///< Upward smoke velocity

        // === Performance ===
        uint32_t max_active_explosions = 10;     ///< Simultaneous explosions
        float lod_distance_high_m = 50.0f;       ///< Full sim distance
        float lod_distance_low_m = 200.0f;       ///< Min sim distance

        // === Debug ===
        bool visualize_shockwave = false;        ///< Draw shockwave sphere
        bool visualize_blast_radius = false;     ///< Draw damage zones
        bool log_explosions = true;              ///< Print explosion events
    } config;

    /**
     * @brief Create grenade explosion
     */
    static ExplosionComponent create_grenade_explosion() noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::Chemical;
        explosion.tnt_equivalent_kg = 0.2f;         // 200g TNT
        explosion.max_blast_radius_m = 10.0f;       // 10m damage
        explosion.max_fireball_radius_m = 2.0f;
        explosion.fireball_duration_s = 0.3f;
        explosion.total_duration_s = 1.0f;
        explosion.fragment_count = 100;             // Grenade fragments
        explosion.fragment_velocity_m_s = 150.0f;
        explosion.create_crater = false;            // Too small for crater
        return explosion;
    }

    /**
     * @brief Create C4 explosion
     */
    static ExplosionComponent create_c4_explosion() noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::Chemical;
        explosion.tnt_equivalent_kg = 1.0f;         // 1kg TNT equivalent
        explosion.max_blast_radius_m = 20.0f;
        explosion.max_fireball_radius_m = 5.0f;
        explosion.fireball_duration_s = 0.5f;
        explosion.total_duration_s = 2.0f;
        explosion.overpressure_peak_pa = 1000000.0f; // 1 MPa
        explosion.create_crater = true;
        explosion.crater_radius_m = 3.0f;
        explosion.crater_depth_m = 1.0f;
        return explosion;
    }

    /**
     * @brief Create fuel-air explosion (gasoline tank)
     */
    static ExplosionComponent create_fuel_air_explosion() noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::FuelAir;
        explosion.tnt_equivalent_kg = 2.0f;
        explosion.max_blast_radius_m = 30.0f;       // Larger area
        explosion.max_fireball_radius_m = 10.0f;    // Big fireball
        explosion.fireball_duration_s = 1.0f;       // Slower burn
        explosion.total_duration_s = 3.0f;
        explosion.shockwave_speed_m_s = 3000.0f;    // Slower than TNT
        explosion.fireball_temperature_k = 2000.0f; // Lower temp
        explosion.generate_fragments = false;       // No shrapnel
        explosion.config.fragments_cause_fire = false; // Already fire
        return explosion;
    }

    /**
     * @brief Create nuclear explosion
     */
    static ExplosionComponent create_nuclear_explosion() noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::Nuclear;
        explosion.tnt_equivalent_kg = 1000000.0f;   // 1000 tons TNT (1 kiloton)
        explosion.max_blast_radius_m = 5000.0f;     // 5km blast
        explosion.max_fireball_radius_m = 500.0f;   // 500m fireball
        explosion.fireball_duration_s = 5.0f;
        explosion.total_duration_s = 30.0f;
        explosion.fireball_temperature_k = 10000000.0f; // 10 million K
        explosion.flash_intensity = 10000.0f;       // Blinding flash
        explosion.flash_duration_s = 0.1f;
        explosion.create_crater = true;
        explosion.crater_radius_m = 100.0f;
        explosion.crater_depth_m = 20.0f;
        explosion.config.generate_smoke_plume = true;
        explosion.config.smoke_duration_s = 600.0f; // 10 minute mushroom cloud
        return explosion;
    }

    /**
     * @brief Create magic explosion (customizable colors)
     */
    static ExplosionComponent create_magic_explosion(
        const math::Vec4& color = {0.5f, 0.0f, 1.0f, 1.0f}, // Purple
        float radius_m = 5.0f
    ) noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::Magic;
        explosion.tnt_equivalent_kg = 0.5f;
        explosion.max_blast_radius_m = radius_m;
        explosion.max_fireball_radius_m = radius_m * 0.5f;
        explosion.fireball_duration_s = 0.8f;
        explosion.total_duration_s = 2.0f;

        // Custom colors
        explosion.flash_color = color * 2.0f;       // Bright flash
        explosion.fireball_color = color;
        explosion.smoke_color = color * 0.3f;       // Tinted smoke

        explosion.generate_fragments = false;       // Magic doesn't have shrapnel
        explosion.create_crater = false;            // Or craters
        return explosion;
    }

    /**
     * @brief Create directed blast (shaped charge)
     */
    static ExplosionComponent create_shaped_charge(
        const math::Vec3& direction = {1.0f, 0.0f, 0.0f}
    ) noexcept {
        ExplosionComponent explosion;
        explosion.type = ExplosionType::Directed;
        explosion.tnt_equivalent_kg = 0.5f;
        explosion.max_blast_radius_m = 15.0f;       // Long range
        explosion.max_fireball_radius_m = 2.0f;     // Small fireball
        explosion.fireball_duration_s = 0.2f;
        explosion.total_duration_s = 0.5f;

        // Most force in one direction
        explosion.lift_coefficient = 0.0f;          // No upward lift
        // TODO: Store direction vector for asymmetric blast

        explosion.fragment_count = 200;             // Focused fragments
        explosion.fragment_velocity_m_s = 300.0f;   // Very fast
        return explosion;
    }

    /**
     * @brief Calculate shockwave overpressure at distance
     *
     * @param distance_m Distance from epicenter (meters)
     * @return Overpressure in Pascals
     */
    float calculate_overpressure(float distance_m) const noexcept {
        if (distance_m >= max_blast_radius_m) {
            return 0.0f;
        }

        // Inverse power law with decay
        float normalized_distance = distance_m / max_blast_radius_m;
        float pressure = overpressure_peak_pa * std::pow(1.0f - normalized_distance, overpressure_decay);
        return pressure * config.overpressure_multiplier;
    }

    /**
     * @brief Calculate impulse force at distance
     *
     * @param distance_m Distance from epicenter (meters)
     * @return Impulse in Newton-seconds
     */
    float calculate_impulse(float distance_m) const noexcept {
        if (distance_m >= max_blast_radius_m) {
            return 0.0f;
        }

        float normalized_distance = distance_m / max_blast_radius_m;
        return impulse_magnitude_n_s * (1.0f - normalized_distance);
    }

    /**
     * @brief Check if shockwave has reached distance
     *
     * @param distance_m Distance from epicenter (meters)
     * @return true if shockwave has arrived
     */
    bool has_shockwave_reached(float distance_m) const noexcept {
        return shockwave_radius_m >= distance_m;
    }

    /**
     * @brief Check if still in fireball phase
     */
    bool is_fireball_active() const noexcept {
        return is_active && elapsed_time < fireball_duration_s;
    }

    /**
     * @brief Check if still producing smoke
     */
    bool is_smoking() const noexcept {
        return is_active && elapsed_time > fireball_duration_s;
    }
};

} // namespace lore::ecs