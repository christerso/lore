# Tile Instancing, Destruction, and Voxel Simulation System

## Overview

This document describes the architecture for efficient tile rendering with mesh instancing, dynamic tile destruction/breaking, and integration with liquid/smoke voxel simulations.

## Current Architecture Analysis

### Existing System (Good Foundation)
```cpp
// TileDefinition: Immutable shared data
struct TileDefinition {
    uint32_t id;
    std::string mesh_path;      // Shared mesh reference
    // ... properties
};

// TileInstance: Per-tile runtime state
struct TileInstance {
    uint32_t definition_id;     // References shared TileDefinition
    TileCoord coord;
    float rotation_degrees;
    float health;               // For destruction system
    // ... state
};
```

**Key Insight:** The system already separates **definition (shared)** from **instance (per-tile state)**. This is perfect for mesh instancing!

## System 1: Mesh Instancing & Resource Management

### Problem
- Loading same mesh 100+ times wastes memory (11x11 room = 121 tiles)
- Rendering 100+ draw calls kills performance
- Need shared GPU resources (vertex buffers, textures)

### Solution: Instance-Based Rendering

```cpp
namespace lore::world {

// Mesh resource manager (singleton, owned by graphics system)
class TileMeshCache {
public:
    struct MeshHandle {
        uint32_t mesh_id;              // Unique mesh ID
        VkBuffer vertex_buffer;        // Shared vertex data
        VkBuffer index_buffer;         // Shared index data
        uint32_t index_count;
        uint32_t reference_count;      // Track usage
    };

    // Load mesh from file (cached, ref-counted)
    MeshHandle* acquire_mesh(const std::string& mesh_path);

    // Release mesh reference (unloads when ref_count == 0)
    void release_mesh(uint32_t mesh_id);

    // Get mesh data for rendering
    const MeshHandle* get_mesh(uint32_t mesh_id) const;

private:
    std::unordered_map<std::string, MeshHandle> mesh_cache_;
    std::unordered_map<uint32_t, MeshHandle*> id_to_mesh_;
    uint32_t next_mesh_id_ = 1;
};

// Extended TileDefinition with cached mesh handle
struct TileDefinition {
    uint32_t id;
    std::string mesh_path;

    // NEW: Cached mesh handle (loaded on first use)
    mutable uint32_t cached_mesh_id = 0;

    // ... existing properties
};

// Per-frame instance data (uploaded to GPU)
struct TileInstanceGPU {
    glm::mat4 transform;           // Position + rotation
    glm::vec4 tint_color;          // RGB + health (alpha channel)
    uint32_t material_id;
    uint32_t flags;                // Bitfield: is_broken, is_active, etc.
};

} // namespace lore::world
```

### Rendering Pipeline

```cpp
// In DeferredRenderer or new TileRenderer class
class TileRenderer {
public:
    void render_tiles(const TilemapWorldSystem& world) {
        // 1. Group instances by mesh_id
        std::unordered_map<uint32_t, std::vector<TileInstanceGPU>> instances_by_mesh;

        for (const auto& chunk : world.get_all_chunks()) {
            for (const auto& tile : chunk.second->tiles) {
                if (!tile.is_active) continue;

                const auto* def = world.get_tile_definition(tile.definition_id);
                if (!def->cached_mesh_id) {
                    def->cached_mesh_id = mesh_cache_.acquire_mesh(def->mesh_path)->mesh_id;
                }

                // Build instance data
                TileInstanceGPU gpu_instance;
                gpu_instance.transform = build_transform(tile);
                gpu_instance.tint_color = glm::vec4(def->tint_color, tile.health);
                gpu_instance.material_id = def->material_id;
                gpu_instance.flags = build_flags(tile);

                instances_by_mesh[def->cached_mesh_id].push_back(gpu_instance);
            }
        }

        // 2. Render each mesh once with all instances
        for (const auto& [mesh_id, instances] : instances_by_mesh) {
            const auto* mesh = mesh_cache_.get_mesh(mesh_id);

            // Upload instance data to GPU buffer
            update_instance_buffer(instances);

            // Single draw call for ALL instances of this mesh
            vkCmdDrawIndexedIndirect(
                command_buffer,
                mesh->index_buffer,
                mesh->index_count,
                instances.size()  // GPU draws N instances
            );
        }
    }

private:
    TileMeshCache mesh_cache_;
    VkBuffer instance_buffer_;  // GPU buffer for instance data
};
```

