#pragma once

#include "lore/math/vec3.hpp"
#include "lore/math/vec4.hpp"
#include <cstdint>
#include <string>

namespace lore::ecs {

/**
 * @brief Volumetric smoke rendering with ReSTIR lighting
 *
 * Microsoft Flight Simulator-level volumetric clouds/smoke based on:
 * - NVIDIA ReSTIR (Reservoir Spatio-Temporal Importance Resampling) 2025
 * - 3D density fields with real-time weather integration
 * - GPU compute shaders for density diffusion and buoyancy
 * - High-contrast volumetric light and shadows
 * - Hybrid volumetric path tracing with ReSTIR sampling
 *
 * Performance target: 60 FPS with hundreds of smoke plumes
 *
 * INI Configuration:
 * @code{.ini}
 * [VolumetricSmoke.Grid]
 * ResolutionX=128
 * ResolutionY=128
 * ResolutionZ=128
 * CellSize=0.2
 * AutoExpand=true
 *
 * [VolumetricSmoke.Simulation]
 * DiffusionRate=0.01
 * DissipationRate=0.98
 * BuoyancyStrength=1.0
 * WindInfluence=1.0
 * TurbulenceAmount=0.3
 *
 * [VolumetricSmoke.Rendering]
 * DensityScale=1.0
 * Opacity=0.8
 * AbsorptionCoeff=2.0
 * ScatteringCoeff=1.2
 * AmbientOcclusion=0.5
 * ReSTIRSamples=8
 * TemporalSamples=16
 *
 * [VolumetricSmoke.Quality]
 * RaymarchSteps=128
 * ShadowSteps=32
 * EnableReSTIR=true
 * EnableTemporalReprojection=true
 * NoiseOctaves=4
 * @endcode
 *
 * Example usage:
 * @code
 * // Create smoke from fire
 * Entity smoke = world.create_entity();
 * world.add_component(smoke, VolumetricSmokeComponent::create_from_fire(
 *     fire_position, fire_temperature, fire_fuel_rate
 * ));
 *
 * // System simulates smoke density diffusion on GPU
 * volumetric_smoke_system->update(world, delta_time);
 *
 * // Render uses ReSTIR for lighting
 * smoke_renderer->render_with_restir(smoke_component);
 * @endcode
 */
struct VolumetricSmokeComponent {
    // 3D density grid (power of 2 for GPU efficiency)
    uint32_t resolution_x = 128;
    uint32_t resolution_y = 128;
    uint32_t resolution_z = 128;

    float cell_size_m = 0.2f;         ///< Voxel size (meters)

    // World-space bounds
    math::Vec3 bounds_min{-12.8f, 0.0f, -12.8f};
    math::Vec3 bounds_max{12.8f, 25.6f, 12.8f};

    // GPU texture handles (managed by system)
    uint32_t density_field_texture = 0;       ///< 3D smoke density (R32F)
    uint32_t velocity_field_texture = 0;      ///< 3D velocity (R32G32B32F)
    uint32_t temperature_field_texture = 0;   ///< 3D temperature (R32F)
    uint32_t color_field_texture = 0;         ///< 3D smoke color (R8G8B8A8)

    // ReSTIR lighting textures
    uint32_t restir_reservoir_texture = 0;    ///< Spatial reservoir
    uint32_t temporal_reservoir_texture = 0;  ///< Temporal history
    uint32_t radiance_cache_texture = 0;      ///< Cached lighting

    // Smoke source (emitter)
    math::Vec3 source_position{0.0f, 1.0f, 0.0f};
    float source_radius_m = 1.0f;
    float source_emission_rate = 1.0f;        ///< Smoke spawn rate
    math::Vec3 source_velocity{0.0f, 2.0f, 0.0f};

    // Physical properties
    float diffusion_rate = 0.01f;             ///< How fast smoke spreads
    float dissipation_rate = 0.98f;           ///< Smoke decay per frame (0-1)
    float buoyancy_strength = 1.0f;           ///< Thermal lift force
    float ambient_temperature_k = 293.15f;    ///< Ambient temp
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};

