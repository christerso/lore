#pragma once

#include "lore/math/vec3.hpp"
#include "lore/math/vec4.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace lore::ecs {

/**
 * @brief Volumetric fluid simulation using GPU Navier-Stokes solver
 *
 * State-of-the-art volumetric fluids for water, oil, blood, lava, acid.
 * Based on:
 * - NVIDIA PhysX Flex position-based fluid dynamics
 * - GPU Navier-Stokes solver with surface tracking
 * - Level-set method for free surface boundaries
 * - SPH (Smoothed Particle Hydrodynamics) for splashes
 * - Real-time performance (60 FPS target)
 *
 * Implementation uses 7 compute shaders:
 * 1. Advect: Move velocity/density through field
 * 2. AddForces: Apply gravity, buoyancy, external forces
 * 3. Divergence: Calculate velocity divergence
 * 4. Jacobi: Pressure solve iterations (incompressibility)
 * 5. Project: Make velocity field divergence-free
 * 6. SurfaceTrack: Update level-set for free surface
 * 7. ViscosityDiffuse: Apply viscosity (implicit solve)
 *
 * Extensive INI configuration:
 * @code{.ini}
 * [VolumetricFluid.Grid]
 * ResolutionX=128
 * ResolutionY=128
 * ResolutionZ=128
 * CellSize=0.05
 * BoundsExpansion=1.2
 *
 * [VolumetricFluid.Physical]
 * FluidDensity=1000.0
 * DynamicViscosity=0.001
 * SurfaceTension=0.0728
 * CompressibilityModulus=2.2e9
 * RestDensity=1000.0
 *
 * [VolumetricFluid.Simulation]
 * TimeStep=0.008
 * PressureIterations=50
 * ViscosityIterations=10
 * SurfaceThreshold=0.5
 * EnableSurfaceTension=true
 *
 * [VolumetricFluid.Rendering]
 * RefractionIndex=1.333
 * Absorption=0.2
 * EnableCaustics=true
 * EnableFoam=true
 * EnableRefraction=true
 * RaymarchSteps=64
 * @endcode
 *
 * Example usage:
 * @code
 * // Create water pool
 * Entity pool = world.create_entity();
 * world.add_component(pool, VolumetricFluidComponent::create_water_pool());
 *
 * // System updates fluid simulation on GPU
 * volumetric_fluid_system->update(world, delta_time);
 *
 * // Render uses 3D density texture with refraction
 * fluid_renderer->render_volumetric_fluid(fluid_component);
 * @endcode
 */
struct VolumetricFluidComponent {
    // Grid dimensions (power of 2 for GPU efficiency)
    uint32_t resolution_x = 128;
    uint32_t resolution_y = 128;
    uint32_t resolution_z = 128;

    float cell_size_m = 0.05f;     ///< Size of each voxel (meters)

    // World-space bounding box
    math::Vec3 bounds_min{-3.2f, 0.0f, -3.2f};
    math::Vec3 bounds_max{3.2f, 6.4f, 3.2f};

    // GPU texture handles (platform-specific, managed by system)
    uint32_t velocity_field_texture = 0;      ///< 3D velocity (R32G32B32F)
    uint32_t density_field_texture = 0;       ///< 3D fluid density (R32F)
    uint32_t pressure_field_texture = 0;      ///< 3D pressure (R32F)
    uint32_t temperature_field_texture = 0;   ///< 3D temperature (R32F)
    uint32_t viscosity_field_texture = 0;     ///< 3D viscosity (R32F)
    uint32_t levelset_field_texture = 0;      ///< 3D signed distance (R32F)
    uint32_t color_field_texture = 0;         ///< 3D fluid color (R32G32B32A32F)

    // Surface tracking (level-set method)
    float surface_threshold = 0.5f;           ///< Level-set iso-surface value
    bool enable_surface_tracking = true;      ///< Track free surface boundary
    uint32_t surface_rebuild_interval = 5;    ///< Frames between rebuilds

    // Physical properties (SI units)
    float fluid_density_kg_m3 = 1000.0f;      ///< Rest density (water = 1000)
    float dynamic_viscosity_pa_s = 0.001f;    ///< Dynamic viscosity (water = 0.001)
    float kinematic_viscosity_m2_s = 1.0e-6f; ///< ν = μ/ρ (water = 1e-6)
    float surface_tension_n_m = 0.0728f;      ///< Surface tension (water = 0.0728)
    float compressibility_modulus_pa = 2.2e9f; ///< Bulk modulus (water = 2.2 GPa)

