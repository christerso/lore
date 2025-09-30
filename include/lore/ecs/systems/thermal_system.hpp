#pragma once

#include "lore/ecs/advanced_ecs.hpp"
#include "lore/ecs/components/thermal_properties_component.hpp"
#include "lore/ecs/components/combustion_component.hpp"
#include "lore/ecs/components/anatomy_component.hpp"
#include "lore/math/vec3.hpp"
#include <vector>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief System for processing thermal dynamics and heat transfer
 *
 * Handles:
 * - Heat conduction between touching entities
 * - Thermal radiation (Stefan-Boltzmann law)
 * - Convection with air/fluids
 * - Phase transitions (solid ↔ liquid ↔ gas)
 * - Kinetic energy → thermal energy conversion (ballistics)
 * - Ignition temperature checks
 * - Thermal damage to anatomy
 * - Temperature-dependent material properties
 *
 * Performance:
 * - Spatial hash grid for efficient neighbor queries
 * - Configurable update rate (can run at lower Hz than physics)
 * - Heat transfer caching between stable entities
 * - Early exit for entities at equilibrium
 *
 * Integration:
 * - Ballistics: Projectile impacts → kinetic heating
 * - Combustion: Temperature → ignition checks
 * - Anatomy: Thermal burns damage organs
 * - Materials: Temperature affects structural properties
 *
 * Example usage:
 * @code
 * ThermalSystem thermal_system;
 * thermal_system.set_ambient_temperature(293.15f); // 20°C
 * thermal_system.set_update_rate(30.0f); // 30 Hz
 *
 * // Each frame
 * thermal_system.update(world, delta_time);
 *
 * // Apply ballistic heating
 * thermal_system.apply_kinetic_heating(
 *     entity,
 *     projectile_mass_kg,
 *     projectile_velocity_m_s
 * );
 * @endcode
 */
class ThermalSystem {
public:
    /**
     * @brief Configuration for thermal simulation
     */
    struct Config {
        // === Update Control ===
        float update_rate_hz = 30.0f;           ///< Thermal updates per second (lower = faster)
        bool enable_heat_transfer = true;       ///< Enable conduction/radiation
        bool enable_phase_transitions = true;   ///< Enable melting/boiling/freezing
        bool enable_ignition_checks = true;     ///< Check for auto-ignition
        bool enable_thermal_damage = true;      ///< Apply burns to anatomy

        // === Environment ===
        float ambient_temperature_k = 293.15f;  ///< World ambient temp (20°C)
        float ambient_pressure_pa = 101325.0f;  ///< Atmospheric pressure (1 atm)
        float air_thermal_conductivity = 0.026f; ///< Air conductivity W/(m·K)
        float convection_coefficient = 10.0f;   ///< Natural convection h (W/(m²·K))

        // === Heat Transfer ===
        float conduction_range_m = 0.5f;        ///< Max distance for conduction
        float radiation_range_m = 10.0f;        ///< Max distance for radiation
        float min_temp_diff_for_transfer = 1.0f; ///< Min ΔT to transfer heat (K)
        float heat_transfer_multiplier = 1.0f;  ///< Global heat transfer rate scale

        // === Thermal Damage ===
        float burn_threshold_temp_k = 318.15f;  ///< 45°C starts tissue damage
        float instant_burn_temp_k = 373.15f;    ///< 100°C instant severe burns
        float damage_rate_j_per_hp = 1000.0f;   ///< Joules needed for 1 damage point

        // === Performance ===
        float spatial_grid_cell_size_m = 2.0f;  ///< Spatial hash grid cell size
        bool use_spatial_partitioning = true;   ///< Use spatial grid for neighbors
        uint32_t max_neighbors_per_entity = 20; ///< Limit heat transfer queries
        bool cache_stable_pairs = true;         ///< Cache heat transfer for stable entities

        // === Phase Transitions ===
        float phase_transition_hysteresis_k = 2.0f; ///< Prevent flickering at boundaries
        bool allow_sublimation = true;          ///< Solid → gas directly
        bool track_latent_heat = true;          ///< Account for phase change energy

        // === Debug ===
        bool visualize_heat_flow = false;       ///< Draw heat transfer arrows
        bool log_phase_transitions = false;     ///< Print phase change events
        bool log_ignitions = true;              ///< Print when materials ignite
    };

    ThermalSystem();
    ~ThermalSystem() = default;

    /**
     * @brief Initialize system with configuration
     */
    void initialize(const Config& config);

    /**
     * @brief Update thermal simulation
     *
     * @param world ECS world
     * @param delta_time_s Time step in seconds
     */
    void update(World& world, float delta_time_s);

    /**
     * @brief Apply kinetic heating from projectile impact
     *
     * Converts kinetic energy → thermal energy using:
     * E_kinetic = 0.5 * m * v²
     * E_thermal = E_kinetic * efficiency
     *
     * @param world ECS world
     * @param target_entity Entity being hit
     * @param projectile_mass_kg Projectile mass (kg)
     * @param projectile_velocity_m_s Impact velocity (m/s)
     * @param conversion_efficiency Energy conversion (0-1, default 0.8)
     * @return Temperature increase in Kelvin
     */
    float apply_kinetic_heating(
        World& world,
        Entity target_entity,
        float projectile_mass_kg,
        float projectile_velocity_m_s,
        float conversion_efficiency = 0.8f
    );

    /**
     * @brief Apply external heat to entity
     *
     * @param world ECS world
     * @param entity Target entity
     * @param heat_energy_j Heat energy in Joules
     * @return Temperature increase in Kelvin
     */
    float apply_heat(
        World& world,
        Entity entity,
        float heat_energy_j
    );

