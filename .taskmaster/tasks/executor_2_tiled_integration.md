# Task Executor 2: Tiled Integration Completion

## Assignment
Complete the Tiled importer by implementing light spawning, spawn point handling, and scene loader wrapper.

## Objective
Eliminate all TODOs in `tiled_importer.cpp` and create a high-level scene loader that integrates Tiled maps with the rendering system.

## Task Breakdown

### Subtask 2.1: Implement Light Spawning (TODO at line 482)
**File**: `src/world/tiled_importer.cpp` - `import_to_world()` function

**Current TODO**:
```cpp
} else if (obj.type == "light") {
    // TODO: Create light in DeferredRenderer
```

**Requirements**:
- Parse light properties from Tiled object:
  - `light_type`: "directional", "point", or "spot"
  - `intensity`: float (lumens or multiplier)
  - `color_r`, `color_g`, `color_b`: float (0.0-1.0)
  - `range`: float (for point/spot lights, in meters)
  - `inner_angle`, `outer_angle`: float (for spot lights, in degrees)
  - `direction_x`, `direction_y`, `direction_z`: float (for directional/spot)
- Convert Tiled pixel coordinates to world position (meters)
- Create `graphics::Light` struct
- Add light to DeferredRenderer via `deferred_renderer->add_light()`
- Store light ID for potential updates

**Implementation Pattern**:
```cpp
else if (obj.type == "light") {
    // Parse light properties
    std::string light_type = obj.properties["light_type"];
    float intensity = std::stof(obj.properties.at("intensity"));
    math::Vec3 color{
        std::stof(obj.properties.at("color_r")),
        std::stof(obj.properties.at("color_g")),
        std::stof(obj.properties.at("color_b"))
    };

    // Convert position
    math::Vec3 world_pos{
        obj.x * TILE_SIZE_METERS + world_origin.x,
        0.0f,
        obj.y * TILE_SIZE_METERS + world_origin.z
    };

    // Create light based on type
    graphics::Light light{};
    if (light_type == "point") {
        light.type = graphics::LightType::Point;
        light.position = world_pos;
        light.color = color;
        light.intensity = intensity;
        light.range = std::stof(obj.properties.at("range"));
    } else if (light_type == "directional") {
        // ... directional setup
    } else if (light_type == "spot") {
        // ... spot setup
    }

    // Add to renderer (requires passing DeferredRenderer reference)
    if (deferred_renderer_ptr) {
        uint32_t light_id = deferred_renderer_ptr->add_light(light);
        // Store light_id if needed for updates
    }
}
```

**Challenge**: `import_to_world()` needs access to DeferredRenderer. Solutions:
1. **Option A**: Add `DeferredRenderer*` parameter to `import_to_world()`
2. **Option B**: Return list of parsed lights, caller adds them to renderer
3. **Option C**: Store lights in TiledMap struct, process separately

**Recommended**: Option B (return lights in TiledMap struct)

### Subtask 2.2: Implement Spawn Point Handling (TODO at line 475)
**File**: `src/world/tiled_importer.cpp` - `import_to_world()` function

**Current TODO**:
```cpp
if (obj.type == "spawn_point") {
    // ... coordinate conversion ...
    // TODO: Spawn entity based on properties
}
```

**Requirements**:
- Parse spawn point properties:
  - `entity_type`: "player", "enemy", "npc", "item", etc.
  - `variant`: optional specific entity variant
  - `facing_angle`: float (degrees, 0-360)
  - Custom properties per entity type
- Convert Tiled coordinates to world position
- Store spawn information for later entity creation
- Do NOT create entities directly (world may not have entity factory yet)

**Implementation Pattern**:
```cpp
if (obj.type == "spawn_point") {
    // Convert coordinates
    TileCoord spawn_tile{
        static_cast<int32_t>(obj.x / (TILE_SIZE_PIXELS * tile_scale)),
        static_cast<int32_t>(obj.y / (TILE_SIZE_PIXELS * tile_scale)),
        0
    };

    math::Vec3 spawn_world_pos = tilemap.tile_to_world_position(spawn_tile);
    spawn_world_pos.x += world_origin.x;
    spawn_world_pos.y += world_origin.y;
    spawn_world_pos.z += world_origin.z;

    // Create spawn info struct
    SpawnPointInfo spawn_info;
    spawn_info.position = spawn_world_pos;
    spawn_info.entity_type = obj.properties.at("entity_type");
    spawn_info.facing_angle = std::stof(obj.properties["facing_angle"]);
    spawn_info.properties = obj.properties;  // Store all properties

    // Add to output list (requires extending TiledMap struct)
    result.spawn_points.push_back(spawn_info);

    LOG_INFO(World, "Spawn point: {} at ({:.1f}, {:.1f}, {:.1f})",
             spawn_info.entity_type,
             spawn_world_pos.x, spawn_world_pos.y, spawn_world_pos.z);
}
```

**Required Changes**:
- Add `SpawnPointInfo` struct to `tiled_importer.hpp`
- Add `std::vector<SpawnPointInfo> spawn_points` to `TiledMap` struct
- Extend properties map to include custom spawn properties

### Subtask 2.3: Implement Trigger Volume Handling (TODO at line 489)
**File**: `src/world/tiled_importer.cpp`

**Current TODO**:
```cpp
} else if (obj.type == "trigger") {
    // TODO: Create trigger volume
```

