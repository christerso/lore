#pragma once

#include "lore/math/vec3.hpp"
#include <cstdint>

namespace lore::ecs {

/**
 * @brief Active combustion (fire) state
 *
 * Tracks an actively burning material with realistic fire behavior based on
 * chemical composition, oxygen availability, and heat transfer.
 *
 * High-performance design:
 * - Lightweight component (64 bytes)
 * - GPU-friendly data layout
 * - Extensive INI configuration for tweaking
 *
 * INI-Configurable Parameters:
 * @code{.ini}
 * [Fire.General]
 * FlameSpreadRate=0.5
 * OxygenConsumptionMultiplier=1.0
 * HeatDissipationRate=50.0
 * MinimumFlameTemperature=800.0
 * MaximumFlameTemperature=1500.0
 *
 * [Fire.Visual]
 * FlameHeightMultiplier=1.0
 * FlameWidthMultiplier=1.0
 * FlameColorIntensity=1.0
 * EmberGenerationRate=10.0
 * SmokeGenerationRate=1.0
 *
 * [Fire.Performance]
 * EnableHeatDistortion=true
 * EnableDynamicLighting=true
 * FlameUpdateRate=60
 * PropagationCheckRadius=5.0
 * @endcode
 *
 * Example usage:
 * @code
 * // Start fire on wood
 * Entity burning_log = world.create_entity();
 * world.add_component(burning_log, CombustionComponent::ignite(
 *     1000.0f,  // 1000 Kelvin ignition temp
 *     5.0f      // 5 kg of wood
 * ));
 *
 * // Update combustion each frame
 * fire_system->update(world, delta_time);
 *
 * // Check if still burning
 * auto* fire = world.get_component<CombustionComponent>(burning_log);
 * if (fire->is_active && fire->fuel_remaining_kg > 0.0f) {
 *     // Still burning
 * }
 * @endcode
 */
struct CombustionComponent {
    // Fire state
    bool is_active = false;                         ///< Is currently burning?
    float ignition_time = 0.0f;                     ///< When fire started (seconds)
    float burn_duration = 0.0f;                     ///< How long has been burning (s)

    // Fuel and combustion
    float fuel_remaining_kg = 1.0f;                 ///< Mass of burnable fuel (kg)
    float fuel_consumption_rate_kg_s = 0.01f;       ///< Burn rate (kg/s)
    float combustion_efficiency = 0.95f;            ///< How complete the burn is (0-1)

    // Temperature
    float flame_temperature_k = 1200.0f;            ///< Current flame temperature (Kelvin)
    float base_flame_temp_k = 1200.0f;             ///< Base temperature for this fuel
    float peak_flame_temp_k = 1500.0f;             ///< Maximum achievable temperature

    // Oxygen availability
    float oxygen_concentration = 0.21f;             ///< Available O2 (0-1, 0.21 = 21% air)
    float oxygen_consumption_mol_s = 0.01f;         ///< O2 consumed per second (moles)
    float stoich_air_fuel_ratio = 15.0f;           ///< Mass ratio of air:fuel needed

    // Heat release
    float heat_output_w = 1000.0f;                  ///< Power output (Watts)
    float total_energy_released_j = 0.0f;           ///< Cumulative heat (Joules)

    // Fire geometry (for rendering)
    math::Vec3 flame_center_offset{0.0f, 0.5f, 0.0f}; ///< Flame position relative to entity
    float flame_height_m = 1.0f;                    ///< Visible flame height (meters)
    float flame_radius_m = 0.5f;                    ///< Flame base radius (meters)

    // Propagation properties
    float spread_rate_m_s = 0.1f;                   ///< How fast fire spreads (m/s)
    float ignition_radius_m = 1.0f;                 ///< Distance that can ignite neighbors
    float heat_transfer_radius_m = 2.0f;            ///< Distance for radiant heat

    // Visual effects intensities (0-1)
    float smoke_generation_rate = 1.0f;             ///< Smoke particle spawn rate multiplier
    float ember_generation_rate = 1.0f;             ///< Ember particle spawn rate
    float flame_flicker_intensity = 0.3f;           ///< Random flame movement (0-1)
    float heat_distortion_strength = 1.0f;          ///< Air distortion effect strength

    // Extensive INI-configurable parameters
    struct Config {
        // === Combustion Physics ===
        float oxygen_consumption_multiplier = 1.0f;  ///< Scale O2 usage
        float fuel_consumption_multiplier = 1.0f;    ///< Scale burn rate
        float heat_release_multiplier = 1.0f;        ///< Scale energy output
        float incomplete_combustion_threshold = 0.15f; ///< O2 level for incomplete burn
        float extinguish_oxygen_threshold = 0.05f;   ///< O2 level to extinguish
        float reignition_temperature_k = 700.0f;     ///< Temp needed to reignite

        // === Fire Spread ===
        float flame_spread_rate_multiplier = 1.0f;   ///< Scale propagation speed
        float spread_probability_per_second = 0.3f;  ///< Chance to ignite neighbor per sec
        float heat_transfer_efficiency = 0.8f;       ///< How well heat radiates
        float wind_influence_factor = 1.0f;          ///< Wind effect on spread direction
        float upward_spread_bias = 2.0f;            ///< Fire climbs faster than spreads laterally
        float material_ignitability_scale = 1.0f;    ///< Global ignition difficulty

