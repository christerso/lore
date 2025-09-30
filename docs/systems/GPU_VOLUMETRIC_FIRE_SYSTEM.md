# GPU Volumetric Fire System

## Overview

The GPU Volumetric Fire System implements state-of-the-art fire simulation using Navier-Stokes fluid dynamics entirely on GPU compute shaders. Based on NVIDIA FlameWorks and Jos Stam's "Stable Fluids" paper, this system provides realistic fire with buoyancy, turbulence, and efficient performance.

## Architecture

```
CPU (ECS System)                        GPU (Compute Shaders)
================                        =====================

VolumetricFireSystem
├─ Create 3D Textures                  ┌─> Velocity Field (R32G32B32A32F)
│  (velocity, density, temp, pressure) ├─> Density Field (R32F)
│                                       ├─> Temperature Field (R32F)
├─ Compile Compute Pipelines           ├─> Pressure Field (R32F)
│  (6 shaders)                         └─> Divergence Field (R32F)
│
└─ Per Frame:                          GPU Simulation Pipeline:
   └─> For each fire entity:           ┌──────────────────────┐
       ├─> 1. Advect                   │ 1. Semi-Lagrangian   │
       │      (fire_advect.comp)       │    Advection         │
       │                                │    - Move fields     │
       ├─> 2. Inject Source            │    - MacCormack      │
       │      (fire_inject_source.comp)└──────────────────────┘
       │                                         ↓
       ├─> 3. Add Forces               ┌──────────────────────┐
       │      (fire_add_force.comp)    │ 2. Source Injection  │
       │                                │    - Add fuel        │
       ├─> 4. Divergence               │    - Add heat        │
       │      (fire_divergence.comp)   │    - Add velocity    │
       │                                └──────────────────────┘
       ├─> 5. Jacobi (×40 iterations)           ↓
       │      (fire_jacobi.comp)       ┌──────────────────────┐
       │                                │ 3. Force Application │
       └─> 6. Project                  │    - Buoyancy        │
           (fire_project.comp)         │    - Gravity         │
                                        │    - Wind            │
                                        │    - Vorticity       │
                                        └──────────────────────┘
                                                 ↓
                                        ┌──────────────────────┐
                                        │ 4. Divergence Calc   │
                                        │    ∇·u = ∂u/∂x +    │
                                        │          ∂v/∂y +    │
                                        │          ∂w/∂z      │
                                        └──────────────────────┘
                                                 ↓
                                        ┌──────────────────────┐
                                        │ 5. Pressure Solve    │
                                        │    ∇²p = ∇·u        │
                                        │    (Jacobi ×40)     │
                                        └──────────────────────┘
                                                 ↓
                                        ┌──────────────────────┐
                                        │ 6. Projection        │
                                        │    u_new = u - ∇p   │
                                        │    Makes ∇·u = 0    │
                                        └──────────────────────┘
```

## Compute Shaders

### 1. fire_advect.comp
**Purpose**: Move velocity, density, and temperature through the velocity field

**Algorithm**: Semi-Lagrangian Advection with MacCormack correction
```glsl
// Backward trace
vec3 back_pos = current_pos - velocity * dt / cell_size;
value_advected = sample(field, back_pos);

// MacCormack correction (optional, reduces dissipation)
vec3 forward_pos = current_pos + velocity * dt / cell_size;
value_forward = sample(field, forward_pos);
error = (value_original - value_forward) * strength;
value_corrected = value_advected + error;
```

**Inputs**:
- Velocity field (R32G32B32A32F)
- Density field (R32F)
- Temperature field (R32F)

**Outputs**:
- Advected velocity, density, temperature

**Performance**: O(n) with trilinear interpolation

### 2. fire_inject_source.comp
**Purpose**: Add fuel, heat, and velocity at fire source point

**Algorithm**: Gaussian-weighted injection
```glsl
float distance = length(cell_pos - source_pos);
float falloff = exp(-distance² / radius²);

velocity = mix(velocity, source_velocity, falloff * 0.8);
density += fuel_rate * falloff * dt;
temperature = max(temperature, source_temp * falloff);
```

**Parameters**:
- Source position, radius
- Source velocity, temperature
- Fuel injection rate

### 3. fire_add_force.comp
**Purpose**: Apply buoyancy, gravity, wind, and turbulence

**Buoyancy** (hot air rises):
```glsl
F_buoyancy = (T - T_ambient) * α * (-gravity)
velocity += F_buoyancy * dt
```

**Vorticity Confinement** (adds turbulence):
```glsl
ω = ∇ × u                    // Calculate vorticity
N = ∇|ω| / |∇|ω||           // Gradient of vorticity magnitude
F_conf = ε * h * (N × ω)    // Confinement force
velocity += F_conf * dt
```

### 4. fire_divergence.comp
**Purpose**: Calculate divergence for pressure solve

