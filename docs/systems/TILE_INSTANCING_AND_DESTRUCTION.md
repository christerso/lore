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

### Solution: Voronoi Fracture System

**NO CUBIC VOXELS** - We use Voronoi diagram fracturing for realistic irregular debris.

```cpp
namespace lore::world {

enum class TileState : uint8_t {
    Intact,           // Normal shared mesh
    Damaged,          // Cracks/damage overlays (still shared mesh + decals)
    Fractured,        // Breaking apart (shared mesh + fracture particles)
    Broken,           // Fully destroyed - transition to debris pieces
    Debris            // Individual irregular fragments
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
    std::vector<DebrisPiece> fragments;     // Irregular Voronoi pieces
    VkBuffer unique_vertex_buffer;           // Combined mesh for all pieces
    VkBuffer unique_index_buffer;
    bool needs_gpu_upload = true;
};

// Individual debris fragment (irregular shape, NOT a cube)
struct DebrisPiece {
    // Visual geometry
    std::vector<math::Vec3> vertices;       // Local space vertices
    std::vector<uint32_t> indices;          // Triangle indices
    math::Vec3 centroid;                    // Center of mass

    // Physics properties
    math::Vec3 position;                    // World position
    math::Quat rotation;                    // Orientation quaternion
    math::Vec3 velocity;                    // Linear velocity
    math::Vec3 angular_velocity;            // Rotational velocity
    float mass;                             // For physics
    math::Vec3 inertia_tensor;              // Rotational inertia

    // Visual properties
    uint32_t material_id;                   // PBR material
    std::vector<math::Vec2> uvs;            // Texture coordinates
    std::vector<math::Vec3> normals;        // Per-vertex normals

    // Simulation state
    bool is_simulated = true;               // False = settled/sleeping
    float lifetime = 30.0f;                 // Auto-cleanup after N seconds

    // Voxel approximation for fluid simulation (internal use only)
    std::vector<VoxelCell> voxel_approximation;  // For smoke/liquid interaction
};

// Voxel cell for fluid simulation (INTERNAL - not visible)
struct VoxelCell {
    math::Vec3 center;                      // Local position within piece
    float radius;                           // Bounding sphere radius
    bool is_occupied;                       // Solid vs empty
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

    std::vector<DebrisPiece> fracture_mesh_voronoi(const TileInstance& tile) {
        // Use Voronoi fracture for realistic irregular debris
        const auto* def = world_->get_tile_definition(tile.definition_id);
        const auto* mesh = mesh_cache_->get_mesh(def->cached_mesh_id);

        // 1. Generate random fracture points (Voronoi cell centers)
        constexpr int32_t NUM_FRAGMENTS = 15;  // Configurable: 8-30 pieces typical
        std::vector<math::Vec3> fracture_points;
        fracture_points.reserve(NUM_FRAGMENTS);

        // Random points within tile bounds with bias toward impact point
        math::Vec3 tile_center = tile_to_world(tile.coord);
        float tile_size = TilemapWorldSystem::TILE_SIZE;

        for (int32_t i = 0; i < NUM_FRAGMENTS; ++i) {
            // Poisson disk sampling prevents clustered points
            math::Vec3 point = generate_poisson_point(tile_center, tile_size);
            fracture_points.push_back(point);
        }

        // 2. Compute Voronoi diagram (each cell becomes a debris piece)
        VoronoiDiagram voronoi = compute_3d_voronoi(fracture_points, tile_center, tile_size);

        // 3. Clip Voronoi cells against original mesh geometry
        std::vector<DebrisPiece> fragments;
        fragments.reserve(NUM_FRAGMENTS);

        for (const auto& cell : voronoi.cells) {
            DebrisPiece piece;

            // Intersect Voronoi cell with mesh to get realistic fragment shape
            clip_voronoi_cell_against_mesh(cell, mesh, piece);

            if (piece.vertices.empty()) {
                continue;  // Cell doesn't intersect mesh
            }

            // Calculate physics properties
            piece.centroid = calculate_centroid(piece.vertices);
            piece.mass = calculate_volume(piece.vertices, piece.indices) * def->density;
            piece.inertia_tensor = calculate_inertia_tensor(piece.vertices, piece.mass);

            // Transfer material and UVs from original mesh
            transfer_material_properties(mesh, piece);

            // Generate voxel approximation for fluid simulation (internal only)
            piece.voxel_approximation = approximate_with_voxels(piece, 4);  // 4x4x4 internal

            // Set initial physics state
            piece.position = tile_center;
            piece.rotation = math::Quat::identity();
            piece.velocity = math::Vec3(0.0f, 0.0f, 0.0f);
            piece.angular_velocity = math::Vec3(0.0f, 0.0f, 0.0f);
            piece.material_id = def->material_id;

            fragments.push_back(std::move(piece));
        }

        return fragments;
    }

    // Voronoi 3D computation
    VoronoiDiagram compute_3d_voronoi(
        const std::vector<math::Vec3>& points,
        const math::Vec3& center,
        float size)
    {
        // Use Fortune's algorithm extended to 3D or voro++ library
        VoronoiDiagram diagram;

        // Define bounding box for Voronoi diagram
        math::Vec3 min_bounds = center - math::Vec3(size * 0.5f);
        math::Vec3 max_bounds = center + math::Vec3(size * 0.5f);

        // For each Voronoi cell, compute convex polyhedron
        for (size_t i = 0; i < points.size(); ++i) {
            VoronoiCell cell;
            cell.center = points[i];

            // Find half-space planes from neighboring points
            for (size_t j = 0; j < points.size(); ++j) {
                if (i == j) continue;

                // Perpendicular bisector plane between points[i] and points[j]
                math::Vec3 midpoint = (points[i] + points[j]) * 0.5f;
                math::Vec3 normal = normalize(points[i] - points[j]);

                cell.planes.push_back({midpoint, normal});
            }

            // Clip against bounding box
            add_bounding_box_planes(cell, min_bounds, max_bounds);

            // Compute vertices by intersecting planes
            cell.vertices = compute_polyhedron_vertices(cell.planes);

            diagram.cells.push_back(cell);
        }

        return diagram;
    }

    // Clip Voronoi cell against mesh geometry
    void clip_voronoi_cell_against_mesh(
        const VoronoiCell& cell,
        const MeshHandle* mesh,
        DebrisPiece& out_piece)
    {
        // 1. Start with Voronoi cell polyhedron
        std::vector<math::Vec3> clipped_vertices = cell.vertices;
        std::vector<uint32_t> clipped_indices = triangulate_convex_polyhedron(cell.vertices);

        // 2. For each triangle in original mesh, clip Voronoi cell
        for (size_t i = 0; i < mesh->index_count; i += 3) {
            math::Vec3 v0 = mesh->vertices[mesh->indices[i + 0]];
            math::Vec3 v1 = mesh->vertices[mesh->indices[i + 1]];
            math::Vec3 v2 = mesh->vertices[mesh->indices[i + 2]];

            // Clip cell against triangle (Sutherland-Hodgman algorithm)
            clip_polyhedron_against_triangle(clipped_vertices, clipped_indices, v0, v1, v2);
        }

        // 3. Result is intersection of Voronoi cell and mesh
        out_piece.vertices = clipped_vertices;
        out_piece.indices = clipped_indices;

        // Generate normals for new geometry
        compute_vertex_normals(out_piece.vertices, out_piece.indices, out_piece.normals);
    }

    // Generate internal voxel approximation for fluid simulation
    std::vector<VoxelCell> approximate_with_voxels(const DebrisPiece& piece, int32_t resolution) {
        // Create coarse voxel grid INSIDE the irregular piece for fluid interaction
        // These voxels are NEVER rendered - only used for collision with liquids/smoke

        math::Vec3 aabb_min, aabb_max;
        compute_aabb(piece.vertices, aabb_min, aabb_max);

        float voxel_size = std::max({
            (aabb_max.x - aabb_min.x) / resolution,
            (aabb_max.y - aabb_min.y) / resolution,
            (aabb_max.z - aabb_min.z) / resolution
        });

        std::vector<VoxelCell> voxels;

        // Sample interior of piece
        for (int32_t x = 0; x < resolution; ++x) {
            for (int32_t y = 0; y < resolution; ++y) {
                for (int32_t z = 0; z < resolution; ++z) {
                    math::Vec3 pos(
                        aabb_min.x + (x + 0.5f) * voxel_size,
                        aabb_min.y + (y + 0.5f) * voxel_size,
                        aabb_min.z + (z + 0.5f) * voxel_size
                    );

                    // Check if point is inside irregular mesh
                    if (point_inside_mesh(pos, piece.vertices, piece.indices)) {
                        VoxelCell cell;
                        cell.center = pos;
                        cell.radius = voxel_size * 0.5f;
                        cell.is_occupied = true;
                        voxels.push_back(cell);
                    }
                }
            }
        }

        return voxels;
    }
};
```