    /**
     * @brief Get current temperature of entity
     *
     * @param world ECS world
     * @param entity Target entity
     * @return Temperature in Kelvin (0 if no thermal component)
     */
    float get_temperature(const World& world, Entity entity) const;

    /**
     * @brief Check if entity can ignite at current temperature
     *
     * @param world ECS world
     * @param entity Target entity
     * @return true if temperature >= ignition point
     */
    bool can_ignite(const World& world, Entity entity) const;

    /**
     * @brief Force entity to specific temperature
     *
     * @param world ECS world
     * @param entity Target entity
     * @param temperature_k New temperature (Kelvin)
     */
    void set_temperature(World& world, Entity entity, float temperature_k);

    /**
     * @brief Get configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const Config& config) { config_ = config; }

    /**
     * @brief Set ambient temperature
     */
    void set_ambient_temperature(float temperature_k) {
        config_.ambient_temperature_k = temperature_k;
    }

    /**
     * @brief Set thermal update rate
     */
    void set_update_rate(float hz) {
        config_.update_rate_hz = hz;
    }

    /**
     * @brief Get thermal statistics for debugging
     */
    struct Stats {
        uint32_t entities_processed = 0;
        uint32_t heat_transfers_performed = 0;
        uint32_t phase_transitions = 0;
        uint32_t ignitions_triggered = 0;
        float total_heat_transferred_j = 0.0f;
        float avg_temperature_k = 0.0f;
    };

    const Stats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats{}; }

private:
    /**
     * @brief Spatial grid for efficient neighbor queries
     */
    struct SpatialCell {
        std::vector<Entity> entities;
    };

    /**
     * @brief Cached heat transfer pair
     */
    struct HeatTransferCache {
        Entity entity_a;
        Entity entity_b;
        float last_transfer_j = 0.0f;
        float time_since_update = 0.0f;
    };

    Config config_;
    Stats stats_;

    float accumulated_time_ = 0.0f;  ///< Time accumulator for fixed timestep
    std::unordered_map<int64_t, SpatialCell> spatial_grid_;
    std::vector<HeatTransferCache> heat_transfer_cache_;

    /**
     * @brief Build spatial grid for this frame
     */
    void build_spatial_grid(World& world);

    /**
     * @brief Get spatial grid key for position
     */
    int64_t get_spatial_key(const math::Vec3& position) const;

    /**
     * @brief Get entities near position
     */
    std::vector<Entity> get_nearby_entities(
        const math::Vec3& position,
        float radius_m
    ) const;

    /**
     * @brief Process heat transfer between entity and neighbors
     */
    void process_heat_transfer(
        World& world,
        Entity entity,
        const math::Vec3& position,
        ThermalPropertiesComponent& thermal,
        float delta_time_s
    );

    /**
     * @brief Calculate heat conduction between two entities
     *
     * Fourier's law: Q = k * A * ΔT * Δt / d
     *
     * @param thermal_a First entity thermal properties
     * @param thermal_b Second entity thermal properties
     * @param distance_m Distance between entities (m)
     * @param contact_area_m2 Estimated contact area (m²)
     * @param delta_time_s Time step (s)
     * @return Heat transferred from A to B (Joules)
     */
    float calculate_conduction(
        const ThermalPropertiesComponent& thermal_a,
        const ThermalPropertiesComponent& thermal_b,
        float distance_m,
        float contact_area_m2,
        float delta_time_s
    ) const;

    /**
     * @brief Calculate thermal radiation between two entities
     *
     * Stefan-Boltzmann law: Q = ε * σ * A * (T₁⁴ - T₂⁴) * Δt
     *
     * @param thermal_a First entity thermal properties
     * @param thermal_b Second entity thermal properties
     * @param distance_m Distance between entities (m)
     * @param delta_time_s Time step (s)
     * @return Heat transferred from A to B (Joules)
     */
    float calculate_radiation(
        const ThermalPropertiesComponent& thermal_a,
        const ThermalPropertiesComponent& thermal_b,
        float distance_m,
        float delta_time_s
    ) const;

    /**
     * @brief Calculate convective heat loss to ambient air
     *
     * Newton's law of cooling: Q = h * A * ΔT * Δt
     *
     * @param thermal Entity thermal properties
     * @param delta_time_s Time step (s)
     * @return Heat lost to air (Joules, positive = cooling)
     */
    float calculate_convection(
        const ThermalPropertiesComponent& thermal,
        float delta_time_s
    ) const;

    /**
     * @brief Check and handle phase transitions
     *
     * @param world ECS world
     * @param entity Target entity
     * @param thermal Thermal properties
     * @return true if phase changed
     */
    bool check_phase_transition(
        World& world,
        Entity entity,
        ThermalPropertiesComponent& thermal
    );

    /**
     * @brief Check for auto-ignition and create combustion
     *
     * @param world ECS world
     * @param entity Target entity
     * @param thermal Thermal properties
     * @return true if ignition occurred
     */
    bool check_ignition(
        World& world,
        Entity entity,
        ThermalPropertiesComponent& thermal
    );

    /**
     * @brief Apply thermal damage to anatomy
     *
     * @param world ECS world
     * @param entity Target entity
     * @param thermal Thermal properties
     * @param delta_time_s Time step
     */
    void apply_thermal_damage(
        World& world,
        Entity entity,
        const ThermalPropertiesComponent& thermal,
        float delta_time_s
    );

    /**
     * @brief Estimate contact area between two entities
     *
     * Simplified calculation based on bounding volumes.
     *
     * @param world ECS world
     * @param entity_a First entity
     * @param entity_b Second entity
     * @return Estimated contact area in m²
     */
    float estimate_contact_area(
        World& world,
        Entity entity_a,
        Entity entity_b
    ) const;
};

} // namespace lore::ecs