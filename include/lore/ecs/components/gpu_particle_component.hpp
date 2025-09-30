#pragma once

#include "lore/core/math_types.hpp"
#include <vector>
#include <string>

namespace lore::ecs {

/**
 * @brief GPU-accelerated particle system component
 *
 * Manages particles entirely on GPU using compute shaders.
 * Integrates with fire, smoke, and explosion systems.
 *
 * Particle Types:
 * - Smoke puffs (billboards, rise with buoyancy)
 * - Embers (glowing, gravity-affected, fade out)
 * - Sparks (fast, short-lived, trail)
 * - Debris (chunks, physics-based)
 * - Magic effects (custom behavior)
 *
 * Physics:
 * - Euler integration: p_new = p + v*dt, v_new = v + a*dt
 * - Drag force: F_drag = -k*v²
 * - Buoyancy: F = (T - T_ambient) * α * (-g)
 * - Wind advection
 * - Collision with world
 *
 * Performance:
 * - 100k particles: ~1.5ms (compute + render)
 * - GPU instancing for rendering
 * - Async compute (overlaps with rendering)
 * - Particle pool recycling (zero allocations)
 *
 * Usage:
 * @code
 * // Create ember particle system
 * auto particle_entity = world.create_entity();
 * auto& particles = world.add_component<GPUParticleComponent>(
 *     particle_entity,
 *     GPUParticleComponent::create_embers()
 * );
 *
 * // Emit particles from fire
 * particles.spawn_position = fire_position;
 * particles.spawn_rate = 100.0f;  // 100 particles/sec
 *
 * // System updates automatically
 * particle_system.update(world, delta_time);
 * @endcode
 */
struct GPUParticleComponent {
    /**
     * @brief Particle type
     */
    enum class Type {
        Smoke,      // Billboards, rise, integrate with VolumetricSmokeComponent
        Embers,     // Glowing orange/red, fall with gravity, fade
        Sparks,     // Fast, short-lived, trail, bright white/yellow
        Debris,     // Solid chunks, physics-based, bounce
        Magic,      // Custom effects (fire, ice, lightning, etc.)
    };

    /**
     * @brief Rendering mode
     */
    enum class RenderMode {
        Billboard,      // Always face camera
        Stretched,      // Stretch along velocity (sparks, trails)
        Mesh,           // Render small mesh (debris)
        Trail,          // Leave trail behind (sparks, magic)
    };

    // ========================================================================
    // TYPE AND BEHAVIOR
    // ========================================================================

    Type type = Type::Smoke;
    RenderMode render_mode = RenderMode::Billboard;

    /**
     * @brief Maximum particles in system
     * Once full, oldest particles are recycled
     */
    uint32_t max_particles = 10000;

    /**
     * @brief Current active particle count (GPU-managed)
     */
    uint32_t active_particles = 0;

    // ========================================================================
    // SPAWNING
    // ========================================================================

    /**
     * @brief World position to spawn particles
     */
    math::Vec3 spawn_position{0.0f, 0.0f, 0.0f};

    /**
     * @brief Spawn radius (particles spawn within sphere)
     */
    float spawn_radius = 0.1f;

    /**
     * @brief Spawn rate (particles per second)
     * 0 = burst mode (manual emission)
     */
    float spawn_rate = 10.0f;

    /**
     * @brief Spawn velocity (initial velocity direction and magnitude)
     */
    math::Vec3 spawn_velocity{0.0f, 1.0f, 0.0f};  // Upward

    /**
     * @brief Velocity randomness (0-1)
     * 0 = all particles same velocity
     * 1 = fully random direction/magnitude
     */
    float velocity_randomness = 0.3f;

    /**
     * @brief Spawn angular velocity (radians/second)
     * For rotating particles (debris)
     */
    float spawn_angular_velocity = 0.0f;

    // ========================================================================
    // LIFETIME
    // ========================================================================

    /**
     * @brief Particle lifetime (seconds)
     */
    float lifetime_min = 1.0f;
    float lifetime_max = 3.0f;

    /**
     * @brief Fade in/out duration (seconds)
     */
    float fade_in_duration = 0.2f;
    float fade_out_duration = 0.5f;

    // ========================================================================
    // PHYSICS
    // ========================================================================

    /**
     * @brief Gravity force (m/s²)
     * Negative for down, positive for up
     */
    float gravity = -9.8f;

    /**
     * @brief Drag coefficient (air resistance)
     * Higher = particles slow down faster
     */
    float drag = 0.1f;

    /**
     * @brief Buoyancy strength (for smoke particles)
     * Based on temperature difference
     */
    float buoyancy_strength = 1.0f;

