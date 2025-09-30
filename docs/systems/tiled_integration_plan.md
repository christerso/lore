# Tiled Editor Integration Plan

## Overview

Use Tiled Map Editor to design 2D layouts that spawn 3D worlds. Tiles have custom properties that define height, meshes, and behaviors.

## Tiled Setup

### Tileset Creation

1. **Create tileset**: `data/tilesets/lore_world.tsx`
2. **Add tiles** with top-down sprites (for visual editing)
3. **Set custom properties per tile**:
   - `mesh_path` (string): "meshes/wall_stone.obj"
   - `height` (int): Number of vertical tiles (1-8)
   - `collision_type` (string): "box", "none", "mesh"
   - `material_id` (int): Index into DeferredRenderer materials
   - `walkable` (bool): Can entities walk on this?
   - `blocks_sight` (bool): Blocks line of sight?
   - `rotation` (int): 0-3 (for rotatable tiles)

### Map Layout

**Layers** (bottom to top):
1. **floor** - Ground tiles, height=0
2. **walls_1** - 1-tile high walls
3. **walls_2** - 2-tile high walls
4. **walls_3** - 3-tile high walls
5. **walls_4** - 4+ tile high walls
6. **furniture** - Objects on floor
7. **objects** - Special object layer (doors, chests, interactive)

**Why Multiple Wall Layers?**
- Visual clarity in editor (see height at a glance)
- Easy to edit specific height ranges
- Layer visibility toggles help focus

### Example Tiled Workflow

```
1. Open Tiled, create new map (50x50)
2. Import tileset: lore_world.tsx
3. Layer "floor": Paint stone floor tiles (tile ID 1)
4. Layer "walls_1": Draw room perimeter with 1-high walls
5. Layer "walls_4": Add tall walls at room corners
6. Object Layer: Add spawn point, door triggers
7. Export: File → Export As → JSON (.tmj format)
```

## TMX/JSON Format

Tiled exports to JSON (.tmj) with this structure:

```json
{
  "width": 50,
  "height": 50,
  "tilewidth": 32,
  "tileheight": 32,
  "layers": [
    {
      "name": "floor",
      "type": "tilelayer",
      "width": 50,
      "height": 50,
      "data": [1, 1, 1, 2, 2, 3, ...],
      "visible": true
    },
    {
      "name": "walls_4",
      "type": "tilelayer",
      "data": [0, 0, 5, 5, 0, ...],
      "properties": [
        {"name": "z_offset", "value": 0}
      ]
    },
    {
      "name": "objects",
      "type": "objectgroup",
      "objects": [
        {
          "id": 1,
          "name": "spawn_player",
          "type": "spawn_point",
          "x": 400,
          "y": 400,
          "properties": [
            {"name": "entity_type", "value": "player"}
          ]
        }
      ]
    }
  ],
  "tilesets": [
    {
      "firstgid": 1,
      "source": "lore_world.tsx"
    }
  ]
}
```

## Importer Implementation

### Data Structures

```cpp
namespace lore::world {

// Parsed Tiled tile properties
struct TiledTileProperties {
    std::string mesh_path;
    int32_t height = 1;
    std::string collision_type = "box";
    uint32_t material_id = 0;
    bool walkable = true;
    bool blocks_sight = false;
};

// Parsed Tiled layer
struct TiledLayer {
    std::string name;
    int32_t width;
    int32_t height;
    std::vector<uint32_t> data;  // Tile IDs (0 = empty)
    int32_t z_offset = 0;        // Vertical offset for this layer
};

// Parsed Tiled object (spawn points, triggers)
struct TiledObject {
    std::string name;
    std::string type;
    float x, y;
    std::map<std::string, std::string> properties;
};

// Complete Tiled map
struct TiledMap {
    int32_t width;
    int32_t height;
    std::vector<TiledLayer> layers;
    std::vector<TiledObject> objects;
    std::map<uint32_t, TiledTileProperties> tile_properties;  // GID → properties
};

} // namespace lore::world
```

### Importer Class