**Performance:**
- 2 unique meshes (Cube, FloorTile) = 2 draw calls (instead of 121!)
- Single vertex/index buffer per mesh
- GPU handles instancing via `vkCmdDrawIndexed` with instance count

## System 2: Tile Breaking & Destruction

### Problem
- Tiles can be damaged and destroyed
- Need to transition from shared mesh to per-tile broken geometry
- Must handle partial destruction (cracks, holes)
- Integration with voxel-based liquid/smoke simulation

### Solution: Progressive Destruction States

```cpp
namespace lore::world {

enum class TileState : uint8_t {
    Intact,           // Normal shared mesh
    Damaged,          // Cracks/damage overlays (still shared mesh + decals)
    Fractured,        // Breaking apart (shared mesh + fracture particles)
    Broken,           // Fully destroyed - transition to debris/voxels
    Debris            // Individual voxel pieces
};

struct TileInstance {
    uint32_t definition_id;
    TileCoord coord;
    float rotation_degrees;
    float health = 1.0f;          // 1.0 = intact, 0.0 = destroyed

    // NEW: Destruction state
    TileState state = TileState::Intact;

    // NEW: Broken tiles become unique geometry
    std::unique_ptr<BrokenTileGeometry> broken_geometry;

    // ... existing fields
};

// Debris/broken tile geometry (per-tile unique)
struct BrokenTileGeometry {
    std::vector<VoxelPiece> voxel_pieces;  // For physics simulation
    VkBuffer unique_vertex_buffer;          // Custom broken mesh
    VkBuffer unique_index_buffer;
    bool needs_gpu_upload = true;
};

struct VoxelPiece {
    math::Vec3 position;         // Sub-tile position
    math::Vec3 velocity;         // Physics velocity
    float mass;                  // For physics
    uint32_t material_id;        // Visual material
    bool is_simulated = true;    // False = settled debris
};

} // namespace lore::world
```

### Destruction State Machine

```cpp
class TileDestructionSystem {
public:
    void damage_tile(TileInstance& tile, float damage_amount) {
        tile.health = std::max(0.0f, tile.health - damage_amount);

        // State transitions based on health
        if (tile.health <= 0.0f && tile.state != TileState::Broken) {
            break_tile(tile);
        } else if (tile.health <= 0.3f && tile.state == TileState::Intact) {
            tile.state = TileState::Fractured;
            // Visual: Add crack decals, particle effects
        } else if (tile.health <= 0.6f && tile.state == TileState::Intact) {
            tile.state = TileState::Damaged;
            // Visual: Add minor damage decals
        }
    }

private:
    void break_tile(TileInstance& tile) {
        tile.state = TileState::Broken;

        // Generate voxel pieces from original mesh
        tile.broken_geometry = std::make_unique<BrokenTileGeometry>();
        tile.broken_geometry->voxel_pieces = voxelize_mesh(tile);

        // Notify liquid/smoke simulation system
        voxel_simulation_->on_tile_broken(tile.coord, tile.broken_geometry->voxel_pieces);

        // Apply physics forces to pieces
        apply_destruction_forces(tile.broken_geometry->voxel_pieces);
    }

    std::vector<VoxelPiece> voxelize_mesh(const TileInstance& tile) {
        // Convert mesh to voxel grid
        const auto* def = world_->get_tile_definition(tile.definition_id);
        const auto* mesh = mesh_cache_->get_mesh(def->cached_mesh_id);

        // Divide tile volume into voxels (e.g., 8x8x8 = 512 voxels)
        constexpr int32_t VOXELS_PER_AXIS = 8;
        float voxel_size = TilemapWorldSystem::TILE_SIZE / VOXELS_PER_AXIS;

        std::vector<VoxelPiece> pieces;
        pieces.reserve(VOXELS_PER_AXIS * VOXELS_PER_AXIS * VOXELS_PER_AXIS);

        // Sample mesh at voxel positions
        for (int32_t x = 0; x < VOXELS_PER_AXIS; ++x) {
            for (int32_t y = 0; y < VOXELS_PER_AXIS; ++y) {
                for (int32_t z = 0; z < VOXELS_PER_AXIS; ++z) {
                    math::Vec3 local_pos(
                        x * voxel_size,
                        y * voxel_size,
                        z * voxel_size
                    );

                    // Check if voxel intersects mesh geometry
                    if (voxel_intersects_mesh(local_pos, voxel_size, mesh)) {
                        VoxelPiece piece;
                        piece.position = tile_to_world(tile.coord) + local_pos;
                        piece.velocity = math::Vec3(0.0f, 0.0f, 0.0f);
                        piece.mass = voxel_size * voxel_size * voxel_size * def->density;
                        piece.material_id = def->material_id;
                        pieces.push_back(piece);
                    }
                }
            }
        }

        return pieces;
    }
};
```