**Algorithm**: Central differences
```glsl
∇·u = (u[i+1] - u[i-1]) / (2*h) +
      (v[j+1] - v[j-1]) / (2*h) +
      (w[k+1] - w[k-1]) / (2*h)
```

### 5. fire_jacobi.comp
**Purpose**: Iteratively solve Poisson equation for pressure

**Equation**: ∇²p = ∇·u

**Algorithm**: Jacobi iteration with SOR
```glsl
p_new = (p[i-1] + p[i+1] + p[j-1] + p[j+1] + p[k-1] + p[k+1] - h² * div) / 6

// Successive over-relaxation
p = ω * p_new + (1-ω) * p_old
```

**Iterations**: 40-50 for convergence
**Over-relaxation factor**: 1.8-1.9 for faster convergence

### 6. fire_project.comp
**Purpose**: Make velocity field divergence-free

**Algorithm**: Helmholtz decomposition
```glsl
// Calculate pressure gradient
∇p = [(p[i+1] - p[i-1]) / (2*h),
      (p[j+1] - p[j-1]) / (2*h),
      (p[k+1] - p[k-1]) / (2*h)]

// Subtract from velocity
u_new = u - ∇p

// Now ∇·u_new = 0 (incompressible)
```

## Usage Example

```cpp
#include "lore/ecs/systems/volumetric_fire_system.hpp"
#include "lore/ecs/systems/thermal_system.hpp"
#include "lore/ecs/systems/combustion_system.hpp"

// Initialize systems
ThermalSystem thermal_system;
CombustionSystem combustion_system;
VolumetricFireSystem volumetric_fire_system;

thermal_system.initialize({...});
combustion_system.initialize({...});
volumetric_fire_system.initialize(vulkan_gpu_context, {
    .enable_async_compute = true,
    .max_jacobi_iterations = 40,
    .enable_maccormack_advection = true,
    .enable_vorticity_confinement = true,
});

// Create entity with volumetric fire
Entity campfire = world.create_entity();

// Add thermal/combustion components
world.add_component(campfire, ThermalPropertiesComponent::create_wood());
world.add_component(campfire, ChemicalCompositionComponent::create_wood());
world.add_component(campfire, CombustionComponent::ignite(1200.0f, 5.0f));

// Add volumetric fire component
world.add_component(campfire, VolumetricFireComponent::create_campfire());

// Game loop
while (running) {
    float delta_time = get_delta_time();

    // Update thermal and combustion (CPU)
    thermal_system.update(world, delta_time);
    combustion_system.update(world, delta_time);

    // Update volumetric fire (GPU)
    volumetric_fire_system.update(world, delta_time);

    // Render
    begin_frame();
    {
        // Render scene geometry
        render_scene();

        // Render volumetric fire (raymarching)
        volumetric_fire_system.render(cmd_buffer, view_matrix, proj_matrix);
    }
    end_frame();
}
```

## Performance

### GPU Cost
- **Per fire entity** (64×128×64 grid):
  - Advection: ~0.5ms
  - Source injection: ~0.1ms
  - Force application: ~0.3ms
  - Divergence: ~0.2ms
  - Jacobi (×40): ~2.0ms
  - Projection: ~0.2ms
  - **Total**: ~3.3ms per fire

- **Multiple fires**: Linear scaling (each fire independent)
- **LOD benefit**: 0.5× resolution = 8× faster (3D grid)

### Memory Usage
Per fire (64×128×64):
- Velocity: 2× 64×128×64× 16 bytes = 16 MB
- Density: 2× 64×128×64× 4 bytes = 4 MB
- Temperature: 2× 64×128×64× 4 bytes = 4 MB
- Pressure: 2× 64×128×64× 4 bytes = 4 MB
- Divergence: 64×128×64× 4 bytes = 2 MB
- **Total**: ~30 MB per fire

### LOD Scaling

```cpp
config.lod_distance_full_m = 20.0f;    // 1.0× resolution (64×128×64)
config.lod_distance_half_m = 50.0f;    // 0.5× resolution (32×64×32) → 8× faster
config.lod_distance_quarter_m = 100.0f; // 0.25× resolution (16×32×16) → 64× faster
```

## Configuration

### Quality Settings

**High Quality** (cinematic):
```cpp
config.max_jacobi_iterations = 60;
config.enable_maccormack_advection = true;
config.maccormack_strength = 0.9f;
config.enable_vorticity_confinement = true;
config.vorticity_strength_mult = 1.0f;
```

**Medium Quality** (gameplay):
```cpp
config.max_jacobi_iterations = 40;
config.enable_maccormack_advection = true;
config.maccormack_strength = 0.8f;
config.enable_vorticity_confinement = true;
config.vorticity_strength_mult = 0.7f;
```

**Low Quality** (performance):
```cpp
config.max_jacobi_iterations = 20;
config.enable_maccormack_advection = false;
config.enable_vorticity_confinement = false;
```

### Fire Presets

