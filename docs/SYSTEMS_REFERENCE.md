# Systems Reference

This document provides a quick reference to all systems in the Lore engine, their tick rates, dependencies, and integration points.

---

## Core Systems

### Vision and Visibility System

**Location**: `lore/vision/`, `lore/ecs/components/vision_*.hpp`, `lore/ecs/systems/*_vision_system.hpp`

**Purpose**: Field-of-view calculation, line-of-sight queries, and AI vision-based behaviors.

**Tick Rate**: Per-frame (60Hz typical)

**Components**:
- `VisionComponent`: Entity vision capabilities (range, FOV, perception)
- `VisibilityComponent`: What entity currently sees (tiles and entities)
- `AIVisionBehaviorComponent`: AI state machine driven by vision

**Systems**:
- `VisionSystem`: Calculates FOV for all entities each frame
- `AIVisionSystem`: Updates AI behaviors based on visibility

**Dependencies**:
- **Requires**: `TilemapWorldSystem` (for tile occlusion queries)
- **Requires**: `TransformComponent` (for entity positions)
- **Integrates With**: AI systems, stealth systems, faction systems, rendering

**Update Order**:
1. `TilemapWorldSystem` (provides world data)
2. `VisionSystem` (calculates FOV)
3. `AIVisionSystem` (processes AI behaviors)
4. Other AI systems (pathfinding, combat, etc.)

**Key Features**:
- Shadow casting FOV algorithm with 8-octant coverage
- Environmental modifiers (darkness, fog, rain, smoke)
- Tile-based occlusion with transparency and height
- AI behaviors: detection, pursuit, fleeing, investigation
- Target classification: prey, predator, neutral, ally
- Customizable vision profiles and presets

**Performance Notes**:
- ~0.5-2ms for 100 entities (depends on FOV range)
- Supports per-entity enable/disable for optimization
- Recommended max: 200-500 entities with active vision

**Documentation**: See `docs/systems/vision.md` for detailed usage guide

**Example Usage**:
```cpp
// Add vision to entity
Entity player = world.create_entity();
world.add_component(player, VisionComponent::create_player());
world.add_component(player, VisibilityComponent{});

// Check what entity can see
auto* visibility = world.get_component<VisibilityComponent>(player);
if (visibility->can_see_entity(target_id)) {
    // Target is visible
}

// Add AI behavior
world.add_component(guard, AIVisionBehaviorComponent::create_guard());
```

**Configuration**:
```cpp
// Set environmental conditions
vision::EnvironmentalConditions env;
env.time_of_day = 0.1f;           // Night
env.fog_density = 0.5f;           // Heavy fog
env.smoke_density = 0.8f;         // Smoke grenades
vision_system.set_environment(env);
```

---

## World Systems

### Tilemap World System with Instancing and Destruction

**Location**: `lore/world/tilemap_world_system.hpp`, `lore/world/tiled_importer.hpp`

**Purpose**: 3D tile-based world with efficient mesh instancing, dynamic destruction, and voxel simulation integration.

**Tick Rate**: Varies by subsystem
- Tile management: On-demand (add/remove tiles)
- Destruction: Event-driven (damage events)
- Voxel simulation: Per-frame (60Hz for fluids)

**Architecture**:
- **TileDefinition**: Immutable shared properties (mesh, material, physics)
- **TileInstance**: Per-tile runtime state (position, rotation, health)
- **TileChunk**: Spatial partition (16×16×16 tiles) for culling

**Key Features**:
- ✅ Mesh instancing - single mesh shared by 1000+ tiles
- ✅ Progressive destruction - intact → damaged → fractured → broken
- ✅ Voxel debris - broken tiles become 512 physics-simulated pieces
- ✅ Fluid integration - liquid/smoke flows through broken tiles
- ✅ Chunk-based streaming - infinite worlds with dynamic loading

**Performance**:
- **Intact tiles**: 2 draw calls for entire room (Cube + FloorTile)
- **Broken tiles**: 1 draw call each (unique geometry)
- **Memory**: 64 bytes per intact tile, ~100KB per broken tile
- **Recommended**: 100K+ intact tiles, <1000 broken tiles

**Destruction States**:
```cpp
enum class TileState {
    Intact,      // Shared mesh, normal rendering
    Damaged,     // Crack decals, 60-100% health
    Fractured,   // Breaking animation, 30-60% health
    Broken,      // Unique voxel debris, 0-30% health
    Debris       // Physics-simulated pieces
};
```

