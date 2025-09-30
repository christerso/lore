#pragma once

#include "lore/math/vec3.hpp"
#include "lore/math/vec4.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace lore::ecs {

/**
 * @brief Volumetric fire simulation using GPU Navier-Stokes solver
 *
 * State-of-the-art volumetric fire based on:
 * - NVIDIA FlameWorks grid-based fluid simulation
 * - Navier-Stokes equations on GPU compute shaders
 * - 3D density fields for realistic fire volume
 * - Real-time performance (60 FPS target)
 *
 * Implementation uses 5 compute shaders:
 * 1. Advect: Move velocity/density through field
 * 2. AddForce: Apply buoyancy and external forces
 * 3. Divergence: Calculate velocity divergence
 * 4. Jacobi: Pressure solve iterations
 * 5. Project: Make velocity field divergence-free
 *
 * Extensive INI configuration:
 * @code{.ini}
 * [VolumetricFire.Grid]
 * ResolutionX=64
 * ResolutionY=128
 * ResolutionZ=64
 * CellSize=0.1
 * BoundsExpansion=1.5
 *
 * [VolumetricFire.Simulation]
 * TimeStep=0.016
 * AdvectionDissipation=0.99
 * VelocityDissipation=0.98
 * TemperatureDissipation=0.95
 * DensityDissipation=0.92
 * BuoyancyCoefficient=1.0
 * VorticityConfinement=0.3
 *
 * [VolumetricFire.Solver]
 * PressureIterations=40
 * JacobiRelaxation=1.8
 * ProjectionAccuracy=0.001
 *
 * [VolumetricFire.Rendering]
 * DensityMultiplier=1.0
 * EmissionStrength=5.0
 * AbsorptionCoefficient=1.5
 * ScatteringCoefficient=0.8
 * ShadowSteps=32
 * RaymarchSteps=128
 * @endcode
 *
 * Example usage:
 * @code
 * // Create volumetric fire
 * Entity fire = world.create_entity();
 * world.add_component(fire, VolumetricFireComponent::create_campfire());
 *
 * // System updates fire simulation on GPU
 * volumetric_fire_system->update(world, delta_time);
 *
 * // Render uses 3D density texture
 * fire_renderer->render_volumetric_fire(fire_component);
 * @endcode
 */
struct VolumetricFireComponent {
    // Grid dimensions (power of 2 for GPU efficiency)
    uint32_t resolution_x = 64;
    uint32_t resolution_y = 128;  // Taller for flame height
    uint32_t resolution_z = 64;

    float cell_size_m = 0.1f;     ///< Size of each voxel (meters)

    // World-space bounding box
    math::Vec3 bounds_min{-3.2f, 0.0f, -3.2f};
    math::Vec3 bounds_max{3.2f, 12.8f, 3.2f};

    // GPU texture handles (platform-specific, managed by system)
    uint32_t velocity_field_texture = 0;      ///< 3D velocity (R32G32B32F)
    uint32_t density_field_texture = 0;       ///< 3D smoke density (R32F)
    uint32_t temperature_field_texture = 0;   ///< 3D temperature (R32F)
    uint32_t pressure_field_texture = 0;      ///< 3D pressure (R32F)
    uint32_t vorticity_field_texture = 0;     ///< 3D vorticity (R32G32B32F)

    // Fire source (fuel injection point)
    math::Vec3 source_position{0.0f, 0.5f, 0.0f}; ///< World-space fuel source
    float source_radius_m = 1.0f;                  ///< Fuel injection radius
    float source_temperature_k = 1500.0f;          ///< Fuel temp at source
    float source_fuel_rate_kg_s = 0.1f;           ///< Fuel injection rate
    math::Vec3 source_velocity{0.0f, 2.0f, 0.0f}; ///< Initial upward velocity

    // Physical simulation parameters
    float time_step_s = 0.016f;                   ///< Simulation dt (1/60)
    float advection_dissipation = 0.99f;          ///< Momentum conservation (0-1)
    float velocity_dissipation = 0.98f;           ///< Velocity decay per step
    float temperature_dissipation = 0.95f;        ///< Heat loss rate
    float density_dissipation = 0.92f;            ///< Smoke dissipation