**Campfire**:
```cpp
auto fire = VolumetricFireComponent::create_campfire();
// 48×96×48 grid, 0.08m cells
// Source: 1200K, 0.05 kg/s fuel
// Bounds: 2m×8m×2m
```

**Building Fire**:
```cpp
auto fire = VolumetricFireComponent::create_building_fire();
// 128×256×128 grid, 0.2m cells
// Source: 1400K, 1.0 kg/s fuel
// Bounds: 12.8m×51.2m×12.8m
```

**Torch**:
```cpp
auto fire = VolumetricFireComponent::create_torch();
// 32×64×32 grid, 0.05m cells
// Source: 1300K, 0.01 kg/s fuel
// Bounds: 0.8m×3.2m×0.8m
```

## Integration with Other Systems

### ThermalSystem
```cpp
// Heat generation from fire
auto* thermal = world.get_component<ThermalPropertiesComponent>(fire_entity);
auto* combustion = world.get_component<CombustionComponent>(fire_entity);

float heat_j = combustion->heat_output_w * delta_time;
thermal_system.apply_heat(world, fire_entity, heat_j);
```

### CombustionSystem
```cpp
// Fuel consumption drives volumetric fire parameters
auto* fire = world.get_component<VolumetricFireComponent>(entity);
auto* combustion = world.get_component<CombustionComponent>(entity);

fire->source_fuel_rate_kg_s = combustion->fuel_consumption_rate_kg_s;
fire->source_temperature_k = combustion->flame_temperature_k;
```

### VolumetricSmokeSystem
```cpp
// Smoke generated from fire
auto* fire = world.get_component<VolumetricFireComponent>(entity);
auto* smoke = world.get_component<VolumetricSmokeComponent>(entity);

smoke->source_position = fire->source_position;
smoke->source_radius_m = fire->flame_radius_m * 1.5f;
smoke->rise_speed_m_s = 2.0f;
```

## Technical Details

### Double Buffering
All fields use ping-pong buffers to avoid read-write hazards:
```cpp
// Read from buffer 0, write to buffer 1
advect(velocity[0], density[0], temp[0] → velocity[1], density[1], temp[1])

// Swap indices
std::swap(read_index, write_index);

// Next pass reads buffer 1, writes buffer 0
inject_source(velocity[1], density[1], temp[1] → velocity[0], density[0], temp[0])
```

### Pipeline Barriers
Between each shader pass, insert memory barrier:
```cpp
VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
};

vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    0, 1, &barrier, 0, nullptr, 0, nullptr);
```

### Workgroup Size
All shaders use 8×8×8 workgroups (512 threads):
```glsl
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
```

Dispatch calculation:
```cpp
uint32_t groups_x = (resolution_x + 7) / 8;
uint32_t groups_y = (resolution_y + 7) / 8;
uint32_t groups_z = (resolution_z + 7) / 8;
vkCmdDispatch(cmd, groups_x, groups_y, groups_z);
```

## Debugging

### Visualization Modes

**Velocity Field**:
```cpp
config.visualize_velocity_field = true;
// Renders colored arrows showing velocity direction/magnitude
```

**Pressure Field**:
```cpp
config.visualize_pressure_field = true;
// Renders pressure as colors (blue=low, red=high)
```

### GPU Timings
```cpp
config.log_gpu_timings = true;

auto stats = volumetric_fire_system.get_stats();
std::cout << "Advection: " << stats.advection_time_ms << " ms\n"
          << "Pressure: " << stats.pressure_solve_time_ms << " ms\n"
          << "Total: " << stats.total_gpu_time_ms << " ms\n";
```

## Known Limitations

1. **No GPU-CPU Readback**: Fire state stays on GPU (cannot query individual cell values from CPU)
2. **Fixed Grid**: Grid resolution is fixed per fire (no dynamic adaptive grids)
3. **No Multi-GPU**: Single GPU only (no AFR or SFR)
4. **Memory Intensive**: Large grids consume significant VRAM
5. **No Fire-Fire Interaction**: Each fire is independent (no merging or splitting)

## Future Enhancements

1. **Adaptive Grids**: Higher resolution near flames, lower in smoke
2. **Sparse Volumes**: Only simulate non-empty regions
3. **Multi-Grid Solver**: Hierarchical pressure solve for faster convergence
4. **Temperature-Dependent Viscosity**: More realistic cooling behavior
5. **Combustion Chemistry**: Track fuel/oxygen concentrations per cell
6. **Fire Splitting/Merging**: Dynamic fire lifecycle

## References

- Jos Stam, "Stable Fluids" (1999)
- Mark J. Harris, "Real-Time Cloud Simulation and Rendering" (2003)
- NVIDIA GameWorks, "PhysX FlameWorks" (2014)
- Robert Bridson, "Fluid Simulation for Computer Graphics" (2015)
- Matthias Müller-Fischer et al., "Fast Stable Fluids" (2008)

---

*GPU Volumetric Fire System - Lore Engine v1.0*