**Dependencies**:
- **Requires**: `TileMeshCache` (mesh resource management)
- **Requires**: `VoxelFluidSimulation` (liquid/smoke sim)
- **Integrates With**: Physics system (debris), rendering, vision system

**Update Order**:
1. `TileDestructionSystem` (processes damage events)
2. `VoxelFluidSimulation` (updates fluid flow)
3. `TilemapWorldSystem` (updates tile states)
4. `VisionSystem` (recalculates occlusion)

**Documentation**: See `docs/systems/TILE_INSTANCING_AND_DESTRUCTION.md` for complete architecture

**Example Usage**:
```cpp
// Load Tiled map with shared meshes
auto tiled_map = TiledImporter::load_tiled_map("assets/maps/test_room.tmj");
TiledImporter::import_to_world(world_system, tiled_map, 0.0f, 0.0f, 0.0f);

// Damage tile (progressive destruction)
auto* tile = world_system.get_tile_mutable({5, 5, 0});
destruction_system.damage_tile(*tile, 0.4f);  // 40% damage

// Check if broken (for fluid simulation)
if (tile->state == TileState::Broken) {
    voxel_simulation.on_tile_broken(tile->coord, tile->broken_geometry->voxel_pieces);
}

// Efficient rendering (instanced)
tile_renderer.render_tiles(world_system);  // 2 draw calls for 121 tiles
```

**Mesh Instancing Pipeline**:
```cpp
// Renderer groups tiles by mesh
std::unordered_map<uint32_t, std::vector<TileInstanceGPU>> instances_by_mesh;

// Single draw call per mesh type
for (const auto& [mesh_id, instances] : instances_by_mesh) {
    vkCmdDrawIndexedIndirect(cmd, mesh, instances.size());  // GPU instancing
}
```

**Configuration**:
```cpp
// Chunk size for spatial partitioning
TilemapWorldSystem::CHUNK_SIZE = 16;  // 16×16×16 tiles

// Voxel resolution for broken tiles
VoxelFluidSimulation::VOXELS_PER_TILE = 8;  // 8×8×8 = 512 voxels

// Destruction parameters
destruction_system.set_voxelization_resolution(8);
destruction_system.set_debris_lifetime(30.0f);  // seconds
```

---

## Physics Systems

### Physics-Based Entity System

**Location**: `lore/ecs/components/biological_properties_component.hpp`, `lore/ecs/components/anatomy_component.hpp`, `lore/physics/`

**Purpose**: Realistic physics-based damage system for biological entities using organ-based health (NO HITPOINTS).

**Tick Rate**: On-demand (damage events)

**Components**:
- `BiologicalPropertiesComponent`: Physical tissue properties for ballistics
- `AnatomyComponent`: Organ-based health system with energy thresholds
- `RigidBodyComponent`: Mass, velocity, forces (from physics library)

**Dependencies**:
- **Requires**: `physics::Material` (base material properties)
- **Requires**: `RigidBodyComponent` (mass and velocity data)
- **Integrates With**: `BallisticsSystem`, `CombatSystem`, rendering for visual damage

**Key Features**:
- **NO HITPOINTS**: Organ function (0.0-1.0) determines health
- **Physics-based penetration**: Kinetic energy calculation (E = 0.5 * m * v²)
- **Realistic tissue properties**: Density, hardness, hydration levels
- **Ballistics raycast**: Ray-sphere intersection through organs
- **Energy thresholds**: Organs destroyed when accumulated energy exceeds threshold
- **Critical organs**: Brain/heart destruction = instant death
- **Bleeding**: Per-organ blood loss rates affect overall health

**Tissue Materials**:
- Flesh: 1060 kg/m³, 75% hydration
- Bone: 1900 kg/m³, 15% hydration
- Fat: 900 kg/m³, 20% hydration
- Skin: 1100 kg/m³, 65% hydration

**Organ Data** (realistic values):
```cpp
// Brain: 1.4kg, 8cm radius, 30J threshold
// Heart: 0.3kg, 6cm radius, 30J threshold
// Lungs: 1.1kg, 10cm radius, 80J threshold
// Liver: 1.5kg, 8cm radius, 100J threshold
```