        // === Temperature Dynamics ===
        float min_flame_temperature_k = 800.0f;      ///< Minimum viable flame temp
        float max_flame_temperature_k = 1800.0f;     ///< Maximum flame temp
        float temperature_rise_rate_k_s = 100.0f;    ///< How fast flame heats up (K/s)
        float temperature_decay_rate_k_s = 50.0f;    ///< Cooling rate when fuel low
        float ambient_heat_loss_rate_w = 50.0f;      ///< Heat lost to surroundings (Watts)

        // === Visual Parameters ===
        float flame_height_multiplier = 1.0f;        ///< Scale visible flame height
        float flame_width_multiplier = 1.0f;         ///< Scale flame base radius
        float flame_color_intensity = 1.0f;          ///< Brightness multiplier
        float flame_transparency = 0.7f;             ///< Alpha blending (0=opaque, 1=transparent)
        float ember_size_multiplier = 1.0f;          ///< Scale ember particles
        float smoke_opacity_multiplier = 1.0f;       ///< Smoke darkness

        // === Particle Effects ===
        float smoke_particle_spawn_rate = 10.0f;     ///< Particles per second
        float ember_particle_spawn_rate = 5.0f;      ///< Embers per second
        float spark_particle_spawn_rate = 20.0f;     ///< Sparks per second
        float smoke_rise_speed_m_s = 1.0f;          ///< Upward velocity
        float smoke_dispersion_rate = 0.5f;          ///< Smoke spread speed
        float ember_lifetime_s = 3.0f;               ///< How long embers last
        float spark_lifetime_s = 0.5f;               ///< How long sparks last

        // === Audio Parameters ===
        float crackling_sound_volume = 0.8f;         ///< Fire crackle loudness
        float roaring_sound_volume = 0.6f;           ///< Large fire roar
        float crackling_frequency_hz = 2.0f;         ///< Crackle pops per second
        float sound_distance_falloff = 1.0f;         ///< Audio attenuation rate

        // === Performance ===
        bool enable_heat_distortion = true;          ///< Render air wobble effect
        bool enable_dynamic_lighting = true;         ///< Fire casts dynamic light
        bool enable_real_time_shadows = true;        ///< Fire casts shadows
        bool enable_particle_collision = false;      ///< Embers bounce off geometry
        float flame_update_rate_hz = 60.0f;          ///< How often to update fire logic
        float propagation_check_radius_m = 5.0f;     ///< Distance to check for spread
        uint32_t max_particles_per_fire = 1000;      ///< Particle budget
        float lod_distance_near_m = 10.0f;           ///< Full detail distance
        float lod_distance_far_m = 50.0f;            ///< Min detail distance

        // === Extinguishing ===
        float water_effectiveness = 1.0f;            ///< How well water puts out fire
        float foam_effectiveness = 1.5f;             ///< Foam suppression multiplier
        float co2_effectiveness = 1.2f;              ///< CO2 suppression multiplier
        float extinguish_duration_s = 2.0f;          ///< Time for fire to fully die
        bool can_reignite = true;                    ///< Allow re-ignition after extinguish
    } config;

    /**
     * @brief Create ignited fire component
     *
     * @param ignition_temp_k Temperature that started the fire (K)
     * @param fuel_mass_kg Mass of burnable material (kg)
     * @return Combustion component in active burning state
     */
    static CombustionComponent ignite(float ignition_temp_k, float fuel_mass_kg) noexcept {
        CombustionComponent fire;
        fire.is_active = true;
        fire.ignition_time = 0.0f;
        fire.fuel_remaining_kg = fuel_mass_kg;
        fire.flame_temperature_k = ignition_temp_k;
        fire.base_flame_temp_k = ignition_temp_k;
        return fire;
    }

