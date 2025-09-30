#pragma once

#include <lore/math/math.hpp>
#include <lore/world/tile_mesh_cache.hpp>
#include <lore/world/voronoi_fracture.hpp>  // For ImpactType enum
#include <vector>
#include <cstdint>

namespace lore::world {

/**
 * @brief Crack path segment for visual rendering
 *
 * Represents a single segment of a crack on a tile surface.
 * Cracks are rendered as decals on the intact mesh.
 */
struct CrackSegment {
    math::Vec3 start_position;      // Start point (world space)
    math::Vec3 end_position;        // End point (world space)
    float width;                    // Crack width in meters (0.001-0.05)
    float depth;                    // Visual depth for rendering (0-1)
    uint32_t generation;            // Crack generation (0=primary, 1=secondary branch, etc.)
};

/**
 * @brief Complete crack path with branching
 *
 * A crack consists of multiple segments that may branch.
 * Cracks propagate along stress lines at 0.1-1.0 m/s.
 */
struct CrackPath {
    std::vector<CrackSegment> segments;     // All segments in this crack
    math::Vec3 origin;                      // Where crack started
    math::Vec3 direction;                   // Primary propagation direction
    float propagation_speed;                // m/s (0.1-1.0)
    float total_length;                     // Total crack length in meters
    float time_since_creation;              // Seconds since crack appeared
    bool is_active;                         // Still propagating?
};

/**
 * @brief Tile damage state and crack information
 *
 * Tracks all damage-related data for a single tile:
 * - Current health and state
 * - Active crack paths
 * - Damage accumulation history
 * - Visual representation data
 */
struct TileDamage {
    // Health and state
    float health = 100.0f;                  // Current health (0-100)
    TileState state = TileState::Pristine;  // Current damage state
    float accumulated_damage = 0.0f;        // Total damage taken

    // Crack paths
    std::vector<CrackPath> crack_paths;     // All cracks on this tile

    // Stress tracking (for crack propagation direction)
    math::Vec3 primary_stress_direction{0, 0, 0};   // Direction of maximum stress
    float stress_magnitude = 0.0f;                  // Current stress level

    // Timing
    float time_in_current_state = 0.0f;     // Time spent in current TileState
    float time_until_collapse = 0.0f;       // Countdown to structural failure (Critical state)

    // Impact history (for crack initiation)
    math::Vec3 last_impact_position{0, 0, 0};
    math::Vec3 last_impact_direction{0, 0, 0};
    float last_impact_force = 0.0f;
    float time_since_last_impact = 0.0f;
};

/**
 * @brief Damage configuration parameters
 */
struct TileDamageConfig {
    // State transition thresholds (health percentages)
    float scratched_threshold = 90.0f;      // Health % for Pristine → Scratched
    float cracked_threshold = 70.0f;        // Health % for Scratched → Cracked
    float damaged_threshold = 50.0f;        // Health % for Cracked → Damaged
    float failing_threshold = 30.0f;        // Health % for Damaged → Failing
    float critical_threshold = 10.0f;       // Health % for Failing → Critical

    // Crack propagation
    float min_propagation_speed = 0.1f;     // m/s (slow cracks)
    float max_propagation_speed = 1.0f;     // m/s (fast cracks)
    float crack_branch_probability = 0.3f;  // Chance of crack branching (0-1)
    float max_crack_width = 0.05f;          // Maximum crack width in meters (5cm)

    // Damage accumulation
    float impact_damage_multiplier = 1.0f;  // Scale factor for impact damage
    float stress_damage_rate = 1.0f;        // Damage/second when under high stress

    // Critical state
    float critical_state_duration = 3.0f;   // Seconds from Critical → Collapsed
    float warning_shake_amplitude = 0.02f;  // Visual shake in meters
};

/**
 * @brief Progressive damage system
 *
 * Manages tile damage states, crack propagation, and visual feedback:
 * - Health-based state machine (7 states: Pristine → Collapsed)
 * - Crack initiation from impacts
 * - Crack propagation along stress lines
 * - Visual crack rendering (decals on intact mesh)
 * - Warning signs before structural failure
 *
 * Performance: <0.5ms per 100 tiles with active cracks
 */
class TileDamageSystem {
public:
    explicit TileDamageSystem(const TileDamageConfig& config = {});