**Example Usage**:
```cpp
// Create entity with biological properties
Entity creature = world.create_entity();
world.add_component(creature, BiologicalPropertiesComponent{});
world.add_component(creature, AnatomyComponent::create_humanoid());

// Process projectile impact
auto* anatomy = world.get_component<AnatomyComponent>(creature);
auto* bio = world.get_component<BiologicalPropertiesComponent>(creature);

// Calculate penetration depth
float penetration = bio->calculate_penetration_depth(
    projectile_mass_kg, projectile_velocity_m_s,
    bio->get_tissue_material(TissueType::Flesh)
);

// Raycast through organs
auto hit_organs = anatomy->check_trajectory_hits(
    impact_point, impact_direction, penetration, entity_position
);

// Apply energy-based damage to organs
for (const auto& organ_name : hit_organs) {
    anatomy->apply_energy_damage(organ_name, kinetic_energy_j);
}

// Check for death (critical organ failure or bleeding)
if (anatomy->is_dead()) {
    // Entity has died
}
```

**Documentation**: See `docs/design/physics_based_entity_system.md`

---

### Structural Physics System

**Location**: `lore/physics/structural_material.hpp`, `lore/ecs/components/world_mesh_material_component.hpp`, `lore/ecs/systems/structural_integrity_system.hpp`

**Purpose**: Structural engineering simulation for world geometry with load bearing and stress analysis.

**Tick Rate**: Per-frame (60Hz typical) + on-demand (impact events)

**Components**:
- `StructuralMaterial`: Material strength properties (tensile, compressive, shear)
- `WorldMeshMaterialComponent`: Per-vertex stress state and load tracking
- `TransformComponent`: Entity position/rotation

**Systems**:
- `StructuralIntegritySystem`: Calculates loads, stress, detects failures, handles collapse

**Dependencies**:
- **Requires**: `physics::Material` (density, hardness)
- **Requires**: Gravity vector for load calculations
- **Integrates With**: `FractureProperties`, `VoronoiFractureSystem`, mesh rendering

**Key Features**:
- **Real engineering properties**: Tensile/compressive/shear strength in Pascals
- **Young's modulus**: Material stiffness (GPa)
- **Stress analysis**: Von Mises stress calculation
- **Load bearing**: Gravitational loads, edge propagation
- **Structural failure**: Threshold-based fracture detection
- **Brittle vs ductile**: Material-specific failure modes

**Material Presets** (real values):
```cpp
// Concrete: 30 MPa compression, 3 MPa tension, 30 GPa stiffness
// Steel: 400 MPa both, 200 GPa stiffness
// Wood: 50 MPa compression, 30 MPa tension, 12 GPa stiffness
// Glass: 50 MPa compression, 50 MPa tension, 70 GPa stiffness (brittle)
// Brick: 20 MPa compression, 2 MPa tension, 15 GPa stiffness
```

**Example Usage**:
```cpp
// Create structural entity
Entity wall = world.create_entity();
world.add_component(wall, StructuralMaterial::create_concrete());
world.add_component(wall, WorldMeshMaterialComponent{});

// System calculates loads and stress automatically
structural_system->update(world, delta_time);

// Apply projectile impact
structural_system->apply_projectile_impact(
    world, wall, hit_position, impact_direction, kinetic_energy_j
);

// System detects failure and triggers fracture if thresholds exceeded
```

**Documentation**: See `docs/design/world_materials_and_structural_physics.md`

---

### Dynamic Mesh Fracture System

**Location**: `lore/ecs/systems/voronoi_fracture_system.hpp`, `lore/ecs/components/fracture_properties.hpp`, `lore/ecs/components/surface_damage_component.hpp`, `lore/ecs/systems/debris_manager.hpp`

**Purpose**: GPU-accelerated dynamic mesh fracture with material-specific breakage patterns.

**Tick Rate**: On-demand (fracture events)

**Components**:
- `FractureProperties`: Material-specific fracture behavior
- `SurfaceDamageComponent`: Bullet holes, dents, chips (geometry modification)
- `DebrisComponent`: Lifetime tracking for debris entities

**Systems**:
- `VoronoiFractureSystem`: GPU Voronoi generation and mesh slicing
- `DebrisManager`: Polygon budget enforcement, LOD, cleanup

**Dependencies**:
- **Requires**: GPU compute shaders (voronoi_fracture.comp, voronoi_build_cells.comp)
- **Requires**: `WorldMeshMaterialComponent` for stress analysis integration
- **Requires**: `RigidBodyComponent` for debris physics
- **Integrates With**: `StructuralIntegritySystem`, rendering, physics simulation

**Update Order**:
1. `StructuralIntegritySystem` (detects failures)
2. `VoronoiFractureSystem` (generates fracture on failure)
3. `DebrisManager` (enforces budgets, LOD, cleanup)
4. Physics systems (debris simulation)

