# Thermal & Combustion System Integration Guide

## Overview

The Thermal and Combustion systems provide realistic fire, heat transfer, and material burning simulation for the Lore Engine. These systems work together with the physics, chemistry, and anatomy systems to create a comprehensive thermodynamics simulation.

## System Architecture

```
Ballistics Impact
     ↓
[Kinetic Energy → Thermal Energy]
     ↓
ThermalSystem
     ├─→ Heat Conduction (between entities)
     ├─→ Thermal Radiation (Stefan-Boltzmann)
     ├─→ Convection (to air)
     ├─→ Phase Transitions (melting/boiling)
     ├─→ Ignition Check (temperature ≥ ignition point)
     └─→ Thermal Damage (to anatomy)
         ↓
CombustionSystem
     ├─→ Fuel Consumption
     ├─→ Heat Generation
     ├─→ Oxygen Consumption
     ├─→ Fire Spread (to neighbors)
     ├─→ Smoke/Ember Particles
     └─→ Fire Suppression
```

## Components

### ThermalPropertiesComponent
```cpp
struct ThermalPropertiesComponent {
    float current_temperature_k = 293.15f;        // 20°C
    PhaseState current_phase = PhaseState::Solid;

    // Material properties
    float specific_heat_capacity_j_kg_k = 500.0f; // J/(kg·K)
    float thermal_conductivity_w_m_k = 50.0f;     // W/(m·K)
    float melting_point_k = 1811.0f;              // Kelvin
    float ignition_temperature_k = 0.0f;          // 0 = non-flammable

    // Methods
    float apply_kinetic_heating(float proj_mass, float velocity, float target_mass);
    float add_thermal_energy(float energy_j, float mass_kg);
};
```

### CombustionComponent
```cpp
struct CombustionComponent {
    bool is_active = false;
    float fuel_remaining_kg = 1.0f;
    float fuel_consumption_rate_kg_s = 0.01f;
    float flame_temperature_k = 1200.0f;
    float oxygen_concentration = 0.21f;           // 21% in air

    // Methods
    float update_combustion(float delta_time, float oxygen);
    bool apply_suppression(float amount_kg, float effectiveness);
    bool can_ignite_neighbor(float distance_m, float target_ignition_temp_k);
};
```

## Usage Examples

### Example 1: Projectile Impact → Heating → Ignition

```cpp
#include "lore/ecs/systems/thermal_system.hpp"
#include "lore/ecs/systems/combustion_system.hpp"

// Initialize systems
ThermalSystem thermal_system;
CombustionSystem combustion_system;

thermal_system.initialize({
    .update_rate_hz = 30.0f,
    .ambient_temperature_k = 293.15f,  // 20°C
    .enable_ignition_checks = true,
});

combustion_system.initialize({
    .update_rate_hz = 60.0f,
    .ambient_oxygen_concentration = 0.21f,
    .enable_fire_spread = true,
});

// Create wooden target
Entity target = world.create_entity();

// Add material properties (wood)
world.add_component(target, ThermalPropertiesComponent{
    .current_temperature_k = 293.15f,
    .specific_heat_capacity_j_kg_k = 1700.0f,
    .thermal_conductivity_w_m_k = 0.15f,
    .ignition_temperature_k = 573.0f,  // 300°C auto-ignition
    .mass_kg = 10.0f,
});

world.add_component(target, ChemicalCompositionComponent::create_wood());

// Projectile impact!
float projectile_mass_kg = 0.01f;    // 10 gram bullet
float projectile_velocity_m_s = 800.0f;  // 800 m/s

// Convert kinetic energy → thermal energy
float temp_increase = thermal_system.apply_kinetic_heating(
    world,
    target,
    projectile_mass_kg,
    projectile_velocity_m_s,
    0.8f  // 80% conversion efficiency
);

std::cout << "Impact heated target by " << temp_increase << " K\n";

// Game loop
while (running) {
    float delta_time = get_delta_time();

    // Update thermal (heat transfer, ignition checks)
    thermal_system.update(world, delta_time);

    // Update combustion (fire simulation)
    combustion_system.update(world, delta_time);

    // Check if target is burning
    if (combustion_system.is_burning(world, target)) {
        float flame_temp = combustion_system.get_flame_temperature(world, target);
        std::cout << "Target is on fire! Flame temp: " << flame_temp << " K\n";
    }
}
```