    /**
     * @brief Update damage system for one timestep
     *
     * Performs:
     * 1. Propagate active cracks
     * 2. Apply stress-based damage
     * 3. Update state machine transitions
     * 4. Check for structural collapse
     *
     * @param damage_data Vector of tile damage (one per tile)
     * @param delta_time Time step in seconds
     */
    void update(std::vector<TileDamage>& damage_data, float delta_time);

    /**
     * @brief Apply damage to a tile from an impact
     *
     * Calculates damage based on impact force and type.
     * Initiates new crack paths from impact point.
     *
     * @param damage Tile damage to modify
     * @param impact_position Impact point (world space)
     * @param impact_direction Impact direction (normalized)
     * @param impact_force Force in Newtons
     * @param impact_type Type of impact
     */
    void apply_impact_damage(
        TileDamage& damage,
        const math::Vec3& impact_position,
        const math::Vec3& impact_direction,
        float impact_force,
        ImpactType impact_type
    );

    /**
     * @brief Apply continuous stress damage
     *
     * Used for tiles under structural load (e.g., supporting weight).
     *
     * @param damage Tile damage to modify
     * @param stress_direction Direction of stress (normalized)
     * @param stress_magnitude Stress level (Pascals)
     * @param delta_time Time step in seconds
     */
    void apply_stress_damage(
        TileDamage& damage,
        const math::Vec3& stress_direction,
        float stress_magnitude,
        float delta_time
    );

    /**
     * @brief Check if tile should transition to next damage state
     *
     * @param damage Tile damage to check
     * @return True if state changed
     */
    bool update_damage_state(TileDamage& damage);

    /**
     * @brief Initiate a new crack from impact point
     *
     * Creates crack path(s) based on impact type and tile state.
     *
     * @param damage Tile damage to modify
     * @param origin Crack starting point
     * @param primary_direction Primary crack direction
     * @param impact_force Impact force (affects crack speed and branching)
     */
    void initiate_crack(
        TileDamage& damage,
        const math::Vec3& origin,
        const math::Vec3& primary_direction,
        float impact_force
    );

    /**
     * @brief Propagate active cracks
     *
     * Updates all active crack paths, extending them along stress lines.
     *
     * @param damage Tile damage with crack paths
     * @param delta_time Time step in seconds
     */
    void propagate_cracks(TileDamage& damage, float delta_time);

    /**
     * @brief Get visual shake offset for warning effects
     *
     * Returns position offset for tiles in Critical state.
     *
     * @param damage Tile damage
     * @param current_time Current game time (for animation)
     * @return Offset vector in meters
     */
    math::Vec3 get_warning_shake_offset(const TileDamage& damage, float current_time) const;

    /**
     * @brief Get/set damage configuration
     */
    const TileDamageConfig& get_config() const { return config_; }
    void set_config(const TileDamageConfig& config) { config_ = config; }

    /**
     * @brief Statistics from last update
     */
    struct Statistics {
        uint32_t tiles_with_cracks = 0;
        uint32_t active_crack_paths = 0;
        uint32_t total_crack_segments = 0;
        uint32_t tiles_in_critical_state = 0;
        float update_time_ms = 0.0f;
    };
    const Statistics& get_statistics() const { return stats_; }

private:
    TileDamageConfig config_;
    Statistics stats_;

    /**
     * @brief Calculate damage amount from impact
     */
    float calculate_impact_damage(float impact_force, ImpactType impact_type) const;

    /**
     * @brief Determine crack propagation direction
     *
     * Uses stress direction and random perturbation.
     */
    math::Vec3 calculate_crack_direction(
        const TileDamage& damage,
        const math::Vec3& current_position,
        const math::Vec3& previous_direction
    ) const;

    /**
     * @brief Check if crack should branch
     */
    bool should_crack_branch(const CrackPath& crack, float impact_force) const;

    /**
     * @brief Get state transition threshold for current state
     */
    float get_state_threshold(TileState current_state) const;
};

} // namespace lore::world
