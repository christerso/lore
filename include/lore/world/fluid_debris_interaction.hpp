#pragma once

#include <lore/math/math.hpp>
#include <lore/world/voronoi_fracture.hpp>
#include <lore/world/debris_physics.hpp>
#include <vector>
#include <cstdint>

namespace lore::world {

/**
 * @brief Fluid cell data for grid-based fluid simulation
 *
 * Represents a single cell in the fluid simulation grid.
 * The fluid system runs on a separate grid from debris voxels.
 */
struct FluidCell {
    float density = 0.0f;           // Fluid density (0-1, typically 0=air, 1=water)
    math::Vec3 velocity{0, 0, 0};   // Fluid velocity (m/s)
    float pressure = 0.0f;          // Pressure (Pascals)
    float temperature = 20.0f;      // Temperature (Celsius)

    // Fluid type
    enum class Type : uint8_t {
        Air = 0,
        Water = 1,
        Smoke = 2,
        Fire = 3,
        Steam = 4
    };
    Type type = Type::Air;
};

/**
 * @brief Fluid simulation grid (simplified)
 *
 * This is a stub representing the engine's fluid simulation system.
 * In practice, this would be replaced by the actual fluid solver.
 */
struct FluidGrid {
    std::vector<FluidCell> cells;
    math::Vec3 origin{0, 0, 0};     // World space origin
    math::Vec3 cell_size{0.1f, 0.1f, 0.1f};  // Cell dimensions
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;

    /**
     * @brief Sample fluid properties at world position
     */
    FluidCell sample_at_position(const math::Vec3& world_pos) const;

    /**
     * @brief Get cell index from world position
     */
    bool world_to_grid(const math::Vec3& world_pos, uint32_t& x, uint32_t& y, uint32_t& z) const;

    /**
     * @brief Get cell at grid coordinates
     */
    const FluidCell* get_cell(uint32_t x, uint32_t y, uint32_t z) const;
};

/**
 * @brief Fluid-debris interaction configuration
 */
struct FluidDebrisConfig {
    // Buoyancy (Archimedes principle)
    float fluid_density = 1000.0f;          // kg/m³ (1000 = water)
    float gravity_magnitude = 9.81f;        // m/s²
    bool enable_buoyancy = true;

    // Drag forces
    float linear_drag_coefficient = 0.5f;   // Coefficient for linear drag (0-2)
    float angular_drag_coefficient = 0.3f;  // Coefficient for angular drag (0-2)
    bool enable_drag = true;

    // Voxel-based collision
    float voxel_collision_threshold = 0.5f; // Fluid density threshold for collision
    bool enable_voxel_collision = true;

    // Flow interaction
    float flow_influence_strength = 1.0f;   // How much fluid flow affects debris (0-2)
    bool enable_flow_influence = true;

    // Performance
    uint32_t max_debris_to_process = 1000;  // Limit for performance
    bool skip_sleeping_debris = true;        // Don't process sleeping debris
};

/**
 * @brief Fluid-debris interaction system
 *
 * Handles realistic interaction between fluid simulation and debris pieces:
 * - Buoyancy forces (Archimedes principle) for floating debris
 * - Fluid drag (linear and angular) based on relative velocity
 * - Voxel-based collision detection for accurate fluid-solid interaction
 * - Flow influence (debris pushed by fluid currents)
 *
 * Uses the internal 4×4×4 voxel approximation per debris piece for:
 * - Calculating submerged volume (for buoyancy)
 * - Determining drag area
 * - Fluid flow interaction points
 *
 * Performance: <1ms per 100 debris pieces in typical fluid
 */
class FluidDebrisInteraction {
public:
    explicit FluidDebrisInteraction(const FluidDebrisConfig& config = {});

    /**
     * @brief Update fluid-debris interaction for one timestep
     *
     * Applies buoyancy and drag forces to debris pieces based on fluid state.
     *
     * @param debris_pieces Vector of debris to update
     * @param fluid_grid Fluid simulation grid
     * @param delta_time Time step in seconds
     */
    void update(
        std::vector<DebrisPiece>& debris_pieces,
        const FluidGrid& fluid_grid,
        float delta_time
    );

    /**
     * @brief Calculate buoyancy force on debris piece
     *
     * Uses Archimedes principle: F_buoyancy = ρ_fluid × V_displaced × g
     * V_displaced is calculated from occupied voxels in fluid.
     *
     * @param piece Debris piece with voxel approximation
     * @param fluid_grid Fluid simulation grid
     * @return Buoyancy force vector (Newtons)
     */
    math::Vec3 calculate_buoyancy(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid
    ) const;

    /**
     * @brief Calculate drag force on debris piece
     *
     * F_drag = 0.5 × ρ × v² × C_d × A
     * Where:
     * - ρ = fluid density
     * - v = relative velocity (debris - fluid)
     * - C_d = drag coefficient
     * - A = cross-sectional area (approximated from voxels)
     *
     * @param piece Debris piece
     * @param fluid_grid Fluid simulation grid
     * @return Drag force vector (Newtons)
     */
    math::Vec3 calculate_drag(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid
    ) const;

    /**
     * @brief Calculate angular drag (torque) on debris piece
     *
     * @param piece Debris piece
     * @param fluid_grid Fluid simulation grid
     * @return Angular drag torque vector (N⋅m)
     */
    math::Vec3 calculate_angular_drag(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid
    ) const;

    /**
     * @brief Calculate flow influence force
     *
     * Pushes debris along fluid flow direction based on voxel overlap.
     *
     * @param piece Debris piece
     * @param fluid_grid Fluid simulation grid
     * @return Flow force vector (Newtons)
     */
    math::Vec3 calculate_flow_force(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid
    ) const;

    /**
     * @brief Update voxel positions after debris rotation/translation
     *
     * Transforms voxel local positions to world space based on debris state.
     *
     * @param piece Debris piece to update
     */
    void update_voxel_positions(DebrisPiece& piece) const;

    /**
     * @brief Check if debris piece is submerged in fluid
     *
     * @param piece Debris piece
     * @param fluid_grid Fluid simulation grid
     * @param out_submerged_ratio Output: ratio of voxels in fluid (0-1)
     * @return True if any part is submerged
     */
    bool is_submerged(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid,
        float* out_submerged_ratio = nullptr
    ) const;

    /**
     * @brief Get/set configuration
     */
    const FluidDebrisConfig& get_config() const { return config_; }
    void set_config(const FluidDebrisConfig& config) { config_ = config; }

    /**
     * @brief Statistics from last update
     */
    struct Statistics {
        uint32_t debris_processed = 0;
        uint32_t debris_submerged = 0;
        uint32_t voxels_in_fluid = 0;
        float average_buoyancy = 0.0f;
        float average_drag = 0.0f;
        float update_time_ms = 0.0f;
    };
    const Statistics& get_statistics() const { return stats_; }

private:
    FluidDebrisConfig config_;
    Statistics stats_;

    /**
     * @brief Calculate submerged volume from voxel grid
     *
     * Returns volume in m³ of debris piece that's in fluid.
     */
    float calculate_submerged_volume(
        const DebrisPiece& piece,
        const FluidGrid& fluid_grid
    ) const;

    /**
     * @brief Calculate cross-sectional area for drag
     *
     * Approximates area from voxel distribution perpendicular to velocity.
     */
    float calculate_drag_area(
        const DebrisPiece& piece,
        const math::Vec3& velocity_direction
    ) const;

    /**
     * @brief Rotate vector by quaternion
     */
    static math::Vec3 rotate_vector(const math::Vec3& v, const math::Quat& q);
};

} // namespace lore::world
