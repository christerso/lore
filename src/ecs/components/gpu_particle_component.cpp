#include "lore/ecs/components/gpu_particle_component.hpp"

namespace lore::ecs {

GPUParticleComponent GPUParticleComponent::create_smoke_puffs() {
    GPUParticleComponent particles;
    particles.type = Type::Smoke;
    particles.render_mode = RenderMode::Billboard;

    // Spawning
    particles.max_particles = 5000;
    particles.spawn_radius = 0.2f;
    particles.spawn_rate = 20.0f;  // 20 puffs/sec
    particles.spawn_velocity = {0.0f, 0.5f, 0.0f};  // Slow rise
    particles.velocity_randomness = 0.4f;

    // Lifetime
    particles.lifetime_min = 2.0f;
    particles.lifetime_max = 4.0f;
    particles.fade_in_duration = 0.3f;
    particles.fade_out_duration = 1.0f;

    // Physics
    particles.gravity = 0.0f;  // Smoke is buoyant
    particles.drag = 0.3f;  // High drag (smoke dissipates quickly)
    particles.buoyancy_strength = 0.8f;
    particles.particle_mass = 0.0001f;  // Very light

    // Visual
    particles.color_start = {0.3f, 0.3f, 0.35f};  // Gray
    particles.color_end = {0.15f, 0.15f, 0.18f};  // Darker gray
    particles.opacity = 0.6f;
    particles.emissive_intensity = 0.0f;
    particles.size_start = 0.1f;
    particles.size_end = 0.5f;  // Expands as it rises
    particles.size_randomness = 0.3f;
    particles.rotation_speed = 0.5f;  // Slow rotation

    // Temperature
    particles.particle_temperature_k = 350.0f;  // Warm smoke
    particles.temperature_decay_rate = 50.0f;

    particles.update_rate_hz = 30.0f;  // Smoke is slow-moving

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_embers() {
    GPUParticleComponent particles;
    particles.type = Type::Embers;
    particles.render_mode = RenderMode::Billboard;

    // Spawning
    particles.max_particles = 2000;
    particles.spawn_radius = 0.15f;
    particles.spawn_rate = 30.0f;  // 30 embers/sec
    particles.spawn_velocity = {0.0f, 2.0f, 0.0f};  // Launch upward
    particles.velocity_randomness = 0.6f;

    // Lifetime
    particles.lifetime_min = 1.5f;
    particles.lifetime_max = 3.0f;
    particles.fade_in_duration = 0.1f;
    particles.fade_out_duration = 0.8f;

    // Physics
    particles.gravity = -2.0f;  // Gravity affects embers
    particles.drag = 0.2f;
    particles.buoyancy_strength = 0.3f;  // Some buoyancy (hot)
    particles.particle_mass = 0.0005f;

    // Visual (glowing orange/red)
    particles.color_start = {1.0f, 0.6f, 0.2f};  // Bright orange
    particles.color_end = {0.3f, 0.05f, 0.0f};  // Dark red
    particles.opacity = 1.0f;
    particles.emissive_intensity = 3.0f;  // Bright glow
    particles.size_start = 0.02f;
    particles.size_end = 0.01f;  // Shrinks as it cools
    particles.size_randomness = 0.4f;

    // Temperature
    particles.particle_temperature_k = 1200.0f;  // Hot!
    particles.temperature_decay_rate = 300.0f;  // Cools quickly

    particles.update_rate_hz = 60.0f;  // Fast-moving

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_sparks() {
    GPUParticleComponent particles;
    particles.type = Type::Sparks;
    particles.render_mode = RenderMode::Stretched;  // Stretch along velocity

    // Spawning
    particles.max_particles = 1000;
    particles.spawn_radius = 0.05f;
    particles.spawn_rate = 50.0f;  // Burst of sparks
    particles.spawn_velocity = {0.0f, 3.0f, 0.0f};  // Fast launch
    particles.velocity_randomness = 0.8f;  // Very random

    // Lifetime (very short)
    particles.lifetime_min = 0.2f;
    particles.lifetime_max = 0.5f;
    particles.fade_in_duration = 0.0f;
    particles.fade_out_duration = 0.15f;

    // Physics
    particles.gravity = -9.8f;  // Full gravity
    particles.drag = 0.1f;
    particles.buoyancy_strength = 0.0f;
    particles.particle_mass = 0.0001f;

    // Visual (bright white/yellow)
    particles.color_start = {1.0f, 0.95f, 0.8f};  // Bright white-yellow
    particles.color_end = {1.0f, 0.5f, 0.0f};  // Orange
    particles.opacity = 1.0f;
    particles.emissive_intensity = 8.0f;  // Very bright!
    particles.size_start = 0.015f;
    particles.size_end = 0.005f;
    particles.size_randomness = 0.3f;

    // Temperature
    particles.particle_temperature_k = 2000.0f;  // Very hot (white-hot)
    particles.temperature_decay_rate = 1000.0f;  // Cools very quickly

    particles.update_rate_hz = 60.0f;  // Fast-moving

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_debris() {
    GPUParticleComponent particles;
    particles.type = Type::Debris;
    particles.render_mode = RenderMode::Mesh;  // Render small mesh

    // Spawning
    particles.max_particles = 500;
    particles.spawn_radius = 0.3f;
    particles.spawn_rate = 0.0f;  // Burst mode (explosion spawns all at once)
    particles.spawn_velocity = {0.0f, 5.0f, 0.0f};
    particles.velocity_randomness = 0.9f;  // Very random directions
    particles.spawn_angular_velocity = 10.0f;  // Spinning chunks

    // Lifetime
    particles.lifetime_min = 5.0f;
    particles.lifetime_max = 10.0f;
    particles.fade_in_duration = 0.0f;
    particles.fade_out_duration = 1.0f;

    // Physics
    particles.gravity = -9.8f;
    particles.drag = 0.05f;  // Low drag (solid chunks)
    particles.buoyancy_strength = 0.0f;
    particles.particle_mass = 0.1f;  // Heavy chunks
    particles.enable_collision = true;  // Bounce off ground
    particles.bounce_factor = 0.3f;

    // Visual
    particles.color_start = {0.4f, 0.4f, 0.4f};  // Gray chunks
    particles.color_end = {0.3f, 0.3f, 0.3f};
    particles.opacity = 1.0f;
    particles.emissive_intensity = 0.0f;
    particles.size_start = 0.05f;
    particles.size_end = 0.05f;  // Constant size
    particles.size_randomness = 0.5f;

    particles.enable_sorting = false;  // Opaque, no sorting needed

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_magic_fire() {
    GPUParticleComponent particles;
    particles.type = Type::Magic;
    particles.render_mode = RenderMode::Billboard;

    // Spawning
    particles.max_particles = 3000;
    particles.spawn_radius = 0.1f;
    particles.spawn_rate = 40.0f;
    particles.spawn_velocity = {0.0f, 1.5f, 0.0f};
    particles.velocity_randomness = 0.5f;

    // Lifetime
    particles.lifetime_min = 1.0f;
    particles.lifetime_max = 2.0f;
    particles.fade_in_duration = 0.2f;
    particles.fade_out_duration = 0.5f;

    // Physics
    particles.gravity = -1.0f;  // Reduced gravity (magical)
    particles.drag = 0.4f;
    particles.buoyancy_strength = 1.0f;
    particles.particle_mass = 0.0002f;

    // Visual (purple magic fire)
    particles.color_start = {0.8f, 0.2f, 1.0f};  // Purple
    particles.color_end = {0.2f, 0.0f, 0.5f};  // Dark purple
    particles.opacity = 0.8f;
    particles.emissive_intensity = 5.0f;
    particles.size_start = 0.08f;
    particles.size_end = 0.15f;
    particles.size_randomness = 0.3f;
    particles.rotation_speed = 2.0f;

    // Temperature
    particles.particle_temperature_k = 800.0f;
    particles.temperature_decay_rate = 200.0f;

    particles.update_rate_hz = 60.0f;

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_steam() {
    GPUParticleComponent particles;
    particles.type = Type::Smoke;
    particles.render_mode = RenderMode::Billboard;

    // Spawning
    particles.max_particles = 2000;
    particles.spawn_radius = 0.15f;
    particles.spawn_rate = 25.0f;
    particles.spawn_velocity = {0.0f, 1.0f, 0.0f};
    particles.velocity_randomness = 0.4f;

    // Lifetime (steam dissipates quickly)
    particles.lifetime_min = 1.0f;
    particles.lifetime_max = 2.5f;
    particles.fade_in_duration = 0.2f;
    particles.fade_out_duration = 0.6f;

    // Physics
    particles.gravity = 0.0f;
    particles.drag = 0.4f;  // High drag
    particles.buoyancy_strength = 1.2f;  // Rises quickly
    particles.particle_mass = 0.00005f;  // Very light

    // Visual (white)
    particles.color_start = {0.95f, 0.95f, 1.0f};  // White-blue
    particles.color_end = {0.8f, 0.8f, 0.85f};  // Light gray
    particles.opacity = 0.5f;
    particles.emissive_intensity = 0.0f;
    particles.size_start = 0.1f;
    particles.size_end = 0.4f;
    particles.size_randomness = 0.3f;

    // Temperature
    particles.particle_temperature_k = 373.15f;  // 100°C (boiling)
    particles.temperature_decay_rate = 100.0f;

    particles.update_rate_hz = 30.0f;

    return particles;
}

GPUParticleComponent GPUParticleComponent::create_dust() {
    GPUParticleComponent particles;
    particles.type = Type::Debris;
    particles.render_mode = RenderMode::Billboard;

    // Spawning
    particles.max_particles = 10000;
    particles.spawn_radius = 1.0f;  // Large area
    particles.spawn_rate = 10.0f;  // Slow spawning
    particles.spawn_velocity = {0.0f, 0.05f, 0.0f};  // Very slow
    particles.velocity_randomness = 0.2f;

    // Lifetime (dust lingers)
    particles.lifetime_min = 10.0f;
    particles.lifetime_max = 30.0f;
    particles.fade_in_duration = 1.0f;
    particles.fade_out_duration = 3.0f;

    // Physics
    particles.gravity = -0.5f;  // Slow fall
    particles.drag = 0.8f;  // Very high drag
    particles.buoyancy_strength = 0.0f;
    particles.particle_mass = 0.00001f;

    // Visual (lit by environment)
    particles.color_start = {0.6f, 0.6f, 0.5f};  // Beige
    particles.color_end = {0.5f, 0.5f, 0.45f};
    particles.opacity = 0.3f;  // Very translucent
    particles.emissive_intensity = 0.0f;
    particles.size_start = 0.01f;
    particles.size_end = 0.01f;  // Constant tiny size
    particles.size_randomness = 0.5f;

    particles.update_rate_hz = 20.0f;  // Slow-moving
    particles.enable_sorting = true;  // Sort for proper blending

    return particles;
}

} // namespace lore::ecs