**Key Features**:
- **GPU Voronoi generation**: 500 cells at 60 FPS (two-pass compute shader)
- **Material-specific patterns**:
  - Glass: 8-40 pieces, radial cracks, sharp edges
  - Metal: 1-3 pieces, tears along stress lines
  - Wood: 3-8 pieces, splits along grain
  - Concrete: 5-15 pieces, crumbly irregular chunks
- **Impact-centered clustering**: Seeds cluster near impact point
- **Fragment physics**: Energy-based velocities, angular momentum
- **Polygon budget**: 500 entities, 50k triangles max
- **LOD system**: 25% triangles at 50m distance
- **Debris lifetime**: 30 seconds default, automatic cleanup
- **Surface damage**: Bullet holes modify geometry without full fracture
- **Convex hull**: Gift wrapping algorithm on GPU for cell construction

**Fracture Behaviors**:
```cpp
enum class FractureBehavior {
    Brittle,    // Glass, concrete - shatters
    Ductile,    // Metal - tears
    Fibrous,    // Wood - splinters
    Granular    // Stone - crumbles
};
```

**Performance Target**: 500 Voronoi cells at 60 FPS

**Example Usage**:
```cpp
// Setup systems
auto fracture_system = std::make_shared<VoronoiFractureSystem>();
auto debris_manager = std::make_shared<DebrisManager>();
fracture_system->set_debris_manager(debris_manager);
world.add_system(fracture_system);
world.add_system(debris_manager);

// Fracture from impact
VoronoiFractureConfig config;
config.seed_count = 15;
config.seed_clustering = 0.7f;  // Cluster near impact
config.use_gpu_generation = true;

uint32_t fragment_count = fracture_system->fracture_mesh_at_point(
    world, wall_entity,
    impact_point, impact_direction,
    kinetic_energy_j, config
);

// Debris manager automatically enforces budgets and cleans up
debris_manager->update(world, delta_time);

// Surface damage for small impacts (no full fracture)
auto* surface_damage = world.get_component<SurfaceDamageComponent>(wall);
if (!surface_damage->apply_projectile_damage(
    impact_point, impact_direction, kinetic_energy_j,
    fracture_props, mesh_vertices, mesh_normals)) {
    // Budget exceeded, fall back to decal rendering
}
```

**GPU Compute Pipeline**:
1. **First pass** (voronoi_fracture.comp):
   - Generate 3D distance field (8x8x8 workgroups)
   - Find nearest seed for each voxel
   - Extract boundary vertices with atomic counters
2. **Second pass** (voronoi_build_cells.comp):
   - Build convex hull per cell (gift wrapping)
   - Generate triangle indices
   - Calculate normals

**Debris Management**:
```cpp
DebrisManager::Config config;
config.max_debris_entities = 500;
config.max_total_triangles = 50000;
config.debris_lifetime_seconds = 30.0f;
config.enable_lod = true;
config.lod_distance_near = 20.0f;
config.lod_distance_far = 50.0f;
config.lod_reduction_far = 0.25f;  // 25% triangles far away
```

**Research Reference**: Liu et al., "Real-time Dynamic Fracture with Volumetric Approximate Convex Decompositions"

**Documentation**: See `docs/design/dynamic_mesh_fracture_system.md`

---

## World Systems

### Tilemap World System

**Location**: `lore/world/tilemap_world_system.hpp`

**Purpose**: 3D tile-based world representation, chunk management, and spatial queries.

**Tick Rate**: Per-frame (60Hz typical)

**Dependencies**:
- **Required By**: `VisionSystem` (provides tile occlusion data)
- **Required By**: Physics systems, pathfinding, rendering

**Integration Points**:
- Implements `IVisionWorld` interface for vision queries
- Provides `TileVisionData` for FOV calculations
- Manages tile definitions and properties

**Documentation**: See world system documentation

---

## ECS Core

### Entity Component System (ECS)

**Location**: `lore/ecs/`

**Purpose**: Core entity-component-system architecture for game objects.

**Components**:
- `TransformComponent`: Entity position, rotation, scale
- `VisionComponent`: Entity vision capabilities
- `VisibilityComponent`: What entity can see
- `AIVisionBehaviorComponent`: AI vision behaviors

**Systems**: All game systems inherit from `System` base class

**Update Pattern**:
```cpp
// Typical system update order
world.update_systems(delta_time);
// 1. Input systems
// 2. AI systems (including VisionSystem, AIVisionSystem)
// 3. Physics systems
// 4. Animation systems
// 5. Rendering systems
```

