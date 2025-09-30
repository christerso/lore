#pragma once

#include "lore/ecs/advanced_ecs.hpp"
#include "lore/ecs/components/combustion_component.hpp"
#include "lore/ecs/components/thermal_properties_component.hpp"
#include "lore/ecs/components/chemical_composition_component.hpp"
#include "lore/math/vec3.hpp"
#include <vector>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief System for processing active combustion (fire) simulation
 *
 * Handles:
 * - Fuel consumption and combustion state updates
 * - Heat generation from burning
 * - Oxygen consumption and availability
 * - Fire spread to nearby flammable materials
 * - Flame color/size based on temperature
 * - Smoke and ember particle generation
 * - Fire suppression (water, foam, CO2)
 * - Integration with thermal system
 * - Extinguishing conditions (no fuel, no oxygen, too cold)
 *
 * Performance:
 * - Spatial partitioning for fire spread queries
 * - Configurable update rate
 * - LOD for distant fires
 * - Particle budget management
 *
 * Integration:
 * - ThermalSystem: Fire → heat generation
 * - ChemicalComposition: Determines combustion products
 * - VolumetricFire/Smoke: GPU-based volumetric rendering
 * - Particle systems: Smoke, embers, sparks
 *
 * Example usage:
 * @code
 * CombustionSystem combustion_system;
 * combustion_system.set_oxygen_concentration(0.21f); // 21% air
 * combustion_system.set_update_rate(60.0f); // 60 Hz
 *
 * // Each frame
 * combustion_system.update(world, delta_time);
 *
 * // Apply suppression (water, foam, CO2)
 * combustion_system.apply_suppression(entity, 1.0f, 1.0f); // 1kg water
 *
 * // Manually extinguish
 * combustion_system.extinguish(world, entity);
 * @endcode
 */
class CombustionSystem {
public:
    /**
     * @brief Configuration for combustion simulation
     */
    struct Config {
        // === Update Control ===
        float update_rate_hz = 60.0f;           ///< Combustion updates per second
        bool enable_fire_spread = true;         ///< Enable fire propagation
        bool enable_heat_generation = true;     ///< Generate heat from combustion
        bool enable_oxygen_consumption = true;  ///< Track oxygen depletion
        bool enable_particle_generation = true; ///< Spawn smoke/ember particles

        // === Environment ===
        float ambient_oxygen_concentration = 0.21f; ///< O2 in air (0-1)
        float oxygen_depletion_rate = 1.0f;     ///< How fast O2 is consumed
        float oxygen_replenishment_rate = 0.1f; ///< O2 recovery rate (outdoor)
        math::Vec3 wind_direction{0.0f, 0.0f, 0.0f}; ///< Wind vector (m/s)
        float wind_effect_on_spread = 1.0f;     ///< Wind influence multiplier

        // === Fire Spread ===
        float spread_check_radius_m = 5.0f;     ///< Distance to check for ignition
        float spread_check_interval_s = 0.5f;   ///< How often to check spread
        float ignition_probability = 0.3f;      ///< Chance to ignite per check
        float spread_multiplier = 1.0f;         ///< Global spread rate multiplier
        bool require_line_of_sight = true;      ///< Fire needs LOS to spread

        // === Combustion Physics ===
        float combustion_efficiency_mult = 1.0f; ///< Global burn efficiency
        float heat_release_multiplier = 1.0f;   ///< Heat output scale
        float fuel_consumption_mult = 1.0f;     ///< Fuel burn rate scale
        float temperature_rise_rate_mult = 1.0f; ///< Heating rate scale

        // === Suppression ===
        float water_effectiveness = 1.0f;       ///< Water cooling effect
        float foam_effectiveness = 1.5f;        ///< Foam suppression multiplier
        float co2_effectiveness = 1.2f;         ///< CO2 smothering effect
        float auto_extinguish_time_s = 5.0f;    ///< Time below threshold to extinguish

        // === Particle Effects ===
        uint32_t max_smoke_particles = 10000;   ///< Global smoke particle budget
        uint32_t max_ember_particles = 5000;    ///< Global ember particle budget
        float particle_spawn_rate_mult = 1.0f;  ///< Particle generation multiplier
        float particle_lod_distance_m = 50.0f;  ///< Max distance for particles

        // === Performance ===
        float spatial_grid_cell_size_m = 5.0f;  ///< Spatial hash grid cell size
        bool use_spatial_partitioning = true;   ///< Use spatial grid
        bool enable_lod = true;                 ///< Distance-based quality
        float lod_distance_high_m = 20.0f;      ///< Full simulation distance
        float lod_distance_medium_m = 50.0f;    ///< Reduced simulation distance
        float lod_distance_low_m = 100.0f;      ///< Minimal simulation distance