## System 3: Voxel Simulation Integration

### Liquid/Smoke Simulation Architecture

```cpp
namespace lore::physics {

// Voxel-based fluid simulation (runs on GPU compute shader)
class VoxelFluidSimulation {
public:
    static constexpr int32_t VOXELS_PER_TILE = 8;  // 8x8x8 = 512 voxels per tile

    struct FluidVoxel {
        float density;           // Liquid/smoke density
        math::Vec3 velocity;     // Fluid velocity field
        float temperature;       // For convection
        uint32_t material_type;  // Water, lava, smoke, etc.
    };

    // Initialize fluid simulation for tile region
    void initialize_tile_region(const TileCoord& coord, const TileDefinition& def);

    // Called when tile breaks - existing fluid fills broken space
    void on_tile_broken(const TileCoord& coord, const std::vector<VoxelPiece>& debris);

    // Update fluid simulation (GPU compute shader)
    void update(float delta_time);

    // Query fluid state at world position
    FluidVoxel sample_fluid(const math::Vec3& world_pos) const;

private:
    // 3D texture on GPU containing voxel grid
    VkImage voxel_grid_texture_;
    VkBuffer voxel_grid_buffer_;

    // Compute shader for Navier-Stokes fluid dynamics
    VkPipeline fluid_simulation_pipeline_;

    // Track active simulation regions (chunk-based)
    std::unordered_map<ChunkCoord, SimulationRegion> active_regions_;
};

} // namespace lore::physics
```

### Integration with Tile System

```cpp
// In TileDestructionSystem::break_tile()
void break_tile(TileInstance& tile) {
    tile.state = TileState::Broken;

    // 1. Generate voxel debris
    tile.broken_geometry = std::make_unique<BrokenTileGeometry>();
    tile.broken_geometry->voxel_pieces = voxelize_mesh(tile);

    // 2. Notify fluid simulation - remove solid voxels, allow flow
    voxel_simulation_->on_tile_broken(tile.coord, tile.broken_geometry->voxel_pieces);

    // 3. Check for neighboring liquids that can now flow into broken space
    for (const auto& neighbor : get_neighbors(tile.coord)) {
        auto fluid = voxel_simulation_->sample_fluid(tile_to_world(neighbor));
        if (fluid.density > 0.0f) {
            // Trigger fluid flow into newly opened space
            voxel_simulation_->trigger_flow(neighbor, tile.coord);
        }
    }

    // 4. Apply explosion/destruction forces to debris pieces
    apply_destruction_forces(tile.broken_geometry->voxel_pieces);
}

// Fluid simulation responds to broken tile
void VoxelFluidSimulation::on_tile_broken(
    const TileCoord& coord,
    const std::vector<VoxelPiece>& debris)
{
    // Convert tile coordinate to voxel region
    int32_t base_x = coord.x * VOXELS_PER_TILE;
    int32_t base_y = coord.y * VOXELS_PER_TILE;
    int32_t base_z = coord.z * VOXELS_PER_TILE;

    // Mark voxels as non-solid (allowing fluid flow)
    for (int32_t x = 0; x < VOXELS_PER_TILE; ++x) {
        for (int32_t y = 0; y < VOXELS_PER_TILE; ++y) {
            for (int32_t z = 0; z < VOXELS_PER_TILE; ++z) {
                VoxelCoord voxel{base_x + x, base_y + y, base_z + z};

                // Check if debris piece occupies this voxel
                bool has_debris = false;
                for (const auto& piece : debris) {
                    if (world_to_voxel(piece.position) == voxel) {
                        has_debris = true;
                        break;
                    }
                }

                if (!has_debris) {
                    // Mark as empty - fluid can flow here
                    set_voxel_solid(voxel, false);
                }
            }
        }
    }

    // GPU compute shader will handle fluid flow on next update
    mark_region_dirty(coord);
}
```