## Visual Comparison: Cubic Voxels vs Voronoi Fracture

### ❌ BAD: Cubic Voxel Debris (What We DON'T Want)
```
Wall breaks into 512 perfect cubes:
┌──┬──┬──┐
│  │  │  │  ← Uniform cubes
├──┼──┼──┤  ← Looks like Minecraft
│  │  │  │  ← Unrealistic and boring
└──┴──┴──┘
```
**Problems:**
- Looks like Minecraft/low-poly indie game
- No visual variation between materials (stone vs wood both = cubes)
- Boring physics (cubes don't tumble realistically)
- Doesn't preserve original mesh detail

### ✅ GOOD: Voronoi Fracture Debris (What We DO Want)
```
Wall breaks into 15-30 irregular chunks:
    ╱╲
   ╱  ╲___
  ╱       ╲___
 ╱   ╱╲      ╲  ← Irregular polygons
╱___╱  ╲___   ╲ ← Natural looking
    ╲      ╲___╲ ← Preserves mesh detail
     ╲________╱
```
**Benefits:**
- Realistic irregular shapes like real-world fractures
- Preserves original mesh textures and materials
- Each break is unique (procedural variation)
- Proper physics with rotation and tumbling
- Can be large chunks or small shards depending on force

### Visual References

**Stone Wall Breaking:**
- Large angular chunks (5-8 pieces) for solid stone
- Irregular polyhedra with sharp edges
- Dust particles at fracture edges

**Wooden Crate Breaking:**
- Splinter-like shards (15-25 pieces)
- Elongated along wood grain
- Some pieces retain planks/boards

**Glass Shattering:**
- Many small irregular shards (20-40 pieces)
- Sharp triangular pieces
- Reflective material preserved on each shard

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

    // 1. Generate irregular Voronoi debris (VISUAL)
    tile.broken_geometry = std::make_unique<BrokenTileGeometry>();
    tile.broken_geometry->fragments = fracture_mesh_voronoi(tile);

    // 2. Notify fluid simulation - remove solid voxels, allow flow
    //    Each irregular debris piece has internal voxel approximation
    voxel_simulation_->on_tile_broken(tile.coord, tile.broken_geometry->fragments);

    // 3. Check for neighboring liquids that can now flow into broken space
    for (const auto& neighbor : get_neighbors(tile.coord)) {
        auto fluid = voxel_simulation_->sample_fluid(tile_to_world(neighbor));
        if (fluid.density > 0.0f) {
            // Trigger fluid flow into newly opened space
            voxel_simulation_->trigger_flow(neighbor, tile.coord);
        }
    }

    // 4. Apply explosion/destruction forces to irregular debris pieces
    apply_destruction_forces(tile.broken_geometry->fragments);
}