    /**
     * @brief Particle mass (kg, for physics)
     */
    float particle_mass = 0.001f;  // 1 gram

    /**
     * @brief Wind velocity (m/s)
     */
    math::Vec3 wind_velocity{0.0f, 0.0f, 0.0f};

    /**
     * @brief Wind turbulence (0-1)
     * Adds noise to wind force
     */
    float wind_turbulence = 0.0f;

    /**
     * @brief Enable collision with world
     */
    bool enable_collision = false;

    /**
     * @brief Bounce factor (0-1)
     * 0 = stick, 1 = perfect bounce
     */
    float bounce_factor = 0.3f;

    // ========================================================================
    // VISUAL APPEARANCE
    // ========================================================================

    /**
     * @brief Start color (RGB, linear)
     */
    math::Vec3 color_start{1.0f, 1.0f, 1.0f};

    /**
     * @brief End color (RGB, linear)
     * Particles lerp from start to end over lifetime
     */
    math::Vec3 color_end{0.5f, 0.5f, 0.5f};

    /**
     * @brief Opacity (0-1)
     */
    float opacity = 1.0f;

    /**
     * @brief Emissive intensity (for glowing particles like embers)
     * 0 = not emissive, >1 = glowing
     */
    float emissive_intensity = 0.0f;

    /**
     * @brief Particle size (meters)
     */
    float size_start = 0.1f;
    float size_end = 0.3f;

    /**
     * @brief Size randomness (0-1)
     */
    float size_randomness = 0.2f;

    /**
     * @brief Texture atlas index (for sprite sheets)
     * 0-based index into texture atlas
     */
    uint32_t texture_index = 0;

    /**
     * @brief Texture animation (0 = no animation)
     * Number of frames to animate through over lifetime
     */
    uint32_t texture_animation_frames = 0;

    /**
     * @brief Billboard rotation (radians)
     * Only for billboard render mode
     */
    float rotation_start = 0.0f;
    float rotation_end = 0.0f;
    float rotation_speed = 0.0f;  // Radians/second

    // ========================================================================
    // INTEGRATION
    // ========================================================================

    /**
     * @brief Link to fire entity (for embers/sparks)
     * If set, particles spawn from fire source
     */
    Entity fire_source_entity = INVALID_ENTITY;

    /**
     * @brief Link to smoke volume (for smoke particles)
     * If set, particles feed into volumetric smoke
     */
    Entity smoke_volume_entity = INVALID_ENTITY;

    /**
     * @brief Particle temperature (kelvin, for smoke/embers)
     * Used for smoke integration and ember glow
     */
    float particle_temperature_k = 293.15f;  // 20°C default

    /**
     * @brief Temperature decay rate (K/s)
     * Embers cool down over time
     */
    float temperature_decay_rate = 100.0f;

    // ========================================================================
    // GPU RESOURCES
    // ========================================================================

    /**
     * @brief GPU particle buffer (positions, velocities, ages, etc.)
     * Managed by GPUParticleSystem
     */
    uint32_t particle_buffer = 0;

    /**
     * @brief Indirect draw buffer (for GPU instancing)
     */
    uint32_t indirect_draw_buffer = 0;

    /**
     * @brief Texture handle (if using textures)
     */
    uint32_t texture = 0;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief LOD level (0 = high, 2 = low)
     */
    uint32_t lod_level = 0;

    /**
     * @brief Update rate (Hz)
     * Lower for distant particles
     */
    float update_rate_hz = 60.0f;

    /**
     * @brief Sorting (for transparency)
     * true = sort by depth (slower, but correct blending)
     * false = no sorting (faster, but possible artifacts)
     */
    bool enable_sorting = true;

    // ========================================================================
    // STATIC FACTORY METHODS
    // ========================================================================

    /**
     * @brief Smoke puffs (integrate with volumetric smoke)
     */
    static GPUParticleComponent create_smoke_puffs();

    /**
     * @brief Embers (glowing, falling)
     */
    static GPUParticleComponent create_embers();

    /**
     * @brief Sparks (fast, bright, short-lived)
     */
    static GPUParticleComponent create_sparks();

    /**
     * @brief Debris (chunks from explosions)
     */
    static GPUParticleComponent create_debris();

    /**
     * @brief Magic fire (custom fire effect)
     */
    static GPUParticleComponent create_magic_fire();

    /**
     * @brief Steam (white, rising)
     */
    static GPUParticleComponent create_steam();

    /**
     * @brief Dust (slow-falling, lit by environment)
     */
    static GPUParticleComponent create_dust();
};

} // namespace lore::ecs