        // === Debug ===
        bool visualize_fire_spread = false;     ///< Draw spread rays
        bool log_ignitions = true;              ///< Print ignition events
        bool log_extinguishments = true;        ///< Print when fires die
        bool log_fuel_depletion = false;        ///< Print fuel warnings
    };

    CombustionSystem();
    ~CombustionSystem() = default;

    /**
     * @brief Initialize system with configuration
     */
    void initialize(const Config& config);

    /**
     * @brief Update combustion simulation
     *
     * @param world ECS world
     * @param delta_time_s Time step in seconds
     */
    void update(World& world, float delta_time_s);

    /**
     * @brief Manually ignite entity
     *
     * Creates CombustionComponent if entity has thermal/chemical properties.
     *
     * @param world ECS world
     * @param entity Target entity
     * @param ignition_temp_k Initial flame temperature (K)
     * @return true if ignition successful
     */
    bool ignite(
        World& world,
        Entity entity,
        float ignition_temp_k = 1200.0f
    );

    /**
     * @brief Apply fire suppression (water, foam, CO2)
     *
     * @param world ECS world
     * @param entity Target entity
     * @param suppression_amount_kg Amount of suppressant (kg)
     * @param effectiveness Multiplier (1.0 = baseline)
     * @return true if fire was extinguished
     */
    bool apply_suppression(
        World& world,
        Entity entity,
        float suppression_amount_kg,
        float effectiveness = 1.0f
    );

    /**
     * @brief Manually extinguish fire
     *
     * @param world ECS world
     * @param entity Target entity
     */
    void extinguish(World& world, Entity entity);

    /**
     * @brief Check if entity is currently burning
     *
     * @param world ECS world
     * @param entity Target entity
     * @return true if has active combustion
     */
    bool is_burning(const World& world, Entity entity) const;

    /**
     * @brief Get flame temperature
     *
     * @param world ECS world
     * @param entity Target entity
     * @return Temperature in Kelvin (0 if not burning)
     */
    float get_flame_temperature(const World& world, Entity entity) const;

    /**
     * @brief Get configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const Config& config) { config_ = config; }

    /**
     * @brief Set ambient oxygen concentration
     */
    void set_oxygen_concentration(float concentration) {
        config_.ambient_oxygen_concentration = std::clamp(concentration, 0.0f, 1.0f);
    }

    /**
     * @brief Set wind direction/speed
     */
    void set_wind(const math::Vec3& wind_velocity_m_s) {
        config_.wind_direction = wind_velocity_m_s;
    }

    /**
     * @brief Get combustion statistics
     */
    struct Stats {
        uint32_t fires_active = 0;
        uint32_t fires_started = 0;
        uint32_t fires_extinguished = 0;
        uint32_t spread_attempts = 0;
        uint32_t successful_spreads = 0;
        float total_heat_generated_j = 0.0f;
        float total_oxygen_consumed_mol = 0.0f;
        uint32_t smoke_particles_spawned = 0;
        uint32_t ember_particles_spawned = 0;
    };

    const Stats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats{}; }

private:
    struct SpatialCell {
        std::vector<Entity> entities;
    };

    Config config_;
    Stats stats_;

    float accumulated_time_ = 0.0f;
    float spread_check_timer_ = 0.0f;
    std::unordered_map<int64_t, SpatialCell> spatial_grid_;
    uint32_t current_smoke_particles_ = 0;
    uint32_t current_ember_particles_ = 0;

    /**
     * @brief Build spatial grid for fire spread queries
     */
    void build_spatial_grid(World& world);

    /**
     * @brief Get spatial grid key
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
     * @brief Process single fire entity
     */
    void process_fire(
        World& world,
        Entity entity,
        const math::Vec3& position,
        CombustionComponent& combustion,
        float delta_time_s
    );

    /**
     * @brief Check and attempt fire spread to neighbors
     */
    void check_fire_spread(
        World& world,
        Entity entity,
        const math::Vec3& position,
        const CombustionComponent& combustion
    );

    /**
     * @brief Generate heat from combustion
     */
    void generate_heat(
        World& world,
        Entity entity,
        const CombustionComponent& combustion,
        float heat_released_j
    );

    /**
     * @brief Spawn smoke/ember particles
     */
    void spawn_particles(
        World& world,
        Entity entity,
        const math::Vec3& position,
        const CombustionComponent& combustion,
        float delta_time_s
    );

    /**
     * @brief Calculate LOD level based on distance
     */
    enum class LODLevel {
        High,    // Full simulation
        Medium,  // Reduced particle count
        Low,     // Minimal simulation
        None     // No simulation
    };

    LODLevel get_lod_level(float distance_m) const;

    /**
     * @brief Check if fire can spread to target entity
     */
    bool can_spread_to(
        World& world,
        Entity source,
        Entity target,
        const math::Vec3& source_pos,
        const math::Vec3& target_pos
    ) const;
};

} // namespace lore::ecs