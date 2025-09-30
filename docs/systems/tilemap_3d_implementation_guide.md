# 3D Tilemap System - Implementation Guide

## Overview

This document provides complete structs and implementation guidelines for building a 3D tilemap-based world generation system. You'll write the logic, but all data structures and architectural decisions are provided here.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Game Loop                            │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   TilemapWorldSystem                        │
│  - Manages chunks                                           │
│  - Loads/unloads based on player position                   │
│  - Spawns entities from tile definitions                    │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │  Chunk   │    │  Chunk   │    │  Chunk   │
    │ (16x16x4)│    │ (16x16x4)│    │ (16x16x4)│
    └──────────┘    └──────────┘    └──────────┘
           │               │               │
           └───────────────┴───────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │  TileDefinitionRegistry│
              │  - Wall definitions    │
              │  - Floor definitions   │
              │  - Entity definitions  │
              └────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │   ECS (Entity Spawn)   │
              │  - Create entities     │
              │  - Add Transform       │
              │  - Add Mesh            │
              │  - Add Collider        │
              └────────────────────────┘
```

## Core Data Structures

### 1. Tile Coordinate System

```cpp
namespace lore::world {

// 3D tile coordinate (integer grid position)
struct TileCoord {
    int32_t x;
    int32_t y;
    int32_t z;