**Requirements**:
- Parse trigger properties:
  - `trigger_type`: "door", "pressure_plate", "teleport", "dialogue", etc.
  - `width`, `height`: float (meters)
  - `target_map`: string (for teleport triggers)
  - `target_x`, `target_y`: float (teleport destination)
  - Custom script/action properties
- Store trigger volume data for game logic system
- Do NOT implement trigger logic (belongs in gameplay system)

**Implementation Pattern**:
```cpp
else if (obj.type == "trigger") {
    TriggerVolumeInfo trigger;
    trigger.position = math::Vec3{obj.x * TILE_SIZE_METERS, 0.0f, obj.y * TILE_SIZE_METERS};
    trigger.extents = math::Vec3{
        std::stof(obj.properties.at("width")) * 0.5f,
        1.0f,  // Height
        std::stof(obj.properties.at("height")) * 0.5f
    };
    trigger.trigger_type = obj.properties.at("trigger_type");
    trigger.properties = obj.properties;

    result.triggers.push_back(trigger);
}
```

### Subtask 2.4: Create Scene Loader Wrapper
**Files**:
- `include/lore/world/scene_loader.hpp` (NEW)
- `src/world/scene_loader.cpp` (NEW)

**Purpose**: High-level API for loading complete scenes from Tiled maps with rendering integration.

**API Design**:
```cpp
namespace lore::world {

struct SceneLoadConfig {
    std::string tiled_map_path;                    // Path to .tmj file
    math::Vec3 world_origin{0.0f, 0.0f, 0.0f};    // World offset
    bool spawn_entities = true;                    // Create entities from spawn points
    bool create_lights = true;                     // Add lights to renderer
    bool load_triggers = true;                     // Create trigger volumes
};

class SceneLoader {
public:
    SceneLoader(
        TilemapWorldSystem& tilemap,
        graphics::DeferredRenderer& renderer,
        ecs::EntityManager& entity_manager
    );

    // Load complete scene from Tiled map
    void load_scene(const SceneLoadConfig& config);

    // Get spawn point by entity type
    std::optional<math::Vec3> get_spawn_position(const std::string& entity_type) const;

    // Get all lights created from map
    const std::vector<uint32_t>& get_light_ids() const { return light_ids_; }

    // Get all spawn points
    const std::vector<SpawnPointInfo>& get_spawn_points() const { return spawn_points_; }

private:
    void load_tiles(const TiledMap& map, const math::Vec3& origin);
    void load_lights(const TiledMap& map, const math::Vec3& origin);
    void load_entities(const TiledMap& map, const math::Vec3& origin);
    void load_triggers(const TiledMap& map, const math::Vec3& origin);

    TilemapWorldSystem& tilemap_;
    graphics::DeferredRenderer& renderer_;
    ecs::EntityManager& entity_manager_;

    std::vector<uint32_t> light_ids_;
    std::vector<SpawnPointInfo> spawn_points_;
    std::vector<TriggerVolumeInfo> triggers_;
};

} // namespace lore::world
```

**Implementation Notes**:
- `load_scene()` orchestrates all loading steps
- `load_tiles()` calls `TiledImporter::import_to_world()`
- `load_lights()` processes light objects and adds to DeferredRenderer
- `load_entities()` creates entities from spawn points (if entity factory exists)
- `load_triggers()` stores trigger volumes for game logic system

### Subtask 2.5: Extend TiledMap Structure
**File**: `include/lore/world/tiled_importer.hpp`

**Add New Structs**:
```cpp
struct SpawnPointInfo {
    math::Vec3 position;
    std::string entity_type;
    float facing_angle = 0.0f;
    std::map<std::string, std::string> properties;
};

struct TriggerVolumeInfo {
    math::Vec3 position;
    math::Vec3 extents;
    std::string trigger_type;
    std::map<std::string, std::string> properties;
};

struct LightInfo {
    graphics::LightType type;
    math::Vec3 position;
    math::Vec3 direction;  // For directional/spot
    math::Vec3 color;
    float intensity;
    float range;           // For point/spot
    float inner_angle;     // For spot (degrees)
    float outer_angle;     // For spot (degrees)
};
```

**Update TiledMap**:
```cpp
struct TiledMap {
    // ... existing fields ...

    std::vector<SpawnPointInfo> spawn_points;
    std::vector<TriggerVolumeInfo> triggers;
    std::vector<LightInfo> lights;
};
```

## Success Criteria
1. ✅ All TODOs eliminated from `tiled_importer.cpp`
2. ✅ Light objects parsed and added to DeferredRenderer
3. ✅ Spawn points stored in TiledMap for entity creation
4. ✅ Trigger volumes parsed and stored
5. ✅ SceneLoader created with complete API
6. ✅ Code compiles without errors or warnings
7. ✅ Integration pattern clear and well-documented

## Dependencies
- DeferredRenderer exists and is production-ready (Task 1 may update it)
- TilemapWorldSystem exists and works
- Light struct defined in `lore/graphics/deferred_renderer.hpp`

## Context
- Project root: `G:\repos\lore`
- Branch: `feature/complete-integration`
- Tiled importer is 90% complete, just needs final TODOs resolved
- Scene loader is completely new file

## Reporting
When complete, report back with:
1. List of modified/created files
2. Compilation status
3. API usage example for SceneLoader
4. Any issues or design decisions made

## Executor Instructions
- Read `tiled_importer.cpp` completely first
- Understand existing code patterns before modifying
- Implement TODOs one at a time, test compilation
- Create SceneLoader as separate, clean abstraction
- Add comprehensive comments and documentation
- Follow existing code style (namespace lore::world, etc.)
- DO NOT simplify or stub out functionality