```cpp
namespace lore::world {

class TiledImporter {
public:
    // Parse TMX/JSON file from Tiled
    static TiledMap load_tiled_map(const std::string& json_path);

    // Convert Tiled map to TilemapWorldSystem
    static void import_to_world(
        TilemapWorldSystem& world,
        const TiledMap& tiled_map,
        const math::Vec3& world_origin = math::Vec3(0, 0, 0)
    );

private:
    // Parse JSON helpers
    static TiledLayer parse_layer(const nlohmann::json& layer_json);
    static TiledTileProperties parse_tile_properties(const nlohmann::json& tile_json);
    static TiledObject parse_object(const nlohmann::json& obj_json);

    // Coordinate conversion
    static TileCoord tiled_to_tile_coord(int tiled_x, int tiled_y, int z_offset);
};

} // namespace lore::world
```

### Import Algorithm

```cpp
void TiledImporter::import_to_world(
    TilemapWorldSystem& world,
    const TiledMap& tiled_map,
    const math::Vec3& world_origin)
{
    // For each layer (floor, walls_1, walls_2, etc.)
    for (const auto& layer : tiled_map.layers) {
        int32_t z_offset = layer.z_offset;

        // For each tile in layer
        for (int y = 0; y < layer.height; y++) {
            for (int x = 0; x < layer.width; x++) {
                uint32_t tile_gid = layer.data[x + y * layer.width];

                if (tile_gid == 0) continue;  // Empty tile

                // Get tile properties
                auto props_it = tiled_map.tile_properties.find(tile_gid);
                if (props_it == tiled_map.tile_properties.end()) {
                    continue;  // No properties defined
                }

                const TiledTileProperties& props = props_it->second;

                // Spawn vertical stack of tiles based on height
                for (int32_t z = 0; z < props.height; z++) {
                    TileCoord coord{x, y, z_offset + z};

                    // Create TileDefinition from properties
                    TileDefinition def;
                    def.mesh_path = props.mesh_path;
                    def.category = (z == 0) ? TileCategory::Floor : TileCategory::Wall;
                    def.has_collision = (props.collision_type != "none");
                    def.is_walkable = (z == 0) ? props.walkable : false;
                    def.blocks_sight = props.blocks_sight;
                    def.pbr_material_id = props.material_id;

                    // Place tile in world
                    uint32_t def_id = world.register_or_get_definition(def);
                    world.set_tile(coord, def_id);
                }
            }
        }
    }

    // Process object layer (spawn points, triggers, etc.)
    for (const auto& obj : tiled_map.objects) {
        if (obj.type == "spawn_point") {
            // Convert pixel coordinates to tile coordinates
            TileCoord spawn_coord{
                static_cast<int32_t>(obj.x / 32),
                static_cast<int32_t>(obj.y / 32),
                0
            };

            // Spawn entity at this location
            // (Implementation depends on entity type)
        }
    }
}
```

## Usage Example

```cpp
// In main.cpp or world initialization:

#include <lore/world/tilemap_world.hpp>
#include <lore/world/tiled_importer.hpp>

// Initialize tilemap system
TilemapWorldSystem tilemap(entity_manager);
tilemap.initialize({});

// Load and import Tiled map
TiledMap map = TiledImporter::load_tiled_map("data/maps/test_room.tmj");
TiledImporter::import_to_world(tilemap, map);

// Set player position from map's spawn point
tilemap.set_player_position(map_spawn_position);

// Update loop
while (running) {
    tilemap.update(delta_time);
    // Chunks load/unload automatically
    // Entities spawn/despawn based on player position
}
```

## Tileset Template

### Basic Tile IDs

| ID | Name | Height | Mesh | Purpose |
|----|------|--------|------|---------|
| 0  | empty | 0 | none | Air/empty space |
| 1  | stone_floor | 1 | floor_stone.obj | Floor |
| 2  | wood_floor | 1 | floor_wood.obj | Floor |
| 3  | stone_wall_1 | 1 | wall_stone.obj | 1-tile wall |
| 4  | stone_wall_4 | 4 | wall_stone_tall.obj | 4-tile wall |
| 5  | door_wood | 2 | door_wood.obj | Door (2 high) |
| 6  | table | 1 | furniture_table.obj | Furniture |
| 7  | chest | 1 | furniture_chest.obj | Interactive |

### Custom Properties Example

