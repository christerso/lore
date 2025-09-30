# Task Executor 3: First Test Area Creation

## Assignment
Create a complete, playable test area using Tiled Editor that demonstrates all integrated systems working together.

## Objective
Design a test scene (laboratory room) with walls, floor, objects, lights, and spawn points. Export as Tiled JSON and create example code that loads the scene with full rendering integration.

## Task Breakdown

### Subtask 3.1: Create Tiled Tileset Definition
**File**: `data/tilesets/lore_test_tileset.json` (NEW)

**Requirements**:
- Define 15-20 basic tiles with proper custom properties
- Include: floors, walls (1-4 height), doors, furniture, equipment
- Set 32×32 pixel tile size (visual only, 1 tile = 1 meter in engine)
- Use simple colored squares as visual placeholders (or reference texture files)

**Tileset Structure**:
```json
{
  "name": "lore_test_tileset",
  "tilewidth": 32,
  "tileheight": 32,
  "tilecount": 20,
  "columns": 5,
  "image": "test_tiles.png",
  "imagewidth": 160,
  "imageheight": 128,
  "margin": 0,
  "spacing": 0,
  "tiles": [
    {
      "id": 0,
      "type": "empty",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": ""},
        {"name": "height", "type": "int", "value": 0}
      ]
    },
    {
      "id": 1,
      "type": "stone_floor",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": "meshes/floor_stone.obj"},
        {"name": "height", "type": "int", "value": 1},
        {"name": "collision_type", "type": "string", "value": "none"},
        {"name": "material_id", "type": "int", "value": 0},
        {"name": "walkable", "type": "bool", "value": true},
        {"name": "blocks_sight", "type": "bool", "value": false}
      ]
    },
    {
      "id": 2,
      "type": "metal_wall_3m",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": "meshes/wall_metal.obj"},
        {"name": "height", "type": "int", "value": 3},
        {"name": "collision_type", "type": "string", "value": "box"},
        {"name": "material_id", "type": "int", "value": 1},
        {"name": "walkable", "type": "bool", "value": false},
        {"name": "blocks_sight", "type": "bool", "value": true}
      ]
    },
    {
      "id": 3,
      "type": "door_standard",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": "meshes/door_metal.obj"},
        {"name": "height", "type": "int", "value": 2},
        {"name": "collision_type", "type": "string", "value": "none"},
        {"name": "material_id", "type": "int", "value": 2},
        {"name": "walkable", "type": "bool", "value": true},
        {"name": "blocks_sight", "type": "bool", "value": false}
      ]
    },
    {
      "id": 4,
      "type": "equipment_bench",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": "meshes/furniture_bench.obj"},
        {"name": "height", "type": "int", "value": 1},
        {"name": "collision_type", "type": "string", "value": "box"},
        {"name": "material_id", "type": "int", "value": 3},
        {"name": "walkable", "type": "bool", "value": false},
        {"name": "blocks_sight", "type": "bool", "value": false}
      ]
    },
    {
      "id": 5,
      "type": "computer_console",
      "properties": [
        {"name": "mesh_path", "type": "string", "value": "meshes/furniture_computer.obj"},
        {"name": "height", "type": "int", "value": 1},
        {"name": "collision_type", "type": "string", "value": "box"},
        {"name": "material_id", "type": "int", "value": 4},
        {"name": "walkable", "type": "bool", "value": false},
        {"name": "blocks_sight", "type": "bool", "value": false}
      ]
    }
  ]
}
```

**Note**: Create simple test_tiles.png (160×128 pixels) with colored squares representing each tile type.

### Subtask 3.2: Design Laboratory Test Area in Tiled JSON
**File**: `data/maps/test_laboratory.tmj` (NEW)

**Room Specifications**:
- Dimensions: 12×10 meters (12×10 tiles)
- Wall height: 3 meters
- Two doorways (2m high) on opposite walls
- Equipment benches along two walls
- Computer consoles in corners
- Central open area for movement
- Ceiling lights (4 point lights)