    // Buoyancy (hot air rises)
    float buoyancy_coefficient = 1.0f;            ///< Strength of thermal lift
    float ambient_temperature_k = 293.15f;        ///< Ambient temp (20°C)
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};      ///< Gravity vector

    // Vorticity confinement (adds turbulence)
    float vorticity_strength = 0.3f;              ///< Turbulence amount (0-1)

    // Pressure solver parameters
    uint32_t pressure_iterations = 40;            ///< Jacobi iterations
    float pressure_relaxation = 1.8f;             ///< Over-relaxation factor (1-2)
    float projection_accuracy = 0.001f;           ///< Convergence threshold

    // Rendering parameters
    float density_multiplier = 1.0f;              ///< Overall opacity
    float emission_strength = 5.0f;               ///< Light emission intensity
    float absorption_coefficient = 1.5f;          ///< How much light is absorbed
    float scattering_coefficient = 0.8f;          ///< Light scattering amount
    uint32_t shadow_raymarch_steps = 32;          ///< Shadow sampling resolution
    uint32_t primary_raymarch_steps = 128;        ///< Primary ray resolution

    // Color gradient (temperature → color mapping)
    struct ColorGradientPoint {
        float temperature_k;                      ///< Temperature at this point
        math::Vec4 color;                         ///< RGBA color (R, G, B, emission)
    };

    std::vector<ColorGradientPoint> temperature_colors = {
        {800.0f,  {0.1f, 0.0f, 0.0f, 0.5f}},     // Dark red (cool)
        {1000.0f, {0.8f, 0.1f, 0.0f, 2.0f}},     // Red
        {1200.0f, {1.0f, 0.4f, 0.0f, 4.0f}},     // Orange
        {1400.0f, {1.0f, 0.7f, 0.1f, 6.0f}},     // Yellow-orange
        {1600.0f, {1.0f, 0.9f, 0.5f, 8.0f}},     // Yellow-white (hot)
    };

    // Wind and external forces
    math::Vec3 wind_velocity{0.0f, 0.0f, 0.0f};  ///< Wind direction/speed
    float wind_influence = 1.0f;                  ///< How much wind affects fire

    // Performance settings
    bool enable_adaptive_timestep = true;         ///< Dynamic dt for stability
    float max_timestep_s = 0.033f;               ///< Max dt (30 FPS)
    float cfl_number = 1.0f;                     ///< Courant-Friedrichs-Lewy stability
    uint32_t substeps_per_frame = 1;             ///< Subdivide timestep for accuracy

    // LOD (Level of Detail)
    float lod_distance_full_m = 20.0f;           ///< Full resolution distance
    float lod_distance_medium_m = 50.0f;         ///< Half resolution distance
    float lod_distance_low_m = 100.0f;           ///< Quarter resolution distance
    float lod_resolution_scale_medium = 0.5f;    ///< Medium LOD scale
    float lod_resolution_scale_low = 0.25f;      ///< Low LOD scale

    // Extensive INI-configurable parameters
    struct Config {
        // === Grid Configuration ===
        bool allow_dynamic_resolution = false;    ///< Adapt grid to fire size
        float grid_expansion_factor = 1.5f;       ///< Extra space around fire
        bool clamp_to_bounds = true;              ///< Kill particles outside grid

        // === Simulation Quality ===
        float advection_accuracy = 1.0f;          ///< Higher = more accurate (slower)
        bool use_bfecc_advection = false;         ///< Back and Forth Error Compensation
        bool use_maccormack_advection = true;     ///< MacCormack scheme (higher order)
        float maccormack_strength = 0.8f;         ///< Strength of correction (0-1)

        // === Turbulence ===
        float turbulence_octaves = 3.0f;          ///< Noise octaves for turbulence
        float turbulence_frequency = 1.0f;        ///< Noise frequency
        float turbulence_amplitude = 0.5f;        ///< Noise strength
        float turbulence_lacunarity = 2.0f;       ///< Octave frequency multiplier
        float turbulence_persistence = 0.5f;      ///< Octave amplitude decay

        // === Temperature Dynamics ===
        float heat_diffusion_rate = 0.01f;        ///< Thermal conduction
        float cooling_rate_ground_k_s = 10.0f;    ///< Ground heat loss
        float cooling_rate_air_k_s = 1.0f;        ///< Air heat loss
        float temperature_buoyancy_scale = 1.0f;  ///< How temp affects buoyancy

        // === Density/Smoke ===
        float smoke_production_rate = 1.0f;       ///< Smoke generation multiplier
        float smoke_opacity_max = 1.0f;           ///< Maximum smoke darkness
        float smoke_particle_size = 1.0f;         ///< Smoke blob size
        float smoke_rise_speed_m_s = 1.0f;        ///< Additional upward velocity

        // === Combustion Chemistry ===
        float fuel_consumption_rate = 1.0f;       ///< How fast fuel burns
        float oxygen_requirement = 1.0f;          ///< O2 needed for combustion
        float incomplete_combustion_smoke = 2.0f; ///< Smoke from poor burn
        float soot_production_factor = 1.0f;      ///< Black carbon particles

        // === Rendering Quality ===
        uint32_t raymarch_min_steps = 64;         ///< Minimum sampling
        uint32_t raymarch_max_steps = 256;        ///< Maximum sampling
        bool use_adaptive_raymarching = true;     ///< Dynamic step count
        float raymarch_step_jitter = 0.5f;        ///< Dither to reduce banding
        bool enable_volumetric_shadows = true;    ///< Self-shadowing
        bool enable_multiple_scattering = false;  ///< Higher quality (expensive)
        uint32_t scattering_octaves = 1;          ///< Multiple scattering passes

        // === Lighting ===
        float self_illumination_strength = 5.0f;  ///< Fire glow power
        float external_light_influence = 0.3f;    ///< Scene lights affect fire
        bool cast_dynamic_light = true;           ///< Fire lights environment
        float light_radius_m = 10.0f;             ///< Dynamic light range
        float light_falloff_exponent = 2.0f;      ///< Inverse square law

        // === Performance ===
        bool use_compute_shaders = true;          ///< GPU compute (vs pixel shader)
        bool enable_gpu_culling = true;           ///< Cull empty regions
        float culling_density_threshold = 0.01f;  ///< Min density to render
        uint32_t max_visible_fires = 10;          ///< Screen-space limit
        bool enable_temporal_reprojection = true; ///< Reuse prev frame
        float temporal_blend_factor = 0.9f;       ///< Blend with history (0-1)

        // === Debug ===
        bool visualize_velocity_field = false;    ///< Show arrows
        bool visualize_pressure_field = false;    ///< Show pressure colors
        bool visualize_temperature_field = false; ///< Show temp colors
        bool visualize_vorticity = false;         ///< Show turbulence
        float debug_visualization_scale = 1.0f;   ///< Debug display scale
    } config;

    /**
     * @brief Create campfire preset
     */
    static VolumetricFireComponent create_campfire() noexcept {
        VolumetricFireComponent fire;
        fire.resolution_x = 48;
        fire.resolution_y = 96;
        fire.resolution_z = 48;
        fire.cell_size_m = 0.08f;
        fire.bounds_min = {-2.0f, 0.0f, -2.0f};
        fire.bounds_max = {2.0f, 8.0f, 2.0f};
        fire.source_radius_m = 0.8f;
        fire.source_temperature_k = 1200.0f;
        fire.source_fuel_rate_kg_s = 0.05f;
        fire.buoyancy_coefficient = 0.8f;
        fire.emission_strength = 4.0f;
        return fire;
    }

    /**
     * @brief Create large building fire preset
     */
    static VolumetricFireComponent create_building_fire() noexcept {
        VolumetricFireComponent fire;
        fire.resolution_x = 128;
        fire.resolution_y = 256;
        fire.resolution_z = 128;
        fire.cell_size_m = 0.2f;
        fire.bounds_min = {-12.8f, 0.0f, -12.8f};
        fire.bounds_max = {12.8f, 51.2f, 12.8f};
        fire.source_radius_m = 5.0f;
        fire.source_temperature_k = 1400.0f;
        fire.source_fuel_rate_kg_s = 1.0f;
        fire.buoyancy_coefficient = 1.2f;
        fire.emission_strength = 6.0f;
        fire.config.smoke_production_rate = 3.0f;
        return fire;
    }

    /**
     * @brief Create torch/small flame preset
     */
    static VolumetricFireComponent create_torch() noexcept {
        VolumetricFireComponent fire;
        fire.resolution_x = 32;
        fire.resolution_y = 64;
        fire.resolution_z = 32;
        fire.cell_size_m = 0.05f;
        fire.bounds_min = {-0.8f, 0.0f, -0.8f};
        fire.bounds_max = {0.8f, 3.2f, 0.8f};
        fire.source_radius_m = 0.3f;
        fire.source_temperature_k = 1300.0f;
        fire.source_fuel_rate_kg_s = 0.01f;
        fire.buoyancy_coefficient = 1.0f;
        fire.emission_strength = 5.0f;
        fire.config.smoke_production_rate = 0.5f;
        return fire;
    }

    /**
     * @brief Calculate total grid cells
     */
    uint32_t get_total_cells() const noexcept {
        return resolution_x * resolution_y * resolution_z;
    }

    /**
     * @brief Calculate grid memory usage (bytes)
     */
    uint64_t get_memory_usage() const noexcept {
        uint32_t cells = get_total_cells();
        // 5 fields: velocity (12 bytes), density (4), temp (4), pressure (4), vorticity (12)
        return cells * (12 + 4 + 4 + 4 + 12);
    }

    /**
     * @brief Get world-space position of grid cell
     *
     * @param x Grid X coordinate
     * @param y Grid Y coordinate
     * @param z Grid Z coordinate
     * @return World-space position (meters)
     */
    math::Vec3 get_cell_world_position(uint32_t x, uint32_t y, uint32_t z) const noexcept {
        float fx = static_cast<float>(x) / static_cast<float>(resolution_x);
        float fy = static_cast<float>(y) / static_cast<float>(resolution_y);
        float fz = static_cast<float>(z) / static_cast<float>(resolution_z);

        return math::Vec3{
            bounds_min.x + fx * (bounds_max.x - bounds_min.x),
            bounds_min.y + fy * (bounds_max.y - bounds_min.y),
            bounds_min.z + fz * (bounds_max.z - bounds_min.z)
        };
    }
};

} // namespace lore::ecs