For tile ID 4 (stone_wall_4):
```json
{
  "id": 4,
  "type": "stone_wall_tall",
  "properties": [
    {"name": "mesh_path", "type": "string", "value": "meshes/wall_stone_tall.obj"},
    {"name": "height", "type": "int", "value": 4},
    {"name": "collision_type", "type": "string", "value": "box"},
    {"name": "material_id", "type": "int", "value": 1},
    {"name": "walkable", "type": "bool", "value": false},
    {"name": "blocks_sight", "type": "bool", "value": true}
  ]
}
```

## Advantages of This Approach

1. **Visual Editing**: See your level layout in 2D top-down view
2. **Flexible Heights**: Each tile defines its own height via properties
3. **Layer Organization**: Separate layers for different wall heights
4. **Unlimited Tiles**: Use as many tile types as needed
5. **Object Support**: Place spawn points, triggers, lights visually
6. **Industry Standard**: Tiled is used in thousands of games
7. **No Custom Editor Needed**: Leverage existing, mature tool

## MCP Server Integration

**CRITICAL WORKFLOW**: The MCP server enables Claude Code to directly operate Tiled Editor, providing AI-assisted level design.

### MCP Server Capabilities

The Tiled MCP server allows the AI to:
1. **Create and configure tilesets** with proper 1x1 meter tile dimensions
2. **Design rooms/areas** based on real-world measurements provided by the user
3. **Place tiles** according to architectural specifications and standards
4. **Set custom properties** for each tile (height, mesh paths, collision, materials)
5. **Export maps** to JSON format for engine import
6. **Validate designs** against engine requirements (mesh paths exist, properties are valid)

### Tile Scale Standard

**All tiles are 1x1 meter** in world space:
- Tiled editor tile size: 32x32 pixels (visual representation only)
- Engine imports: 1 tile = 1.0 meter in X/Y dimensions
- Height property: Specifies Z-axis height in meters (1-8 typical range)
- Real-world reference: Standard door = 2 meters high (height=2)

### AI-Assisted Workflow

When the user requests a level design:

```
User: "Create a small laboratory room, 10x8 meters, with 3-meter tall walls,
       two doors (standard 2m high), and equipment benches along the walls"

AI (via MCP):
1. Creates new Tiled map (10x8 tiles @ 1m each)
2. Sets up layers: floor, walls_3, furniture, objects
3. Paints floor tiles (stone lab floor)
4. Places 3-meter wall tiles around perimeter
5. Adds door tiles (2m high) at specified positions
6. Places equipment bench tiles along walls
7. Adds spawn point object for player
8. Exports to data/maps/laboratory_01.tmj
9. Reports: "Laboratory created: 10x8m, 3m walls, 2 doors, 4 equipment benches"
```

### MCP Server Requirements

**Tiled MCP Server** must support:
- **map.create(width, height, tile_size)** - Create new map
- **tileset.load(path)** or **tileset.create(name)** - Manage tilesets
- **tile.set_property(tile_id, property_name, value)** - Configure tile properties
- **layer.create(name, type)** - Add layers (tilelayer, objectgroup)
- **layer.paint(layer_name, x, y, tile_id)** - Place tiles
- **object.add(layer_name, type, x, y, properties)** - Add spawn points, triggers
- **map.export(path)** - Export to JSON format
- **map.validate()** - Check for engine compatibility issues

**Blender MCP Server** must support:
- **scene.create(name)** - Create new Blender scene
- **import.tiled_map(tmj_path)** - Import Tiled map as 3D geometry
- **mesh.create_from_tile(tile_def)** - Generate mesh for tile definition
- **material.create_pbr(name, textures)** - Set up PBR materials
- **object.place(mesh, position, rotation, scale)** - Place objects in scene
- **camera.setup(position, target)** - Configure viewport camera
- **lighting.setup(type, intensity, color)** - Add lights to scene
- **render.preview()** - Generate preview render
- **export.scene(path, format)** - Export to FBX/glTF for engine

### Complete Workflow: Tiled → Blender → Engine

```
1. User: "Create a 10x10 meter laboratory with 3m walls"

2. AI (Tiled MCP):
   - Creates 10x10 tile map
   - Places floor and wall tiles
   - Exports to data/maps/lab.tmj

3. AI (Blender MCP):
   - Imports lab.tmj
   - Generates 3D meshes for each tile
   - Applies PBR materials (concrete floors, metal walls)
   - Sets up lighting (fluorescent ceiling lights)
   - Positions camera for preview
   - Renders preview image

4. User: "Looks good but add more equipment"

5. AI (Tiled MCP):
   - Reopens lab.tmj
   - Adds equipment tiles (benches, computers, shelves)
   - Re-exports

6. AI (Blender MCP):
   - Reimports updated lab.tmj
   - Updates scene with new equipment
   - Renders updated preview

7. When satisfied, engine imports lab.tmj directly
```