// Fluid simulation responds to broken tile with irregular debris
void VoxelFluidSimulation::on_tile_broken(
    const TileCoord& coord,
    const std::vector<DebrisPiece>& irregular_fragments)
{
    // Convert tile coordinate to voxel region
    int32_t base_x = coord.x * VOXELS_PER_TILE;
    int32_t base_y = coord.y * VOXELS_PER_TILE;
    int32_t base_z = coord.z * VOXELS_PER_TILE;

    // Mark voxels as non-solid (allowing fluid flow)
    for (int32_t x = 0; x < VOXELS_PER_TILE; ++x) {
        for (int32_t y = 0; y < VOXELS_PER_TILE; ++y) {
            for (int32_t z = 0; z < VOXELS_PER_TILE; ++z) {
                math::Vec3 voxel_world_pos(
                    (base_x + x + 0.5f) * voxel_size_,
                    (base_y + y + 0.5f) * voxel_size_,
                    (base_z + z + 0.5f) * voxel_size_
                );

                // Check if any irregular debris piece occupies this voxel
                bool has_debris = false;
                for (const auto& piece : irregular_fragments) {
                    // Use internal voxel approximation of irregular piece
                    for (const auto& voxel_cell : piece.voxel_approximation) {
                        math::Vec3 cell_world_pos = piece.position + voxel_cell.center;
                        float dist = length(cell_world_pos - voxel_world_pos);

                        if (dist < voxel_cell.radius) {
                            has_debris = true;
                            break;
                        }
                    }
                    if (has_debris) break;
                }

                if (!has_debris) {
                    // Mark as empty - fluid can flow here
                    set_voxel_solid({base_x + x, base_y + y, base_z + z}, false);
                }
            }
        }
    }

    // GPU compute shader will handle fluid flow on next update
    mark_region_dirty(coord);
}