    // Wind and external forces
    math::Vec3 wind_velocity{0.0f, 0.0f, 0.0f};
    float wind_influence = 1.0f;
    float turbulence_amount = 0.3f;           ///< Procedural noise strength

    // Rendering properties
    float density_scale = 1.0f;               ///< Overall opacity multiplier
    float opacity = 0.8f;                     ///< Max opacity (0-1)
    float absorption_coefficient = 2.0f;      ///< Light absorption (Beer's law)
    float scattering_coefficient = 1.2f;      ///< Out-scattering amount
    float anisotropy = 0.6f;                  ///< Forward scattering bias (-1 to 1)

    // Smoke color
    math::Vec4 base_color{0.2f, 0.2f, 0.2f, 1.0f}; ///< Default gray smoke
    float color_variation = 0.1f;             ///< Random color noise

    // ReSTIR parameters (2025 NVIDIA technique)
    uint32_t restir_spatial_samples = 8;      ///< Spatial neighbor samples
    uint32_t restir_temporal_samples = 16;    ///< Temporal reuse samples
    float restir_temporal_blend = 0.95f;      ///< History weight (0-1)
    float restir_bias_correction = 1.0f;      ///< Unbiased estimator weight

    // Ambient occlusion
    float ambient_occlusion_strength = 0.5f;  ///< Self-shadowing intensity
    uint32_t ao_sample_count = 8;             ///< AO rays per voxel

    // Raymarch quality
    uint32_t raymarch_steps = 128;            ///< Primary ray samples
    uint32_t shadow_raymarch_steps = 32;      ///< Shadow ray samples
    float raymarch_step_jitter = 0.5f;        ///< Dithering to reduce banding
    float raymarch_adaptive_threshold = 0.01f; ///< Early ray termination

    // Noise-based detail (MSFS clouds use this)
    float noise_frequency = 1.0f;             ///< Base noise frequency
    float noise_amplitude = 0.5f;             ///< Noise strength
    uint32_t noise_octaves = 4;               ///< Fractal detail levels
    float noise_lacunarity = 2.0f;            ///< Frequency multiplier per octave
    float noise_persistence = 0.5f;           ///< Amplitude decay per octave
    math::Vec3 noise_scroll_speed{0.1f, 0.2f, 0.1f}; ///< Animated detail

    // LOD (Level of Detail)
    float lod_distance_full_m = 50.0f;
    float lod_distance_medium_m = 150.0f;
    float lod_distance_low_m = 300.0f;
    float lod_resolution_scale_medium = 0.5f;
    float lod_resolution_scale_low = 0.25f;

    // Temporal reprojection (reuse previous frame)
    bool enable_temporal_reprojection = true;
    float temporal_blend_factor = 0.9f;       ///< Blend with history
    uint32_t temporal_history_buffer = 0;     ///< Previous frame texture

    // Bitmap texture support (user-provided smoke shapes)
    uint32_t shape_texture = 0;               ///< Custom smoke shape (R8)
    float shape_influence = 0.0f;             ///< 0=procedural, 1=texture
    bool tile_shape_texture = true;           ///< Repeat texture in 3D

    // Extensive INI-configurable parameters
    struct Config {
        // === Simulation Quality ===
        float simulation_timestep_s = 0.016f; ///< Fixed dt (1/60)
        uint32_t substeps_per_frame = 1;      ///< Subdivide for stability
        bool use_bfecc_advection = false;     ///< Higher order advection
        float advection_limiter = 1.0f;       ///< Clamp extreme velocities

        // === Diffusion ===
        float density_diffusion_rate = 0.01f; ///< Smoke spread speed
        float temperature_diffusion_rate = 0.05f; ///< Heat conduction
        uint32_t diffusion_iterations = 20;   ///< Solver iterations
        bool use_multigrid_solver = false;    ///< Faster convergence (complex)

        // === Dissipation ===
        float density_decay_rate = 0.02f;     ///< Smoke fades per second
        float temperature_decay_rate = 0.05f; ///< Cooling rate
        float velocity_damping = 0.99f;       ///< Momentum loss
        float ground_absorption = 0.5f;       ///< Density lost at ground