    /**
     * @brief Update combustion state
     *
     * @param delta_time_s Time step (seconds)
     * @param oxygen_available Oxygen concentration (0-1)
     * @return Heat released this frame (Joules)
     */
    float update_combustion(float delta_time_s, float oxygen_available = 0.21f) noexcept {
        if (!is_active) return 0.0f;

        oxygen_concentration = oxygen_available * config.oxygen_consumption_multiplier;
        burn_duration += delta_time_s;

        // Check if oxygen sufficient for combustion
        if (oxygen_concentration < config.extinguish_oxygen_threshold) {
            // Extinguish due to lack of oxygen
            is_active = false;
            flame_temperature_k -= config.temperature_decay_rate_k_s * delta_time_s;
            return 0.0f;
        }

        // Check if fuel depleted
        if (fuel_remaining_kg <= 0.0f) {
            is_active = false;
            flame_temperature_k -= config.temperature_decay_rate_k_s * delta_time_s;
            return 0.0f;
        }

        // Calculate combustion efficiency based on oxygen
        if (oxygen_concentration < config.incomplete_combustion_threshold) {
            // Incomplete combustion (less efficient, more smoke)
            combustion_efficiency = oxygen_concentration / config.incomplete_combustion_threshold;
            smoke_generation_rate = 2.0f - combustion_efficiency; // More smoke when inefficient
        } else {
            combustion_efficiency = 0.95f;
            smoke_generation_rate = 1.0f;
        }

        // Consume fuel
        float fuel_consumed_kg = fuel_consumption_rate_kg_s * config.fuel_consumption_multiplier * delta_time_s;
        fuel_remaining_kg -= fuel_consumed_kg;

        if (fuel_remaining_kg < 0.0f) {
            fuel_consumed_kg += fuel_remaining_kg; // Adjust for negative
            fuel_remaining_kg = 0.0f;
        }

        // Calculate heat release (simplified)
        float heat_released_j = fuel_consumed_kg * 30e6f * combustion_efficiency * config.heat_release_multiplier;
        total_energy_released_j += heat_released_j;
        heat_output_w = heat_released_j / delta_time_s;

        // Update flame temperature
        if (fuel_remaining_kg > fuel_consumption_rate_kg_s * 10.0f) {
            // Plenty of fuel: temperature rises
            flame_temperature_k += config.temperature_rise_rate_k_s * delta_time_s;
            if (flame_temperature_k > peak_flame_temp_k) {
                flame_temperature_k = peak_flame_temp_k;
            }
        } else {
            // Low fuel: temperature drops
            flame_temperature_k -= config.temperature_decay_rate_k_s * delta_time_s;
            if (flame_temperature_k < config.min_flame_temperature_k) {
                is_active = false; // Flame too cool to sustain
            }
        }

        // Clamp temperature
        if (flame_temperature_k > config.max_flame_temperature_k) {
            flame_temperature_k = config.max_flame_temperature_k;
        }
        if (flame_temperature_k < config.min_flame_temperature_k) {
            flame_temperature_k = config.min_flame_temperature_k;
        }

        // Update flame geometry based on heat output
        float heat_factor = heat_output_w / 10000.0f; // Normalize to ~1.0
        flame_height_m = (0.5f + heat_factor) * config.flame_height_multiplier;
        flame_radius_m = (0.3f + heat_factor * 0.5f) * config.flame_width_multiplier;

        return heat_released_j;
    }

    /**
     * @brief Apply fire suppression (water, foam, CO2)
     *
     * @param suppression_amount Amount of suppression agent (kg)
     * @param effectiveness Suppression effectiveness multiplier (1.0 = baseline)
     * @return true if fire was extinguished
     */
    bool apply_suppression(float suppression_amount, float effectiveness = 1.0f) noexcept {
        if (!is_active) return true;

        // Reduce flame temperature
        float temp_reduction_k = suppression_amount * 100.0f * effectiveness;
        flame_temperature_k -= temp_reduction_k;

        // Reduce oxygen availability (smothering effect)
        oxygen_concentration -= suppression_amount * 0.01f * effectiveness;
        if (oxygen_concentration < 0.0f) oxygen_concentration = 0.0f;

        // Check if extinguished
        if (flame_temperature_k < config.reignition_temperature_k) {
            is_active = false;
            return true;
        }

        return false;
    }

    /**
     * @brief Check if fire can spread to a neighbor
     *
     * @param distance_m Distance to potential ignition target (meters)
     * @param target_ignition_temp_k Ignition temperature of target material (K)
     * @return true if target should ignite
     */
    bool can_ignite_neighbor(float distance_m, float target_ignition_temp_k) const noexcept {
        if (!is_active) return false;
        if (distance_m > ignition_radius_m) return false;

        // Calculate radiated heat at distance (inverse square law)
        float radiant_heat_w = heat_output_w / (distance_m * distance_m + 1.0f);

        // Estimate temperature increase from radiation
        float temp_increase_k = radiant_heat_w * config.heat_transfer_efficiency * 0.1f;

        return (293.15f + temp_increase_k) >= target_ignition_temp_k;
    }

    /**
     * @brief Get flame color based on temperature
     *
     * Returns RGB color (0-1) based on black-body radiation.
     * Used for dynamic lighting and rendering.
     *
     * @return RGB color (r, g, b) in range [0-1]
     */
    math::Vec3 get_flame_color() const noexcept {
        // Simplified black-body color based on temperature
        // 800K = red, 1200K = orange, 1500K+ = yellow-white

        float t = flame_temperature_k;
        math::Vec3 color{1.0f, 0.3f, 0.0f}; // Default red-orange

        if (t < 900.0f) {
            // Dark red
            color = {0.8f, 0.1f, 0.0f};
        } else if (t < 1100.0f) {
            // Red-orange
            color = {1.0f, 0.3f, 0.0f};
        } else if (t < 1300.0f) {
            // Orange
            color = {1.0f, 0.6f, 0.0f};
        } else if (t < 1500.0f) {
            // Yellow-orange
            color = {1.0f, 0.8f, 0.2f};
        } else {
            // Yellow-white (hottest)
            color = {1.0f, 0.9f, 0.7f};
        }

        return color * config.flame_color_intensity;
    }
};

} // namespace lore::ecs