    // Temperature properties
    float thermal_conductivity_w_m_k = 0.6f;  ///< Thermal conductivity (water)
    float specific_heat_j_kg_k = 4186.0f;     ///< Specific heat (water)
    float freezing_point_k = 273.15f;         ///< 0°C
    float boiling_point_k = 373.15f;          ///< 100°C
    float current_average_temp_k = 293.15f;   ///< 20°C

    // Simulation parameters
    float time_step_s = 0.008f;               ///< Simulation dt (1/125)
    float advection_dissipation = 0.995f;     ///< Momentum conservation (0-1)
    float velocity_dissipation = 0.99f;       ///< Velocity decay per step
    float density_dissipation = 1.0f;         ///< Fluid conservation (1.0 = perfect)

    // External forces
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};   ///< Gravity vector
    math::Vec3 wind_velocity{0.0f, 0.0f, 0.0f}; ///< Wind force
    float wind_influence = 0.1f;              ///< How much wind affects fluid

    // Pressure solver parameters
    uint32_t pressure_iterations = 50;        ///< Jacobi iterations (incompressibility)
    float pressure_relaxation = 1.9f;         ///< Over-relaxation factor (1-2)
    float divergence_tolerance = 0.0001f;     ///< Convergence threshold

    // Viscosity solver parameters
    uint32_t viscosity_iterations = 10;       ///< Viscosity diffusion iterations
    float viscosity_relaxation = 1.5f;        ///< Over-relaxation for viscosity

    // Vorticity confinement (adds turbulence/swirls)
    float vorticity_strength = 0.2f;          ///< Turbulence amount (0-1)
    bool enable_vorticity = true;             ///< Enable vortex preservation

    // Surface tension (creates droplets, meniscus)
    bool enable_surface_tension = true;
    float surface_tension_strength = 1.0f;    ///< Multiplier for surface tension
    float contact_angle_deg = 90.0f;          ///< Wetting angle (water-glass = 0°)
    float adhesion_force_n_m = 0.01f;         ///< Wall adhesion strength

    // Foam generation (at high turbulence/velocity)
    bool enable_foam_generation = true;
    float foam_threshold_velocity_m_s = 5.0f; ///< Velocity to create foam
    float foam_lifetime_s = 3.0f;             ///< How long foam bubbles last
    float foam_density_multiplier = 0.3f;     ///< Foam opacity vs fluid

    // Splash/droplet generation (SPH particles)
    bool enable_splash_particles = true;
    float splash_threshold_velocity_m_s = 3.0f;
    uint32_t max_splash_particles = 10000;
    float splash_particle_lifetime_s = 2.0f;
    float splash_particle_size_m = 0.02f;

    // Wave/ripple parameters
    bool enable_surface_waves = true;
    float wave_amplitude_m = 0.1f;            ///< Maximum wave height
    float wave_frequency_hz = 0.5f;           ///< Wave oscillation rate
    float wave_damping = 0.98f;               ///< Wave energy loss per frame

    // Rendering parameters
    float refraction_index = 1.333f;          ///< Index of refraction (water)
    math::Vec4 base_color{0.1f, 0.3f, 0.8f, 0.7f}; ///< RGBA fluid color
    float absorption_coefficient = 0.2f;      ///< Beer's law absorption
    float scattering_coefficient = 0.1f;      ///< Light scattering
    float fresnel_power = 5.0f;               ///< Fresnel effect strength
    bool enable_refraction = true;            ///< Refract scene behind fluid
    bool enable_caustics = true;              ///< Light caustics on geometry
    bool enable_subsurface_scattering = false; ///< Expensive light transport
    uint32_t raymarch_steps = 64;             ///< Ray sampling resolution
    uint32_t caustics_resolution = 512;       ///< Caustics texture size

    // Emission (for lava, glowing acid)
    bool is_emissive = false;
    float emission_strength = 0.0f;           ///< Light emission intensity
    math::Vec3 emission_color{1.0f, 0.5f, 0.0f}; ///< Emission color

    // Interaction properties
    float drag_coefficient = 1.0f;            ///< Drag on objects in fluid
    float buoyancy_strength = 1.0f;           ///< Archimedes force multiplier
    bool can_freeze = false;                  ///< Can solidify at freezing point
    bool can_evaporate = false;               ///< Can turn to vapor at boiling point

    // Performance settings
    bool enable_adaptive_timestep = true;     ///< Dynamic dt for stability
    float max_timestep_s = 0.016f;            ///< Max dt (60 FPS)
    float cfl_number = 0.8f;                  ///< Courant-Friedrichs-Lewy stability
    uint32_t substeps_per_frame = 1;          ///< Subdivide timestep for accuracy

    // LOD (Level of Detail)
    float lod_distance_full_m = 20.0f;        ///< Full resolution distance
    float lod_distance_medium_m = 50.0f;      ///< Half resolution distance
    float lod_distance_low_m = 100.0f;        ///< Quarter resolution distance
    float lod_resolution_scale_medium = 0.5f; ///< Medium LOD scale
    float lod_resolution_scale_low = 0.25f;   ///< Low LOD scale

    // Extensive INI-configurable parameters
    struct Config {
        // === Grid Configuration ===
        bool allow_dynamic_bounds = false;    ///< Adapt grid to fluid extent
        float bounds_expansion_factor = 1.2f; ///< Extra space around fluid
        bool clamp_to_bounds = true;          ///< Kill fluid outside grid
        bool enable_boundary_cells = true;    ///< Ghost cells for boundaries

        // === Simulation Quality ===
        float advection_accuracy = 1.0f;      ///< Higher = more accurate (slower)
        bool use_bfecc_advection = false;     ///< Back and Forth Error Compensation
        bool use_maccormack_advection = true; ///< MacCormack scheme (higher order)
        float maccormack_strength = 0.9f;     ///< Strength of correction (0-1)
        bool use_semi_lagrangian = true;      ///< Semi-Lagrangian advection

        // === Pressure Solve ===
        bool use_multigrid_solver = false;    ///< Multigrid for faster convergence
        bool use_conjugate_gradient = false;  ///< CG solver (more accurate)
        uint32_t multigrid_levels = 3;        ///< V-cycle depth
        float pressure_preconditioner = 1.0f; ///< Diagonal preconditioner

        // === Viscosity ===
        float temperature_viscosity_scale = 1.0f; ///< How temp affects viscosity
        bool enable_non_newtonian = false;    ///< Non-Newtonian fluids (blood)
        float shear_thinning_exponent = 1.0f; ///< Power-law fluid model
        float yield_stress_pa = 0.0f;         ///< Bingham plastic threshold

        // === Surface Tracking ===
        bool use_marching_cubes = true;       ///< Extract surface mesh
        bool use_narrow_band = true;          ///< Track surface in narrow band
        float narrow_band_width_m = 0.3f;     ///< Distance from surface
        uint32_t surface_smooth_iterations = 3; ///< Smooth surface mesh

        // === Surface Tension ===
        float curvature_flow_strength = 1.0f; ///< Curvature-driven surface flow
        bool enable_marangoni_effect = false; ///< Temperature gradient surface flow
        float surface_reconstruction_threshold = 0.1f; ///< Min feature size

        // === Foam ===
        float foam_diffusion_rate = 0.05f;    ///< Foam spread rate
        float foam_dissipation_rate = 0.95f;  ///< Foam decay per frame
        float foam_generation_rate_mult = 1.0f; ///< Foam spawn multiplier
        float foam_color_tint_r = 1.0f;       ///< Foam RGB tint
        float foam_color_tint_g = 1.0f;
        float foam_color_tint_b = 1.0f;
        float foam_particle_size_mult = 1.0f; ///< Foam bubble size

        // === Splash/Droplets ===
        float splash_angle_threshold_deg = 30.0f; ///< Angle for splash
        float splash_energy_threshold_j = 0.1f;   ///< Energy to create splash
        float droplet_merge_distance_m = 0.05f;   ///< Distance to merge droplets
        bool enable_droplet_collision = true;     ///< Droplets interact
        float splash_particle_drag = 0.99f;       ///< Air drag on droplets

        // === Waves ===
        float wave_propagation_speed_m_s = 1.0f; ///< Wave travel speed
        float wave_reflection_coefficient = 0.8f; ///< Boundary reflection
        bool enable_deep_water_waves = true;      ///< Dispersion relation
        bool enable_shallow_water_waves = false;  ///< Height-averaged

        // === Rendering Quality ===
        uint32_t raymarch_min_steps = 32;     ///< Minimum sampling
        uint32_t raymarch_max_steps = 128;    ///< Maximum sampling
        bool use_adaptive_raymarching = true; ///< Dynamic step count
        float raymarch_step_jitter = 0.5f;    ///< Dither to reduce banding
        bool enable_volumetric_shadows = true; ///< Self-shadowing
        uint32_t shadow_raymarch_steps = 16;  ///< Shadow ray samples

        // === Refraction ===
        float refraction_roughness = 0.0f;    ///< Surface roughness (0-1)
        float chromatic_aberration = 0.01f;   ///< RGB split in refraction
        bool enable_total_internal_reflection = true; ///< TIR at grazing angles
        float refraction_blur_samples = 8;    ///< Blur samples for rough refraction

        // === Caustics ===
        float caustics_intensity = 1.0f;      ///< Caustics brightness
        float caustics_scale = 1.0f;          ///< Caustics pattern scale
        bool use_photon_mapping = false;      ///< Accurate but expensive
        uint32_t caustics_photon_count = 10000; ///< Photons for caustics

        // === Subsurface Scattering ===
        float sss_distance_m = 0.1f;          ///< SSS penetration depth
        math::Vec3 sss_color{0.9f, 0.9f, 1.0f}; ///< SSS tint
        uint32_t sss_samples = 8;             ///< SSS ray samples

        // === Thermal Interaction ===
        bool enable_thermal_convection = true; ///< Heat causes fluid flow
        float thermal_expansion_coeff = 0.0002f; ///< Volume change per K
        float heat_transfer_rate = 1.0f;      ///< Heat exchange multiplier
        bool enable_phase_transitions = false; ///< Freeze/evaporate

        // === Chemical Interaction ===
        bool can_dissolve_materials = false;  ///< Acid dissolving objects
        float dissolution_rate_kg_m2_s = 0.0f; ///< Dissolution speed
        bool can_react_with_fire = false;     ///< Water extinguishes fire
        float fire_suppression_effectiveness = 1.0f; ///< Extinguish multiplier

        // === Performance ===
        bool use_compute_shaders = true;      ///< GPU compute (vs pixel shader)
        bool enable_async_compute = true;     ///< Overlap GPU work
        bool enable_gpu_culling = true;       ///< Cull empty regions
        float culling_density_threshold = 0.01f; ///< Min density to render
        uint32_t max_visible_fluids = 10;     ///< Screen-space limit
        bool enable_temporal_reprojection = true; ///< Reuse prev frame
        float temporal_blend_factor = 0.9f;   ///< Blend with history (0-1)
        bool enable_half_precision = false;   ///< Use FP16 for performance

        // === Debug ===
        bool visualize_velocity_field = false; ///< Show arrows
        bool visualize_pressure_field = false; ///< Show pressure colors
        bool visualize_surface = false;        ///< Show level-set surface
        bool visualize_vorticity = false;      ///< Show vortex cores
        bool visualize_foam = false;           ///< Show foam density
        float debug_visualization_scale = 1.0f; ///< Debug display scale
    } config;

    /**
     * @brief Create water pool preset
     */
    static VolumetricFluidComponent create_water_pool() noexcept {
        VolumetricFluidComponent fluid;
        fluid.resolution_x = 128;
        fluid.resolution_y = 64;
        fluid.resolution_z = 128;
        fluid.cell_size_m = 0.05f;
        fluid.bounds_min = {-3.2f, 0.0f, -3.2f};
        fluid.bounds_max = {3.2f, 3.2f, 3.2f};

        // Water properties
        fluid.fluid_density_kg_m3 = 1000.0f;
        fluid.dynamic_viscosity_pa_s = 0.001f;
        fluid.kinematic_viscosity_m2_s = 1.0e-6f;
        fluid.surface_tension_n_m = 0.0728f;
        fluid.refraction_index = 1.333f;
        fluid.base_color = {0.1f, 0.3f, 0.8f, 0.7f};

        fluid.enable_surface_waves = true;
        fluid.enable_foam_generation = true;
        fluid.enable_caustics = true;
        fluid.can_freeze = true;
        fluid.can_evaporate = true;

        return fluid;
    }

    /**
     * @brief Create oil spill preset
     */
    static VolumetricFluidComponent create_oil_spill() noexcept {
        VolumetricFluidComponent fluid;
        fluid.resolution_x = 128;
        fluid.resolution_y = 32;
        fluid.resolution_z = 128;
        fluid.cell_size_m = 0.08f;
        fluid.bounds_min = {-5.0f, 0.0f, -5.0f};
        fluid.bounds_max = {5.0f, 2.5f, 5.0f};

        // Oil properties
        fluid.fluid_density_kg_m3 = 850.0f;
        fluid.dynamic_viscosity_pa_s = 0.05f;  // Much more viscous than water
        fluid.kinematic_viscosity_m2_s = 5.9e-5f;
        fluid.surface_tension_n_m = 0.032f;    // Lower than water
        fluid.refraction_index = 1.47f;
        fluid.base_color = {0.1f, 0.08f, 0.05f, 0.9f}; // Dark brown

        fluid.enable_surface_waves = false;
        fluid.enable_foam_generation = false;
        fluid.enable_caustics = true;
        fluid.can_freeze = false;
        fluid.can_evaporate = false;

        fluid.config.enable_non_newtonian = true;
        fluid.config.shear_thinning_exponent = 0.8f;

        return fluid;
    }

    /**
     * @brief Create blood preset
     */
    static VolumetricFluidComponent create_blood() noexcept {
        VolumetricFluidComponent fluid;
        fluid.resolution_x = 64;
        fluid.resolution_y = 64;
        fluid.resolution_z = 64;
        fluid.cell_size_m = 0.03f;
        fluid.bounds_min = {-1.0f, 0.0f, -1.0f};
        fluid.bounds_max = {1.0f, 2.0f, 1.0f};

        // Blood properties (non-Newtonian)
        fluid.fluid_density_kg_m3 = 1060.0f;
        fluid.dynamic_viscosity_pa_s = 0.004f; // 4x water viscosity
        fluid.kinematic_viscosity_m2_s = 3.8e-6f;
        fluid.surface_tension_n_m = 0.058f;
        fluid.refraction_index = 1.35f;
        fluid.base_color = {0.6f, 0.05f, 0.05f, 0.95f}; // Dark red

        fluid.enable_surface_waves = false;
        fluid.enable_foam_generation = false;
        fluid.enable_caustics = false;
        fluid.enable_subsurface_scattering = true;
        fluid.can_freeze = false;
        fluid.can_evaporate = false;

        fluid.config.enable_non_newtonian = true;
        fluid.config.shear_thinning_exponent = 0.7f;
        fluid.config.sss_distance_m = 0.05f;
        fluid.config.sss_color = {1.0f, 0.3f, 0.3f};

        return fluid;
    }

    /**
     * @brief Create acid preset (corrosive, glowing)
     */
    static VolumetricFluidComponent create_acid() noexcept {
        VolumetricFluidComponent fluid;
        fluid.resolution_x = 64;
        fluid.resolution_y = 64;
        fluid.resolution_z = 64;
        fluid.cell_size_m = 0.05f;
        fluid.bounds_min = {-1.5f, 0.0f, -1.5f};
        fluid.bounds_max = {1.5f, 3.0f, 1.5f};

        // Acid properties
        fluid.fluid_density_kg_m3 = 1200.0f;
        fluid.dynamic_viscosity_pa_s = 0.002f;
        fluid.kinematic_viscosity_m2_s = 1.7e-6f;
        fluid.surface_tension_n_m = 0.065f;
        fluid.refraction_index = 1.38f;
        fluid.base_color = {0.2f, 0.8f, 0.2f, 0.8f}; // Green

        fluid.is_emissive = true;
        fluid.emission_strength = 2.0f;
        fluid.emission_color = {0.3f, 1.0f, 0.3f};

        fluid.enable_surface_waves = true;
        fluid.enable_foam_generation = true;
        fluid.enable_caustics = true;
        fluid.can_freeze = false;
        fluid.can_evaporate = true;

        fluid.config.can_dissolve_materials = true;
        fluid.config.dissolution_rate_kg_m2_s = 0.01f;

        return fluid;
    }

    /**
     * @brief Create lava preset (very high viscosity, emissive)
     */
    static VolumetricFluidComponent create_lava() noexcept {
        VolumetricFluidComponent fluid;
        fluid.resolution_x = 64;
        fluid.resolution_y = 48;
        fluid.resolution_z = 64;
        fluid.cell_size_m = 0.1f;
        fluid.bounds_min = {-3.0f, 0.0f, -3.0f};
        fluid.bounds_max = {3.0f, 4.8f, 3.0f};

        // Lava properties
        fluid.fluid_density_kg_m3 = 3100.0f;
        fluid.dynamic_viscosity_pa_s = 100.0f; // Extremely viscous
        fluid.kinematic_viscosity_m2_s = 0.032f;
        fluid.surface_tension_n_m = 0.4f;      // High surface tension
        fluid.refraction_index = 1.6f;
        fluid.base_color = {1.0f, 0.3f, 0.05f, 0.95f}; // Orange-red
        fluid.current_average_temp_k = 1473.15f; // 1200°C

        fluid.is_emissive = true;
        fluid.emission_strength = 10.0f;
        fluid.emission_color = {1.0f, 0.5f, 0.1f};

        fluid.enable_surface_waves = false;
        fluid.enable_foam_generation = false;
        fluid.enable_caustics = false;
        fluid.enable_splash_particles = false;
        fluid.can_freeze = false;
        fluid.can_evaporate = false;

        fluid.viscosity_iterations = 20;       // More iterations for high viscosity
        fluid.config.enable_non_newtonian = true;
        fluid.config.shear_thinning_exponent = 0.5f;

        return fluid;
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
        // 7 fields: velocity (12), density (4), pressure (4), temp (4), viscosity (4), levelset (4), color (16)
        return cells * (12 + 4 + 4 + 4 + 4 + 4 + 16);
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

    /**
     * @brief Calculate buoyancy force on object
     *
     * Archimedes' principle: F_buoyancy = ρ_fluid * V_displaced * g
     *
     * @param object_volume_m3 Volume of submerged object (m³)
     * @return Buoyancy force vector (Newtons)
     */
    math::Vec3 calculate_buoyancy_force(float object_volume_m3) const noexcept {
        float buoyancy_magnitude_n = fluid_density_kg_m3 * object_volume_m3 * 9.81f;
        return math::Vec3{0.0f, buoyancy_magnitude_n * buoyancy_strength, 0.0f};
    }

    /**
     * @brief Calculate drag force on object
     *
     * Drag: F_drag = 0.5 * ρ * v² * C_d * A
     *
     * @param object_velocity_m_s Object velocity relative to fluid (m/s)
     * @param cross_section_area_m2 Object cross-sectional area (m²)
     * @return Drag force vector (Newtons)
     */
    math::Vec3 calculate_drag_force(
        const math::Vec3& object_velocity_m_s,
        float cross_section_area_m2
    ) const noexcept {
        float speed = object_velocity_m_s.length();
        if (speed < 1e-6f) return {0.0f, 0.0f, 0.0f};

        float drag_magnitude = 0.5f * fluid_density_kg_m3 * speed * speed *
                              drag_coefficient * cross_section_area_m2;

        math::Vec3 drag_direction = object_velocity_m_s * (-1.0f / speed);
        return drag_direction * drag_magnitude;
    }

    /**
     * @brief Check if fluid is frozen
     */
    bool is_frozen() const noexcept {
        return can_freeze && current_average_temp_k <= freezing_point_k;
    }

    /**
     * @brief Check if fluid is boiling
     */
    bool is_boiling() const noexcept {
        return can_evaporate && current_average_temp_k >= boiling_point_k;
    }

    /**
     * @brief Calculate Reynolds number for flow
     *
     * Re = ρ * v * L / μ
     * Re < 2000: laminar flow
     * Re > 4000: turbulent flow
     *
     * @param characteristic_velocity_m_s Flow velocity (m/s)
     * @param characteristic_length_m Flow length scale (m)
     * @return Reynolds number (dimensionless)
     */
    float calculate_reynolds_number(
        float characteristic_velocity_m_s,
        float characteristic_length_m
    ) const noexcept {
        return fluid_density_kg_m3 * characteristic_velocity_m_s *
               characteristic_length_m / dynamic_viscosity_pa_s;
    }
};

} // namespace lore::ecs