        // === Buoyancy ===
        float buoyancy_temperature_scale = 1.0f;
        float buoyancy_density_scale = 1.0f;
        float vorticity_confinement = 0.3f;   ///< Add turbulence

        // === Wind ===
        math::Vec3 wind_direction{0.0f, 0.0f, 1.0f};
        float wind_speed_m_s = 2.0f;
        float wind_turbulence = 0.2f;         ///< Wind gustiness
        float wind_vertical_influence = 0.5f; ///< Wind on Y axis

        // === Rendering Quality ===
        bool enable_adaptive_raymarching = true;
        uint32_t raymarch_min_steps = 64;
        uint32_t raymarch_max_steps = 256;
        float early_ray_termination_threshold = 0.99f;
        bool use_blue_noise_dithering = true; ///< Better dither pattern

        // === Lighting (ReSTIR) ===
        bool enable_restir_gi = true;         ///< ReSTIR global illumination
        bool enable_restir_shadows = true;    ///< ReSTIR volumetric shadows
        uint32_t restir_initial_samples = 32; ///< Candidate light samples
        float restir_visibility_bias = 0.1f;  ///< Shadow softness
        bool enable_multiple_scattering = false; ///< Very expensive
        uint32_t scattering_bounces = 2;      ///< Light bounces in smoke

        // === Ambient Occlusion ===
        float ao_radius_m = 2.0f;             ///< AO sample distance
        float ao_falloff = 2.0f;              ///< Distance attenuation
        bool use_hbao = false;                ///< Horizon-Based AO (better)

        // === Noise Detail ===
        bool use_3d_texture_noise = true;     ///< Precomputed noise texture
        bool use_procedural_noise = false;    ///< Runtime noise (slower)
        float detail_noise_scale = 2.0f;      ///< Fine detail frequency
        float detail_noise_strength = 0.3f;   ///< Detail amount
        bool animate_noise = true;            ///< Scrolling detail

        // === Color ===
        bool enable_temperature_color = true; ///< Hot smoke = lighter
        float temperature_color_scale = 1.0f;
        bool enable_density_color = true;     ///< Dense = darker
        float density_color_scale = 0.5f;
        math::Vec4 tint_color{1.0f, 1.0f, 1.0f, 1.0f};

        // === Performance ===
        bool use_compute_shaders = true;
        bool enable_frustum_culling = true;
        bool enable_occlusion_culling = true;
        float culling_density_threshold = 0.01f;
        uint32_t max_visible_smoke_entities = 20;
        bool enable_async_compute = true;     ///< Overlap GPU work

        // === Temporal Reprojection ===
        float temporal_max_velocity_pixels = 20.0f;
        float temporal_stability_threshold = 0.1f;
        bool temporal_antialiasing = true;
        float temporal_jitter_scale = 1.0f;

        // === Bitmap Texture ===
        bool enable_custom_shapes = false;
        std::string shape_texture_path;       ///< Path to shape bitmap
        float shape_blend_sharpness = 1.0f;   ///< Edge blending
        math::Vec3 shape_tiling{1.0f, 1.0f, 1.0f};

        // === Debug ===
        bool visualize_density_field = false;
        bool visualize_velocity_field = false;
        bool visualize_temperature_field = false;
        bool visualize_restir_samples = false;
        float debug_slice_height = 0.5f;      ///< Debug slice Y position
    } config;

    /**
     * @brief Create smoke from fire source
     */
    static VolumetricSmokeComponent create_from_fire(
        const math::Vec3& fire_position,
        float fire_temperature_k,
        float fire_fuel_rate_kg_s
    ) noexcept {
        VolumetricSmokeComponent smoke;
        smoke.source_position = fire_position + math::Vec3{0.0f, 1.0f, 0.0f};
        smoke.source_emission_rate = fire_fuel_rate_kg_s * 2.0f; // Smoke production
        smoke.source_velocity = {0.0f, 3.0f, 0.0f}; // Strong upward
        smoke.buoyancy_strength = 1.5f;
        smoke.base_color = {0.1f, 0.1f, 0.1f, 1.0f}; // Dark gray
        smoke.config.density_decay_rate = 0.01f; // Slow dissipation
        return smoke;
    }