**Map Structure**:
```json
{
  "compressionlevel": -1,
  "height": 10,
  "infinite": false,
  "layers": [
    {
      "name": "floor",
      "type": "tilelayer",
      "width": 12,
      "height": 10,
      "data": [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
               1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
               ...],
      "opacity": 1,
      "visible": true,
      "x": 0,
      "y": 0
    },
    {
      "name": "walls_3m",
      "type": "tilelayer",
      "width": 12,
      "height": 10,
      "data": [2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
               2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2],
      "opacity": 1,
      "visible": true,
      "x": 0,
      "y": 0
    },
    {
      "name": "furniture",
      "type": "tilelayer",
      "width": 12,
      "height": 10,
      "data": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
               0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0,
               0, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 0,
               ...],
      "opacity": 1,
      "visible": true,
      "x": 0,
      "y": 0
    },
    {
      "name": "objects",
      "type": "objectgroup",
      "objects": [
        {
          "id": 1,
          "name": "player_spawn",
          "type": "spawn_point",
          "x": 192,
          "y": 160,
          "width": 0,
          "height": 0,
          "visible": true,
          "properties": [
            {"name": "entity_type", "type": "string", "value": "player"},
            {"name": "facing_angle", "type": "float", "value": 90.0}
          ]
        },
        {
          "id": 2,
          "name": "ceiling_light_nw",
          "type": "light",
          "x": 96,
          "y": 96,
          "width": 0,
          "height": 0,
          "visible": true,
          "properties": [
            {"name": "light_type", "type": "string", "value": "point"},
            {"name": "intensity", "type": "float", "value": 150.0},
            {"name": "color_r", "type": "float", "value": 1.0},
            {"name": "color_g", "type": "float", "value": 0.95},
            {"name": "color_b", "type": "float", "value": 0.9},
            {"name": "range", "type": "float", "value": 8.0}
          ]
        },
        {
          "id": 3,
          "name": "ceiling_light_ne",
          "type": "light",
          "x": 288,
          "y": 96,
          "properties": [
            {"name": "light_type", "type": "string", "value": "point"},
            {"name": "intensity", "type": "float", "value": 150.0},
            {"name": "color_r", "type": "float", "value": 1.0},
            {"name": "color_g", "type": "float", "value": 0.95},
            {"name": "color_b", "type": "float", "value": 0.9},
            {"name": "range", "type": "float", "value": 8.0}
          ]
        },
        {
          "id": 4,
          "name": "ceiling_light_sw",
          "type": "light",
          "x": 96,
          "y": 224,
          "properties": [
            {"name": "light_type", "type": "string", "value": "point"},
            {"name": "intensity", "type": "float", "value": 150.0},
            {"name": "color_r", "type": "float", "value": 1.0},
            {"name": "color_g", "type": "float", "value": 0.95},
            {"name": "color_b", "type": "float", "value": 0.9},
            {"name": "range", "type": "float", "value": 8.0}
          ]
        },
        {
          "id": 5,
          "name": "ceiling_light_se",
          "type": "light",
          "x": 288,
          "y": 224,
          "properties": [
            {"name": "light_type", "type": "string", "value": "point"},
            {"name": "intensity", "type": "float", "value": 150.0},
            {"name": "color_r", "type": "float", "value": 1.0},
            {"name": "color_g", "type": "float", "value": 0.95},
            {"name": "color_b", "type": "float", "value": 0.9},
            {"name": "range", "type": "float", "value": 8.0}
          ]
        }
      ],
      "opacity": 1,
      "visible": true,
      "x": 0,
      "y": 0
    }
  ],
  "nextlayerid": 5,
  "nextobjectid": 6,
  "orientation": "orthogonal",
  "renderorder": "right-down",
  "tiledversion": "1.10.0",
  "tilewidth": 32,
  "tileheight": 32,
  "tilesets": [
    {
      "firstgid": 1,
      "source": "../tilesets/lore_test_tileset.json"
    }
  ],
  "type": "map",
  "version": "1.10",
  "width": 12
}
```

### Subtask 3.3: Create Simple Tile Texture Atlas
**File**: `data/tilesets/test_tiles.png` (NEW)

**Requirements**:
- 160×128 pixel PNG image
- 5 columns × 4 rows of 32×32 tiles
- Simple solid colors for each tile type:
  - Tile 0: Black (empty)
  - Tile 1: Gray (stone floor)
  - Tile 2: Dark blue (metal wall)
  - Tile 3: Green (door)
  - Tile 4: Brown (equipment bench)
  - Tile 5: Light blue (computer console)
  - Tiles 6-19: Various colors for future use

**Note**: This is just for Tiled editor visualization. Engine uses 3D meshes.

### Subtask 3.4: Create Test Area Loading Example
**File**: `examples/load_test_area.cpp` (NEW)

**Requirements**:
- Complete, runnable example showing full integration
- Load Tiled map using SceneLoader
- Initialize all rendering systems (deferred, shadows, atmospheric, post-process)
- Set up camera at player spawn point
- Render loop with all systems active
- Show performance metrics