    bool operator==(const TileCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator<(const TileCoord& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

// Chunk coordinate (each chunk is 16x16x4 tiles)
struct ChunkCoord {
    int32_t chunk_x;
    int32_t chunk_y;
    int32_t chunk_z;

    bool operator==(const ChunkCoord& other) const {
        return chunk_x == other.chunk_x &&
               chunk_y == other.chunk_y &&
               chunk_z == other.chunk_z;
    }
};

// Convert tile coordinate to chunk coordinate
inline ChunkCoord tile_to_chunk(const TileCoord& tile) {
    return ChunkCoord{
        tile.x >> 4,  // Divide by 16
        tile.y >> 4,
        tile.z >> 2   // Divide by 4 (fewer vertical chunks)
    };
}

// Convert chunk coordinate to world position (meters)
inline math::Vec3 chunk_to_world_pos(const ChunkCoord& chunk) {
    return math::Vec3(
        static_cast<float>(chunk.chunk_x * 16),
        static_cast<float>(chunk.chunk_y * 16),
        static_cast<float>(chunk.chunk_z * 4)
    );
}

} // namespace lore::world
```

### 2. Tile Definition System

```cpp
namespace lore::world {

// Tile type categories
enum class TileCategory : uint8_t {
    Air,         // Empty space (no entity spawned)
    Floor,       // Walkable ground
    Wall,        // Solid obstacle
    Door,        // Interactive portal
    Furniture,   // Decorative/interactive object
    Entity       // Special entity spawn point
};

// How the tile occupies space
enum class TileOccupancy : uint8_t {
    Single,      // 1x1x1 (most tiles)
    Multi_2x2,   // 2x2x1 (large furniture)
    Multi_3x3,   // 3x3x1 (very large objects)
    Multi_1x1x2  // 1x1x2 (tall objects like doors)
};

// Tile definition - describes what gets spawned
struct TileDefinition {
    uint32_t id;                     // Unique tile ID
    std::string name;                // Human-readable name
    TileCategory category;           // What kind of tile is this?
    TileOccupancy occupancy;         // How much space does it take?

    // Visual representation
    std::string mesh_path;           // Path to 3D mesh (e.g., "meshes/wall_stone.obj")
    std::string texture_path;        // Optional texture override
    math::Vec3 mesh_scale;           // Scale factor for mesh
    math::Vec3 mesh_offset;          // Offset from tile center

    // Physics
    bool has_collision;              // Should this spawn a collider?
    std::string collision_shape;     // "box", "sphere", "mesh", "capsule"
    math::Vec3 collision_extents;    // Size of collision volume

    // Gameplay properties
    bool is_walkable;                // Can entities walk on this?
    bool blocks_sight;               // Does this block line of sight?
    bool is_interactable;            // Can player interact with this?
    std::string interaction_type;    // "door", "chest", "lever", etc.

    // Material properties (for deferred renderer)
    uint32_t pbr_material_id;        // Index into DeferredRenderer materials

    // Multi-tile support
    std::vector<TileCoord> additional_occupied; // For multi-tile objects
};

} // namespace lore::world
```

### 3. Tile Instance (placed in world)

```cpp
namespace lore::world {

// A placed tile in the world
struct TileInstance {
    TileCoord position;              // Where is this tile?
    uint32_t definition_id;          // Which TileDefinition to use
    ecs::EntityID spawned_entity;    // The ECS entity created for this tile
    uint8_t rotation;                // 0-3 (90 degree increments)
    bool is_loaded;                  // Has the entity been spawned yet?

    // Optional per-instance overrides
    math::Vec3 custom_scale;         // Override definition scale
    uint32_t custom_material;        // Override definition material
};

} // namespace lore::world
```

### 4. Chunk System

```cpp
namespace lore::world {

// Chunk dimensions
constexpr int32_t CHUNK_SIZE_X = 16;
constexpr int32_t CHUNK_SIZE_Y = 16;
constexpr int32_t CHUNK_SIZE_Z = 4;
constexpr int32_t CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// Chunk load state
enum class ChunkState : uint8_t {
    Unloaded,    // Not in memory
    Loading,     // Being loaded from disk/generation
    Loaded,      // In memory, entities spawned
    Unloading    // Being removed
};

// A chunk of the world (16x16x4 tiles)
struct Chunk {
    ChunkCoord coord;                              // Chunk position
    ChunkState state;                              // Current state

    // Tile storage (flat array, indexed as [x + y * 16 + z * 256])
    std::array<TileInstance, CHUNK_VOLUME> tiles;

    // Quick access to spawned entities
    std::vector<ecs::EntityID> entities;

    // Timestamp for LRU eviction
    std::chrono::steady_clock::time_point last_access;

    // Helper to index into tiles array
    inline TileInstance& get_tile(int32_t x, int32_t y, int32_t z) {
        return tiles[x + y * CHUNK_SIZE_X + z * (CHUNK_SIZE_X * CHUNK_SIZE_Y)];
    }

    inline const TileInstance& get_tile(int32_t x, int32_t y, int32_t z) const {
        return tiles[x + y * CHUNK_SIZE_X + z * (CHUNK_SIZE_X * CHUNK_SIZE_Y)];
    }
};

} // namespace lore::world
```

### 5. Tilemap World System

```cpp
namespace lore::world {

// Configuration for the tilemap system
struct TilemapConfig {
    int32_t render_distance_chunks = 4;    // Load chunks within this radius
    int32_t unload_distance_chunks = 6;    // Unload chunks beyond this radius
    size_t max_loaded_chunks = 256;        // Memory limit
    bool enable_chunk_generation = true;   // Generate new chunks on demand
    std::string world_save_path = "saves/world.dat";
};

class TilemapWorldSystem {
public:
    TilemapWorldSystem(ecs::EntityManager& entity_manager);
    ~TilemapWorldSystem();

    // Lifecycle
    void initialize(const TilemapConfig& config);
    void shutdown();
    void update(float delta_time);

    // Tile definition management
    void register_tile_definition(const TileDefinition& def);
    const TileDefinition* get_tile_definition(uint32_t id) const;
    void load_tile_definitions(const std::string& json_path);

    // World manipulation
    void set_tile(const TileCoord& pos, uint32_t definition_id, uint8_t rotation = 0);
    void remove_tile(const TileCoord& pos);
    const TileInstance* get_tile(const TileCoord& pos) const;

    // Chunk management
    void load_chunk(const ChunkCoord& coord);
    void unload_chunk(const ChunkCoord& coord);
    bool is_chunk_loaded(const ChunkCoord& coord) const;

    // Player tracking (for chunk loading/unloading)
    void set_player_position(const math::Vec3& world_pos);

    // Serialization
    void save_world(const std::string& path);
    void load_world(const std::string& path);

private:
    // Internal helpers (YOU IMPLEMENT THESE)
    void update_chunk_loading();
    void spawn_tile_entity(TileInstance& tile, const TileDefinition& def);
    void despawn_tile_entity(TileInstance& tile);
    Chunk* get_or_create_chunk(const ChunkCoord& coord);

    // Data members
    TilemapConfig config_;
    ecs::EntityManager& entity_manager_;

    std::unordered_map<uint32_t, TileDefinition> tile_definitions_;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> loaded_chunks_;

    math::Vec3 player_position_;
    ChunkCoord player_chunk_;
};

} // namespace lore::world
```

## Implementation Guidelines

### Phase 1: Core Data Structures (YOU DO THIS)

1. **Create header file**: `include/lore/world/tilemap_world.hpp`
   - Copy all structs above
   - Add hash specializations for TileCoord and ChunkCoord (for unordered_map)

2. **Create implementation file**: `src/world/tilemap_world.cpp`
   - Implement TilemapWorldSystem constructor/destructor
   - Implement initialize/shutdown (initialize maps, clear state)

### Phase 2: Tile Definition System (YOU DO THIS)

1. **Implement `register_tile_definition()`**:
   ```cpp
   // Store definition in map
   tile_definitions_[def.id] = def;
   ```

2. **Implement `load_tile_definitions()`**:
   ```cpp
   // Parse JSON file containing tile definitions
   // For each definition in JSON:
   //   - Create TileDefinition struct
   //   - Call register_tile_definition()
   ```

3. **Create example JSON**: `data/tiles/basic_tiles.json`
   ```json
   {
     "tiles": [
       {
         "id": 1,
         "name": "stone_floor",
         "category": "Floor",
         "mesh_path": "meshes/floor_stone.obj",
         "has_collision": true,
         "is_walkable": true
       },
       {
         "id": 2,
         "name": "stone_wall",
         "category": "Wall",
         "mesh_path": "meshes/wall_stone.obj",
         "has_collision": true,
         "blocks_sight": true
       }
     ]
   }
   ```

### Phase 3: Chunk Loading System (YOU DO THIS)

1. **Implement `load_chunk()`**:
   - Create new Chunk at given coordinate
   - Set state to Loading
   - Initialize all tiles to Air (definition_id = 0)
   - Either generate procedurally OR load from disk
   - For each non-air tile: call `spawn_tile_entity()`
   - Set state to Loaded
   - Add to `loaded_chunks_` map

2. **Implement `unload_chunk()`**:
   - Set state to Unloading
   - For each tile: call `despawn_tile_entity()`
   - Optionally save chunk to disk
   - Remove from `loaded_chunks_` map

3. **Implement `update_chunk_loading()`** (called from update()):
   - Get player chunk coordinate
   - Calculate chunks within render distance
   - For each nearby chunk:
     - If not loaded: call `load_chunk()`
   - For each loaded chunk:
     - If beyond unload distance: call `unload_chunk()`
   - Enforce max_loaded_chunks limit (LRU eviction)

### Phase 4: Entity Spawning (YOU DO THIS)

1. **Implement `spawn_tile_entity()`**:
   ```cpp
   // Get tile definition
   auto* def = get_tile_definition(tile.definition_id);
   if (!def) return;

   // Create ECS entity
   auto entity = entity_manager_.create_entity();
   tile.spawned_entity = entity;

   // Add Transform component
   math::Vec3 world_pos = tile_to_world_pos(tile.position);
   entity_manager_.add_component<Transform>(entity, {
       .position = world_pos + def->mesh_offset,
       .rotation = rotation_from_tile(tile.rotation),
       .scale = def->mesh_scale
   });

   // Add MeshRenderer component
   entity_manager_.add_component<MeshRenderer>(entity, {
       .mesh_path = def->mesh_path,
       .material_id = def->pbr_material_id
   });

   // Add Collider component if needed
   if (def->has_collision) {
       entity_manager_.add_component<Collider>(entity, {
           .shape = parse_collision_shape(def->collision_shape),
           .extents = def->collision_extents
       });
   }

   tile.is_loaded = true;
   ```

2. **Implement `despawn_tile_entity()`**:
   ```cpp
   if (tile.is_loaded && tile.spawned_entity != INVALID_ENTITY) {
       entity_manager_.destroy_entity(tile.spawned_entity);
       tile.spawned_entity = INVALID_ENTITY;
       tile.is_loaded = false;
   }
   ```

### Phase 5: World Manipulation (YOU DO THIS)

1. **Implement `set_tile()`**:
   - Get or create chunk for this tile
   - Calculate local tile coordinates within chunk
   - Get TileInstance at that position
   - If tile already exists: despawn old entity
   - Set new definition_id and rotation
   - Spawn new entity

2. **Implement `remove_tile()`**:
   - Same as set_tile() but set definition_id to 0 (Air)

3. **Implement `get_tile()`**:
   - Find chunk containing this tile
   - Return const TileInstance* (or nullptr if not loaded)

### Phase 6: Testing Setup (YOU DO THIS)

1. **Create test world in main.cpp**:
   ```cpp
   TilemapWorldSystem tilemap(entity_manager);
   tilemap.initialize({});
   tilemap.load_tile_definitions("data/tiles/basic_tiles.json");

   // Create a 10x10 room
   for (int x = 0; x < 10; x++) {
       for (int y = 0; y < 10; y++) {
           // Floor
           tilemap.set_tile({x, y, 0}, 1); // stone_floor

           // Walls on edges
           if (x == 0 || x == 9 || y == 0 || y == 9) {
               tilemap.set_tile({x, y, 1}, 2); // stone_wall
           }
       }
   }

   // Set player position to center of room
   tilemap.set_player_position({5.0f, 5.0f, 1.5f});
   ```

2. **In render loop**:
   ```cpp
   tilemap.update(delta_time);
   // ECS systems will render the spawned entities automatically
   ```

## Integration with Deferred Renderer

Once you have tiles spawning entities with MeshRenderer components:

1. **MeshRenderer component** should reference a mesh asset and PBR material
2. **Rendering system** iterates all entities with Transform + MeshRenderer
3. **Geometry pass**: Render each mesh with its transform
4. **Lighting pass**: Apply PBR lighting to the G-Buffer

## Optimization Tips

1. **Mesh Instancing**: Group tiles with same mesh_path and draw with instancing
2. **Frustum Culling**: Only render chunks visible to camera
3. **Occlusion Culling**: Don't render tiles behind walls
4. **Level of Detail**: Use simpler meshes for distant chunks

## Next Steps After Basic Implementation

1. Multi-tile object support (doors, large furniture)
2. Tile rotation system (4 orientations)
3. Procedural generation (noise-based terrain)
4. Save/load system (serialize chunks to disk)
5. Dynamic tile updates (destroying walls, opening doors)

## Questions to Consider

1. **Mesh Assets**: Where will you store .obj files? Asset system integration?
2. **Collision System**: Does physics system already have box/sphere colliders?
3. **Entity Components**: What components does your ECS already have?
4. **Camera System**: How will you attach camera to player entity?

I'm ready to help with any specific implementation questions as you build this!