    /**
     * @brief Create explosion smoke cloud
     */
    static VolumetricSmokeComponent create_explosion_smoke() noexcept {
        VolumetricSmokeComponent smoke;
        smoke.resolution_x = 64;
        smoke.resolution_y = 64;
        smoke.resolution_z = 64;
        smoke.cell_size_m = 0.5f;
        smoke.bounds_min = {-16.0f, -16.0f, -16.0f};
        smoke.bounds_max = {16.0f, 16.0f, 16.0f};
        smoke.source_emission_rate = 10.0f; // Burst
        smoke.source_velocity = {0.0f, 5.0f, 0.0f};
        smoke.diffusion_rate = 0.05f; // Fast spread
        smoke.dissipation_rate = 0.95f; // Quick fade
        smoke.base_color = {0.05f, 0.05f, 0.05f, 1.0f}; // Very dark
        smoke.turbulence_amount = 0.8f; // Chaotic
        smoke.config.density_decay_rate = 0.05f;
        return smoke;
    }

    /**
     * @brief Create MSFS-style volumetric cloud
     */
    static VolumetricSmokeComponent create_volumetric_cloud() noexcept {
        VolumetricSmokeComponent cloud;
        cloud.resolution_x = 256;
        cloud.resolution_y = 128;
        cloud.resolution_z = 256;
        cloud.cell_size_m = 2.0f; // Large scale
        cloud.bounds_min = {-256.0f, 500.0f, -256.0f};
        cloud.bounds_max = {256.0f, 756.0f, 256.0f};
        cloud.diffusion_rate = 0.001f; // Slow change
        cloud.dissipation_rate = 0.9999f; // Very persistent
        cloud.buoyancy_strength = 0.1f; // Gentle
        cloud.base_color = {0.9f, 0.9f, 0.95f, 1.0f}; // White-ish
        cloud.absorption_coefficient = 0.5f; // Translucent
        cloud.scattering_coefficient = 2.0f; // High scatter
        cloud.anisotropy = 0.8f; // Forward scattering
        cloud.noise_octaves = 6; // High detail
        cloud.config.enable_restir_gi = true; // Full lighting
        cloud.config.enable_multiple_scattering = true; // Realism
        cloud.config.scattering_bounces = 3;
        return cloud;
    }

    /**
     * @brief Calculate total memory usage
     */
    uint64_t get_memory_usage() const noexcept {
        uint32_t cells = resolution_x * resolution_y * resolution_z;
        // 4 fields: density (4 bytes), velocity (12), temp (4), color (4)
        // + ReSTIR reservoirs (16 bytes per cell)
        return cells * (4 + 12 + 4 + 4 + 16);
    }

    /**
     * @brief Check if smoke reduces vision system visibility
     *
     * Integrates with vision system for occlusion.
     *
     * @param position World position to check
     * @return Visibility multiplier (0=blocked, 1=clear)
     */
    float calculate_visibility_at_position(const math::Vec3& position) const noexcept {
        // Check if position inside smoke bounds
        if (position.x < bounds_min.x || position.x > bounds_max.x ||
            position.y < bounds_min.y || position.y > bounds_max.y ||
            position.z < bounds_min.z || position.z > bounds_max.z) {
            return 1.0f; // Outside smoke
        }

        // Would sample density texture here
        // For now, estimate based on distance from source
        math::Vec3 to_source = position - source_position;
        float dist = math::length(to_source);

        if (dist > source_radius_m * 5.0f) return 1.0f;

        // Exponential falloff
        float density_estimate = std::exp(-dist / source_radius_m);
        float visibility = 1.0f - (density_estimate * opacity * density_scale);

        return math::clamp(visibility, 0.0f, 1.0f);
    }
};

} // namespace lore::ecs