### Example 2: Manual Ignition

```cpp
// Create entity with thermal and chemical properties
Entity campfire = world.create_entity();

world.add_component(campfire, ThermalPropertiesComponent{
    .ignition_temperature_k = 573.0f,
    .mass_kg = 5.0f,
});

world.add_component(campfire, ChemicalCompositionComponent::create_wood());

// Manually ignite
bool ignited = combustion_system.ignite(
    world,
    campfire,
    1200.0f  // Initial flame temperature
);

if (ignited) {
    std::cout << "Campfire started!\n";
}
```

### Example 3: Fire Spread

```cpp
// Setup environment with multiple flammable objects
std::vector<Entity> wooden_crates;

for (int i = 0; i < 10; ++i) {
    Entity crate = world.create_entity();

    world.add_component(crate, ThermalPropertiesComponent{
        .ignition_temperature_k = 573.0f,
        .mass_kg = 20.0f,
    });

    world.add_component(crate, ChemicalCompositionComponent::create_wood());

    // TODO: Set positions in world
    wooden_crates.push_back(crate);
}

// Ignite first crate
combustion_system.ignite(world, wooden_crates[0]);

// Fire will spread to nearby crates automatically
// Configure spread behavior:
auto config = combustion_system.get_config();
config.enable_fire_spread = true;
config.spread_check_radius_m = 5.0f;      // Check 5m radius
config.ignition_probability = 0.5f;       // 50% chance per check
config.spread_check_interval_s = 0.5f;    // Check every 0.5s
combustion_system.set_config(config);
```

### Example 4: Fire Suppression

```cpp
// Apply water suppression
bool extinguished = combustion_system.apply_suppression(
    world,
    burning_entity,
    10.0f,  // 10 kg of water
    1.0f    // 1.0 = baseline effectiveness
);

// Apply foam (more effective)
extinguished = combustion_system.apply_suppression(
    world,
    burning_entity,
    5.0f,   // 5 kg of foam
    1.5f    // 1.5x more effective than water
);

// Manually extinguish
combustion_system.extinguish(world, burning_entity);
```

### Example 5: Thermal Damage to Anatomy

```cpp
// Entity with anatomy
Entity character = world.create_entity();

world.add_component(character, ThermalPropertiesComponent{
    .current_temperature_k = 310.15f,  // 37°C body temp
    .ignition_temperature_k = 0.0f,    // Humans don't ignite (normally...)
    .mass_kg = 70.0f,
});

world.add_component(character, AnatomyComponent::create_humanoid());

// Thermal system configuration for damage
auto thermal_config = thermal_system.get_config();
thermal_config.enable_thermal_damage = true;
thermal_config.burn_threshold_temp_k = 318.15f;  // 45°C starts damage
thermal_config.instant_burn_temp_k = 373.15f;    // 100°C severe burns
thermal_config.damage_rate_j_per_hp = 1000.0f;   // 1000 J = 1 damage point
thermal_system.set_config(thermal_config);

// Apply external heat (fire, hot surface, etc.)
thermal_system.apply_heat(world, character, 50000.0f);  // 50 kJ

// Thermal system will automatically damage anatomy organs based on temperature
// Damaged organs will have DamageType::Thermal flag set
```

### Example 6: Heat Transfer Between Entities