**Example Structure**:
```cpp
/**
 * Load Test Area Example
 * ======================
 *
 * Demonstrates loading a complete scene from Tiled map with full rendering integration:
 * - Loads test_laboratory.tmj
 * - Creates tiles, lights, and spawn points
 * - Sets up complete rendering pipeline
 * - Renders with deferred + shadows + atmospheric + post-process
 */

#include <lore/world/scene_loader.hpp>
#include <lore/world/tilemap_world_system.hpp>
#include <lore/graphics/deferred_renderer.hpp>
#include <lore/graphics/post_process_pipeline.hpp>
#include <lore/ecs/world_manager.hpp>
#include <lore/ecs/components/atmospheric_component.hpp>

using namespace lore;

int main() {
    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Initialize Vulkan (standard setup)
    VkDevice device = /* ... */;
    VkPhysicalDevice physical_device = /* ... */;
    VmaAllocator allocator = /* ... */;

    // Create ECS world
    ecs::EntityManager entity_manager;

    // Create tilemap system
    world::TilemapWorldSystem tilemap(entity_manager);
    tilemap.initialize({});

    // Create deferred renderer
    graphics::DeferredRenderer deferred_renderer(device, physical_device, allocator);
    deferred_renderer.initialize({1920, 1080}, VK_FORMAT_B8G8R8A8_UNORM);

    // Create atmospheric component
    auto atmos_entity = entity_manager.create_entity();
    auto atmos = ecs::AtmosphericComponent::create_earth_clear_day();
    entity_manager.add_component(atmos_entity, std::move(atmos));

    // Create post-process pipeline
    auto pp_config = graphics::PostProcessConfig::create_aces_neutral();
    graphics::PostProcessPipeline post_process(device, allocator, pp_config);

    // ========================================================================
    // SCENE LOADING
    // ========================================================================

    world::SceneLoader scene_loader(tilemap, deferred_renderer, entity_manager);

    world::SceneLoadConfig config;
    config.tiled_map_path = "data/maps/test_laboratory.tmj";
    config.world_origin = math::Vec3{0.0f, 0.0f, 0.0f};
    config.spawn_entities = true;
    config.create_lights = true;

    scene_loader.load_scene(config);

    LOG_INFO(App, "Scene loaded: {} lights, {} spawn points",
             scene_loader.get_light_ids().size(),
             scene_loader.get_spawn_points().size());

    // Position camera at player spawn
    auto player_spawn = scene_loader.get_spawn_position("player");
    math::Vec3 camera_pos = player_spawn.value_or(math::Vec3{6.0f, 2.0f, 5.0f});
    camera_pos.y = 2.0f;  // Eye height

    // ========================================================================
    // RENDER LOOP
    // ========================================================================

    while (!should_quit()) {
        // Update time
        float delta_time = /* ... */;

        // Update systems
        tilemap.update(delta_time);

        // Begin frame
        VkCommandBuffer cmd = /* ... */;

        // [1] Shadow pass
        deferred_renderer.begin_shadow_pass(cmd, 0);
        // ... render shadow-casting geometry ...
        deferred_renderer.end_shadow_pass(cmd);

        // [2] Geometry pass (G-Buffer)
        deferred_renderer.begin_geometry_pass(cmd);
        // ... render scene geometry ...
        deferred_renderer.end_geometry_pass(cmd);

        // [3] Lighting pass (PBR + shadows + atmospheric)
        VkImage hdr_image = deferred_renderer.get_hdr_image();
        VkImageView hdr_view = deferred_renderer.get_hdr_image_view();
        deferred_renderer.begin_lighting_pass(cmd, hdr_image, hdr_view);
        deferred_renderer.end_lighting_pass(cmd);

        // [4] Post-processing (tone mapping)
        VkImage ldr_image = swapchain_images[image_index];
        post_process.apply(cmd, hdr_image, ldr_image);

        // Present
        /* ... */;
    }

    return 0;
}
```

### Subtask 3.5: Create Integration Documentation
**File**: `docs/systems/SCENE_LOADING_GUIDE.md` (NEW)

**Requirements**:
- Complete guide to using Tiled + SceneLoader
- Workflow: Design in Tiled → Export → Load in Engine
- Tile property reference
- Light object reference
- Spawn point reference
- Common issues and solutions
- Performance considerations

**Outline**:
```markdown
# Scene Loading Guide

## Overview
How to create game levels using Tiled Editor and load them with full rendering integration.

## Workflow
1. Design level in Tiled Editor
2. Set custom properties on tiles
3. Add light objects
4. Add spawn point objects
5. Export to JSON (.tmj)
6. Load with SceneLoader in engine

## Tile Properties Reference
[Table of all supported properties]

## Object Types
### Lights
[Complete light properties]

### Spawn Points
[Complete spawn point properties]

### Triggers
[Complete trigger properties]

## Example: Creating a Room
[Step-by-step tutorial]

## Integration Code
[Example code snippets]

## Performance Tips
[Optimization suggestions]
```

## Success Criteria
1. ✅ Tileset JSON created with 15+ tiles properly defined
2. ✅ Test laboratory map created with 12×10 room
3. ✅ 4 lights placed correctly in map
4. ✅ Player spawn point defined
5. ✅ Example code compiles and runs
6. ✅ Documentation complete and clear
7. ✅ All assets in proper data/ directory structure

## Dependencies
- SceneLoader must be created (Task 2)
- DeferredRenderer must have HDR support (Task 1)
- Tiled importer must be complete (Task 2)

## Context
- Project root: `G:\repos\lore`
- Branch: `feature/complete-integration`
- This is the final integration test showing everything working together

## Reporting
When complete, report back with:
1. List of all created files and their locations
2. Compilation status of example code
3. Screenshot or description of test area layout
4. Any issues encountered
5. Suggestions for next test scenarios

## Executor Instructions
- Create all files in proper data/ directory structure
- Follow Tiled JSON format precisely
- Make example code production-quality (no shortcuts)
- Add extensive comments in example code
- Create thorough documentation
- Test that all file paths resolve correctly
- Ensure tileset references work properly