## Performance Considerations

### Memory Management

**Intact Tiles (99% of world):**
- Share single mesh: ~1MB vertex data
- TileInstance: 64 bytes per tile
- 10,000 tiles = 640KB + 1MB mesh = **1.6MB total**

**Broken Tiles (1% of world):**
- Unique geometry: ~100KB per broken tile
- 100 broken tiles = 10MB
- Voxel pieces: 512 voxels × 32 bytes = 16KB per tile
- **Total: ~11.6MB for 100 broken tiles**

### Render Performance

**Before Instancing:**
- 10,000 tiles × 2 meshes = 20,000 draw calls
- **Frame time: ~300ms (unacceptable)**

**After Instancing:**
- 2 unique meshes = 2 draw calls (intact tiles)
- 100 broken tiles = 100 draw calls (unique geometry)
- **Total: 102 draw calls (~1.5ms)**

### LOD System (Future)

```cpp
// Distance-based level of detail
enum class TileLOD : uint8_t {
    High,      // Full mesh + voxel simulation (near player)
    Medium,    // Simplified mesh, no voxel sim
    Low,       // Billboard impostor
    Culled     // Outside view frustum
};

// In TileRenderer::render_tiles()
TileLOD calculate_lod(const TileCoord& coord, const math::Vec3& camera_pos) {
    float distance = length(tile_to_world(coord) - camera_pos);

    if (distance < 20.0f) return TileLOD::High;
    if (distance < 50.0f) return TileLOD::Medium;
    if (distance < 100.0f) return TileLOD::Low;
    return TileLOD::Culled;
}
```

## Implementation Plan

### Phase 1: Mesh Instancing (Week 1)
1. Implement `TileMeshCache` with ref-counting
2. Add `cached_mesh_id` to `TileDefinition`
3. Modify `TiledImporter` to use mesh cache
4. Implement `TileRenderer` with instanced rendering
5. Test with test_room (121 tiles → 2 draw calls)

### Phase 2: Basic Destruction (Week 2)
1. Add `TileState` enum and state machine
2. Implement `damage_tile()` and health tracking
3. Add visual feedback (cracks, particles)
4. Test with explosive projectiles

### Phase 3: Voxelization (Week 3)
1. Implement `voxelize_mesh()` for debris generation
2. Add `BrokenTileGeometry` with unique buffers
3. Physics simulation for debris pieces
4. Collision detection for voxel pieces

### Phase 4: Fluid Integration (Week 4)
1. Implement `VoxelFluidSimulation` GPU compute shaders
2. Add `on_tile_broken()` fluid flow logic
3. Test water flooding through broken walls
4. Add smoke/gas propagation

## API Usage Examples

```cpp
// Example: Explosion breaks tiles
void apply_explosion(const math::Vec3& position, float radius, float damage) {
    auto tiles = world.get_tiles_in_sphere(position, radius);

    for (auto* tile : tiles) {
        float distance = length(position - tile_to_world(tile->coord));
        float falloff = 1.0f - (distance / radius);

        destruction_system.damage_tile(*tile, damage * falloff);
    }
}

// Example: Water flows through broken wall
if (tile->state == TileState::Broken) {
    auto fluid = voxel_simulation.sample_fluid(tile_to_world(tile->coord));
    if (fluid.density > 0.5f) {
        // Render water flowing through broken wall
        render_fluid_surface(tile->coord, fluid);
    }
}
```

## Conclusion

This architecture provides:
- ✅ **Efficient mesh instancing** - shared resources, minimal draw calls
- ✅ **Progressive destruction** - intact → damaged → broken states
- ✅ **Voxel integration** - seamless transition to voxel-based debris
- ✅ **Fluid simulation** - realistic liquid/smoke flow through broken tiles
- ✅ **Scalability** - handles 100K+ tiles with <5ms render time

The key insight is: **Keep most tiles intact (shared mesh) and only create unique geometry when breaking occurs**.