```cpp
// Hot metal rod
Entity hot_rod = world.create_entity();
world.add_component(hot_rod, ThermalPropertiesComponent{
    .current_temperature_k = 1200.0f,  // Red hot
    .thermal_conductivity_w_m_k = 50.0f,  // Steel
    .mass_kg = 5.0f,
});

// Cold water bucket
Entity water_bucket = world.create_entity();
world.add_component(water_bucket, ThermalPropertiesComponent{
    .current_temperature_k = 293.15f,  // 20°C
    .thermal_conductivity_w_m_k = 0.6f,   // Water
    .mass_kg = 10.0f,
});

// Place entities near each other
// Thermal system will automatically:
// 1. Calculate heat conduction (Fourier's law)
// 2. Calculate thermal radiation (Stefan-Boltzmann)
// 3. Transfer heat from hot → cold
// 4. Update temperatures each frame

// Monitor temperatures
while (running) {
    thermal_system.update(world, delta_time);

    float rod_temp = thermal_system.get_temperature(world, hot_rod);
    float water_temp = thermal_system.get_temperature(world, water_bucket);

    std::cout << "Rod: " << rod_temp << " K, Water: " << water_temp << " K\n";
}
```

## Performance Tuning

### Thermal System

```cpp
auto thermal_config = thermal_system.get_config();

// Reduce update rate (default: 30 Hz)
thermal_config.update_rate_hz = 15.0f;  // Half rate = 2x faster

// Adjust heat transfer range
thermal_config.conduction_range_m = 0.3f;   // Smaller = fewer queries
thermal_config.radiation_range_m = 5.0f;    // Reduce from 10m

// Spatial partitioning (essential for large worlds)
thermal_config.use_spatial_partitioning = true;
thermal_config.spatial_grid_cell_size_m = 2.0f;
thermal_config.max_neighbors_per_entity = 10;  // Limit queries

// Disable features if not needed
thermal_config.enable_phase_transitions = false;
thermal_config.enable_thermal_damage = false;

thermal_system.set_config(thermal_config);
```

### Combustion System

```cpp
auto combustion_config = combustion_system.get_config();

// LOD distances
combustion_config.enable_lod = true;
combustion_config.lod_distance_high_m = 20.0f;   // Full sim within 20m
combustion_config.lod_distance_medium_m = 50.0f; // Reduced sim 20-50m
combustion_config.lod_distance_low_m = 100.0f;   // Minimal sim 50-100m

// Particle budgets
combustion_config.max_smoke_particles = 5000;    // Reduce from 10000
combustion_config.max_ember_particles = 2000;    // Reduce from 5000
combustion_config.particle_lod_distance_m = 30.0f; // No particles beyond 30m

// Fire spread optimization
combustion_config.spread_check_interval_s = 1.0f;  // Check every 1s instead of 0.5s
combustion_config.spatial_grid_cell_size_m = 10.0f; // Larger cells

combustion_system.set_config(combustion_config);
```

## Integration with Other Systems

### With Ballistics System

```cpp
// In ballistics impact handler
void on_projectile_impact(Entity projectile, Entity target, float velocity) {
    auto* proj_props = world.get_component<ProjectileComponent>(projectile);
    if (!proj_props) return;

    // Convert kinetic energy to heat
    thermal_system.apply_kinetic_heating(
        world,
        target,
        proj_props->mass_kg,
        velocity,
        0.8f  // 80% conversion
    );

    // Check if target ignited from impact heat
    if (thermal_system.can_ignite(world, target)) {
        combustion_system.ignite(world, target);
    }
}
```

### With Anatomy System

Thermal damage is automatically applied by ThermalSystem when:
1. Entity has both `ThermalPropertiesComponent` and `AnatomyComponent`
2. Temperature exceeds `burn_threshold_temp_k` (default: 45°C)
3. Config `enable_thermal_damage` is true

Damage is applied to organs with `DamageType::Thermal` flag set.

### With Material System