### Lighting Consistency: Blender ↔ Engine

**CRITICAL**: Blender previews must match game lighting as closely as possible.

#### PBR Material Workflow

Both Blender and Engine use **Physically Based Rendering**:
- **Albedo** (Base Color)
- **Metallic** (0.0 = dielectric, 1.0 = metal)
- **Roughness** (0.0 = mirror, 1.0 = diffuse)
- **Normal Map** (tangent space)
- **Ambient Occlusion** (cavity shadows)
- **Emissive** (self-illumination)

**Material Setup**:
1. Blender: Use Principled BSDF shader (PBR standard)
2. Engine: DeferredRenderer uses identical Cook-Torrance BRDF
3. Same texture maps in both systems
4. Same metallic/roughness values

#### Light Types & Parameters

| Light Type | Blender Setup | Engine Equivalent | Key Parameters |
|------------|---------------|-------------------|----------------|
| **Directional** | Sun lamp | Directional light | Direction, intensity, color |
| **Point** | Point lamp | Point light | Position, radius, intensity, color |
| **Spot** | Spot lamp | Spot light | Position, direction, angle, intensity, color |
| **Area** | Area lamp | Multiple point lights | Approximated with point light grid |

**Light Matching Strategy**:
1. Export light positions/types from Blender scene
2. Store in Tiled map object layer as "light" objects
3. Import lights into engine DeferredRenderer
4. Use identical intensity/color values
5. Calibrate: 1 Blender unit = 1 game unit of brightness

#### Environment Lighting

**Ambient Light**:
- Blender: World shader background color
- Engine: Ambient light term in lighting shader
- Match ambient intensity and color temperature

**Image-Based Lighting (Future)**:
- Blender: HDR environment map
- Engine: Pre-filtered cubemap for reflections
- Share same HDR environment between systems

#### Tone Mapping & Exposure

**Blender Settings**:
```python
# Blender scene setup for engine-matching
bpy.context.scene.view_settings.view_transform = 'Standard'
bpy.context.scene.view_settings.exposure = 0.0  # Match engine exposure
bpy.context.scene.world.light_settings.use_ambient_occlusion = True
bpy.context.scene.world.light_settings.ao_factor = 0.5  # Match engine AO
```

**Engine Settings**:
- Use same exposure value as Blender (default: 0.0)
- Apply tone mapping in post-process (future: Reinhard or ACES)
- Match gamma correction (2.2 standard)

#### Validation Workflow

When Blender MCP imports a map:
1. Parse light objects from Tiled map
2. Create matching Blender lamps
3. Set identical intensity/color values
4. Render Blender preview
5. User compares with engine screenshot
6. Adjust lighting in Tiled/Blender
7. Re-export and test in engine

**Lighting Object Format in Tiled**:
```json
{
  "type": "light",
  "name": "ceiling_light_01",
  "x": 500,
  "y": 400,
  "properties": [
    {"name": "light_type", "value": "point"},
    {"name": "intensity", "value": "100.0"},
    {"name": "color_r", "value": "1.0"},
    {"name": "color_g", "value": "0.95"},
    {"name": "color_b", "value": "0.9"},
    {"name": "radius", "value": "10.0"}
  ]
}
```

## Implementation Steps (Your TODO)

1. **Install Tiled**: Download from mapeditor.org
2. **Create basic tileset**: 5-10 tiles with properties
3. **Design test room**: 10x10 room in Tiled
4. **Export to JSON**: File → Export As → JSON
5. **Implement TiledImporter::load_tiled_map()**: Parse JSON
6. **Implement TiledImporter::import_to_world()**: Convert to tiles
7. **Test**: Load map, spawn entities, verify in 3D

## Questions?

- Should walls auto-connect (corner pieces, T-junctions)?
- Do you want tile variants (random stone floor textures)?
- Should we support animated tiles (torches, water)?
- Multi-tile objects (2x2 table) - spawn from single tile or multiple?