---

## System Integration Guidelines

### Adding a New System

1. **Define Components** (`lore/ecs/components/`)
   - Create component structs with data and behavior
   - Add factory methods for common configurations
   - Document component fields and usage

2. **Implement System** (`lore/ecs/systems/`)
   - Inherit from `System` base class
   - Implement `init()`, `update()`, `shutdown()`
   - Query components using `world.for_each<>()`

3. **Register System** (in game initialization)
   ```cpp
   my_system = std::make_unique<MySystem>(dependencies);
   world.register_system(my_system.get());
   ```

4. **Update Documentation**
   - Add entry to this `SYSTEMS_REFERENCE.md`
   - Create detailed guide in `docs/systems/my_system.md`
   - Document dependencies and integration points

### System Update Order

Systems should be updated in this general order:

1. **Input Systems**: Process player input, camera control
2. **World Systems**: Update tilemap, chunks, spatial data
3. **Vision Systems**: Calculate FOV, update visibility
4. **AI Systems**: Process AI behaviors, decision making
5. **Physics Systems**: Movement, collision, physics simulation
6. **Animation Systems**: Update skeletal animations, procedural animation
7. **Rendering Systems**: Render scene, UI, effects

**Critical**: Systems with dependencies must update in correct order:
- `VisionSystem` requires `TilemapWorldSystem`
- `AIVisionSystem` requires `VisionSystem`
- Physics systems may require `VisionSystem` for raycast queries

### Cross-System Communication

Systems can communicate via:

1. **Shared Components**: Read/write component data
   ```cpp
   auto* visibility = world.get_component<VisibilityComponent>(entity);
   ```

2. **Events**: Publish/subscribe event system
   ```cpp
   event_bus.publish(TargetDetectedEvent{entity, target});
   ```

3. **Direct References**: Systems can hold references to other systems
   ```cpp
   VisionSystem vision_system(tilemap_world);  // Reference to world
   ```

4. **Singleton Services**: Shared services (audio, logging, etc.)
   ```cpp
   audio_system.play_sound("alert", position);
   ```

---

## Performance Monitoring

### System Performance Budget

Target frame time: 16.67ms (60 FPS)

Typical system budgets:
- Input: 0.1ms
- World Updates: 1.0ms
- Vision System: 1.0ms
- AI Systems: 2.0ms
- Physics: 3.0ms
- Animation: 1.0ms
- Rendering: 8.0ms
- Other: 0.57ms

**Optimization Strategies**:
1. Disable vision for distant/off-screen entities
2. Reduce update frequency for low-priority entities
3. Use spatial partitioning for entity queries
4. Profile systems and optimize bottlenecks

---

## Debugging Systems

### Debug Tools

1. **System Enable/Disable**:
   ```cpp
   vision_system.set_enabled(false);  // Disable system
   ```

2. **Debug Visualization**:
   ```cpp
   ai_vision_system.set_debug_mode(true);  // Enable debug output
   ```

3. **Component Inspection**:
   ```cpp
   // Log component state
   auto* vision = world.get_component<VisionComponent>(entity);
   LOG_INFO("Vision Range: {}", vision->profile.base_range_meters);
   ```

4. **Performance Profiling**:
   ```cpp
   PROFILE_SCOPE("VisionSystem::update");
   ```

---

## System Checklist

When implementing a new system:

- [ ] System class inherits from `System` base class
- [ ] Components defined in `lore/ecs/components/`
- [ ] System implementation in `lore/ecs/systems/`
- [ ] Factory methods for common component configurations
- [ ] Init/Update/Shutdown methods implemented
- [ ] Dependencies documented
- [ ] Update order defined
- [ ] Performance characteristics documented
- [ ] Entry added to `SYSTEMS_REFERENCE.md`
- [ ] Detailed documentation created in `docs/systems/`
- [ ] Integration examples provided
- [ ] Debug tools implemented (enable/disable, visualization)

---

## Related Documentation

- **Vision System**: `docs/systems/vision.md`
- **World System**: `docs/systems/world.md` (TODO)
- **ECS Architecture**: `docs/ecs/architecture.md` (TODO)
- **Performance Guide**: `docs/performance/optimization.md` (TODO)

---

## Revision History

| Date       | Version | Changes                           |
|------------|---------|-----------------------------------|
| 2025-09-30 | 1.0     | Initial version, Vision System    |