```cpp
// Material determines thermal properties
world.add_component(entity, ThermalPropertiesComponent{
    .thermal_conductivity_w_m_k = material.thermal_conductivity,
    .melting_point_k = material.melting_point,
    .ignition_temperature_k = material.ignition_temp,
});

// Chemical composition determines combustion
if (material.is_combustible) {
    world.add_component(entity, ChemicalCompositionComponent{
        .chemical_formula = material.formula,
        .is_combustible = true,
        .heat_of_combustion_kj_mol = material.energy_density,
    });
}
```

## Statistics & Debugging

```cpp
// Thermal system stats
auto thermal_stats = thermal_system.get_stats();
std::cout << "Thermal System:\n"
          << "  Entities: " << thermal_stats.entities_processed << "\n"
          << "  Heat Transfers: " << thermal_stats.heat_transfers_performed << "\n"
          << "  Phase Transitions: " << thermal_stats.phase_transitions << "\n"
          << "  Ignitions: " << thermal_stats.ignitions_triggered << "\n"
          << "  Total Heat: " << thermal_stats.total_heat_transferred_j << " J\n"
          << "  Avg Temp: " << thermal_stats.avg_temperature_k << " K\n";

// Combustion system stats
auto combustion_stats = combustion_system.get_stats();
std::cout << "Combustion System:\n"
          << "  Active Fires: " << combustion_stats.fires_active << "\n"
          << "  Fires Started: " << combustion_stats.fires_started << "\n"
          << "  Fires Extinguished: " << combustion_stats.fires_extinguished << "\n"
          << "  Spread Attempts: " << combustion_stats.spread_attempts << "\n"
          << "  Successful Spreads: " << combustion_stats.successful_spreads << "\n"
          << "  Heat Generated: " << combustion_stats.total_heat_generated_j << " J\n"
          << "  O2 Consumed: " << combustion_stats.total_oxygen_consumed_mol << " mol\n"
          << "  Smoke Particles: " << combustion_stats.smoke_particles_spawned << "\n"
          << "  Ember Particles: " << combustion_stats.ember_particles_spawned << "\n";
```

## Common Pitfalls

### 1. Forgetting ChemicalCompositionComponent
```cpp
// BAD: Thermal component alone can't ignite
world.add_component(entity, ThermalPropertiesComponent{...});

// GOOD: Add chemical composition for combustion
world.add_component(entity, ThermalPropertiesComponent{...});
world.add_component(entity, ChemicalCompositionComponent::create_wood());
```

### 2. Ignition Temperature Too High
```cpp
// BAD: Unrealistic ignition temperature
thermal_props.ignition_temperature_k = 5000.0f;  // Never reaches this

// GOOD: Realistic values
thermal_props.ignition_temperature_k = 573.0f;   // Wood: ~300°C
```

### 3. Not Updating Systems
```cpp
// BAD: Systems must be updated each frame
// (no update calls = nothing happens)

// GOOD: Update both systems
thermal_system.update(world, delta_time);
combustion_system.update(world, delta_time);
```

### 4. Extreme Update Rates
```cpp
// BAD: Too high causes performance issues
thermal_config.update_rate_hz = 120.0f;  // Overkill

// GOOD: 30 Hz is usually sufficient
thermal_config.update_rate_hz = 30.0f;
```

## Best Practices

1. **Use Spatial Partitioning**: Essential for worlds with many thermal entities
2. **Tune Update Rates**: 30 Hz thermal, 60 Hz combustion is a good baseline
3. **Enable LOD**: Reduces cost for distant fires
4. **Limit Particle Budgets**: Prevents performance collapse
5. **Profile First**: Measure before optimizing
6. **Use Material Presets**: Start with presets, customize as needed
7. **Monitor Stats**: Track system performance with get_stats()

## Next Steps

- Implement volumetric fire rendering (GPU Navier-Stokes)
- Add particle systems for smoke/embers
- Create fire suppression mechanics (fire extinguishers, sprinklers)
- Implement temperature-based material property changes
- Add heat haze/distortion rendering
- Create sound system integration (crackling, roaring)

---

*This document describes Lore Engine thermal and combustion systems v1.0*