// Update debris voxel approximations as pieces move/rotate
void update_debris_fluid_interaction(std::vector<DebrisPiece>& fragments, float delta_time) {
    for (auto& piece : fragments) {
        if (!piece.is_simulated) continue;

        // Update voxel cell positions based on piece transform
        for (auto& voxel_cell : piece.voxel_approximation) {
            // Transform voxel center by piece's rotation and position
            math::Vec3 rotated = piece.rotation * voxel_cell.center;
            math::Vec3 world_pos = piece.position + rotated;

            // Check for fluid interaction at this voxel position
            FluidVoxel fluid = sample_fluid(world_pos);

            if (fluid.density > 0.3f) {
                // Apply buoyancy force to piece (Archimedes principle)
                float buoyancy_force = voxel_cell.radius * voxel_cell.radius * voxel_cell.radius
                                     * fluid.density * 9.81f;

                math::Vec3 buoyancy = math::Vec3(0.0f, 0.0f, buoyancy_force);
                apply_force_at_point(piece, buoyancy, world_pos);

                // Apply fluid drag
                math::Vec3 relative_velocity = piece.velocity - fluid.velocity;
                math::Vec3 drag = -relative_velocity * 0.5f * fluid.density;
                apply_force(piece, drag);
            }
        }
    }
}
```

## Performance Considerations

### Memory Management

**Intact Tiles (99% of world):**
- Share single mesh: ~1MB vertex data for Cube.fbx
- TileInstance: 64 bytes per tile
- 10,000 tiles = 640KB + 1MB mesh = **1.6MB total**

**Broken Tiles (1% of world) - Voronoi Fracture:**
- Irregular debris: 15-30 pieces per tile
- Each piece: ~200 vertices × 12 bytes = 2.4KB
- Average 20 pieces × 2.4KB = **48KB per broken tile**
- Internal voxel approximation: 4×4×4 = 64 voxels × 16 bytes = 1KB
- **Total per broken tile: ~50KB**
- 100 broken tiles = **5MB** (much better than cubic voxels!)

**Visual Quality Comparison:**
- ❌ Cubic voxels: 512 pieces × 8 vertices = 4096 verts per tile (49KB but looks bad)
- ✅ Voronoi: 20 pieces × 200 vertices = 4000 verts per tile (48KB and looks realistic!)
- **Same memory, vastly better visuals**

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
