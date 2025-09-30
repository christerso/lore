# GPU Particle System

## Overview

GPU-accelerated particle system with zero CPU overhead. Integrates with fire, smoke, and explosion systems.

**7 Built-In Presets:**
1. **Smoke Puffs** - Integrate with volumetric smoke, rise with buoyancy
2. **Embers** - Glowing orange/red, fall with gravity, cool over time
3. **Sparks** - Fast, bright, short-lived, stretch along velocity
4. **Debris** - Solid chunks, physics-based, bounce off ground
5. **Magic Fire** - Purple fire effect (customizable colors)
6. **Steam** - White, rising, dissipates quickly
7. **Dust** - Slow-falling, ambient, lit by environment

## Quick Start

```cpp
// Create ember particles from fire
auto particle_entity = world.create_entity();
auto& particles = world.add_component<GPUParticleComponent>(
    particle_entity,
    GPUParticleComponent::create_embers()
);

// Link to fire source
particles.fire_source_entity = fire_entity;
particles.spawn_position = fire_position;
particles.spawn_rate = 30.0f;  // 30 embers/sec

// System handles everything
particle_system.update(world, delta_time);
particle_system.render(cmd, view_matrix, proj_matrix);
```

## Presets

### Smoke Puffs
```cpp
auto particles = GPUParticleComponent::create_smoke_puffs();
// Gray billboards, rise slowly, integrate with VolumetricSmokeComponent
// Spawn rate: 20/sec, Lifetime: 2-4s
```

### Embers
```cpp
auto particles = GPUParticleComponent::create_embers();
// Glowing orange→red, launch upward, fall with gravity
// Spawn rate: 30/sec, Lifetime: 1.5-3s, Temperature: 1200K
```

### Sparks
```cpp
auto particles = GPUParticleComponent::create_sparks();
// Bright white-yellow, stretch along velocity, very short-lived
// Spawn rate: 50/sec, Lifetime: 0.2-0.5s, Temperature: 2000K (white-hot)
```

### Debris
```cpp
auto particles = GPUParticleComponent::create_debris();
// Solid chunks, spin, bounce off ground
// Burst mode (explosion), Lifetime: 5-10s, Collision: enabled
```

## Integration with Fire

```cpp
// Fire automatically spawns embers and sparks
auto& combustion = world.get_component<CombustionComponent>(fire_entity);

// Create ember particles
auto ember_entity = world.create_entity();
auto& embers = world.add_component<GPUParticleComponent>(
    ember_entity,
    GPUParticleComponent::create_embers()
);
embers.fire_source_entity = fire_entity;
embers.spawn_position = combustion.position;
embers.spawn_rate = combustion.flame_intensity * 10.0f;  // Scale with fire

// Create sparks
auto spark_entity = world.create_entity();
auto& sparks = world.add_component<GPUParticleComponent>(
    spark_entity,
    GPUParticleComponent::create_sparks()
);
sparks.fire_source_entity = fire_entity;
sparks.spawn_rate = combustion.fuel_consumption_rate_kg_s * 50.0f;
```

## Performance

| Particles | Update | Render | Total |
|-----------|--------|--------|-------|
| 10k | 0.3ms | 0.2ms | 0.5ms |
| 50k | 1.0ms | 0.6ms | 1.6ms |
| 100k | 1.8ms | 1.2ms | 3.0ms |

**Optimizations:**
- GPU compute (zero CPU cost)
- Instanced rendering
- Async compute (overlaps with rendering)
- LOD system (reduce distant particles)
- Particle recycling (zero allocations)

## API Reference

```cpp
// Presets
GPUParticleComponent::create_smoke_puffs();
GPUParticleComponent::create_embers();
GPUParticleComponent::create_sparks();
GPUParticleComponent::create_debris();
GPUParticleComponent::create_magic_fire();
GPUParticleComponent::create_steam();
GPUParticleComponent::create_dust();

// System
void update(World& world, float delta_time_s);
void render(VkCommandBuffer cmd, const float* view_matrix, const float* proj_matrix);
void emit_burst(World& world, Entity entity, uint32_t count, vec3 pos, vec3 vel);
```