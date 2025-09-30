# Lore World Tilesets

## Tile Scale Standard

**All tiles are 1x1 meter in world space:**
- Tiled editor displays tiles as 32x32 pixels (visual only)
- Engine imports: 1 tile = 1.0 meter in X/Y dimensions
- Height property: Specifies Z-axis height in meters

## Using Existing Blender Assets

You already have tiles, floors, and alcoves in Blender. Here's how to integrate them:

### 1. Export from Blender 4.5

For each tile asset (floor, wall, alcove, etc.):

1. **Ensure proper scale**: 1 Blender unit = 1 meter
2. **Center origin**: Place object origin at bottom-center of tile
3. **Export as OBJ or FBX**:
   - File → Export → Wavefront (.obj) or FBX (.fbx)
   - Location: `G:/repos/lore/data/meshes/[category]/[name].obj`
   - Categories: `floors/`, `walls/`, `furniture/`, `props/`
   - Apply modifiers: Yes
   - Include: Selected Objects Only
   - Forward: -Z Forward
   - Up: Y Up
   - Scale: 1.0

### 2. Update Tileset JSON

Edit `lore_world.json` to reference your exported meshes:

```json
{
  "id": 0,
  "type": "your_floor_name",
  "properties": [
    {"name": "mesh_path", "type": "string", "value": "meshes/floors/your_floor.obj"},
    {"name": "height", "type": "int", "value": 1},
    {"name": "collision_type", "type": "string", "value": "box"},
    {"name": "material_id", "type": "int", "value": 0},
    {"name": "walkable", "type": "bool", "value": true},
    {"name": "blocks_sight", "type": "bool", "value": false}
  ]
}
```

### 3. Create Tileset Image (Optional)

For visual editing in Tiled, create a sprite sheet:

1. In Blender, render top-down orthographic view of each tile
2. Arrange renders in 8-column grid (32x32 pixels per tile)
3. Save as `lore_world.png`
4. Place in `data/tilesets/` directory

### 4. Import into Tiled 1.11.2

1. Open Tiled
2. New Tileset:
   - Type: Based on Tileset Image
   - Source: `lore_world.png`
   - Tile width: 32 pixels
   - Tile height: 32 pixels
3. Import Properties:
   - File → Import → Custom Properties
   - Select: `lore_world.json`
   - This loads all tile properties defined above

### 5. Example: Alcove Tile

If you have an alcove in Blender:

1. Export as `meshes/walls/alcove_2m.obj`
2. Add to tileset JSON:

```json
{
  "id": 16,
  "type": "alcove_2m",
  "properties": [
    {"name": "mesh_path", "type": "string", "value": "meshes/walls/alcove_2m.obj"},
    {"name": "height", "type": "int", "value": 2},
    {"name": "collision_type", "type": "string", "value": "box"},
    {"name": "material_id", "type": "int", "value": 2},
    {"name": "walkable", "type": "bool", "value": false},
    {"name": "blocks_sight", "type": "bool", "value": false}
  ]
}
```

3. In Tiled, assign this tile to alcove locations

## Tile Properties Reference

| Property | Type | Description | Example Values |
|----------|------|-------------|----------------|
| `mesh_path` | string | Path to 3D mesh file | `meshes/floors/concrete.obj` |
| `height` | int | Vertical height in meters | 1, 2, 3, 4 |
| `collision_type` | string | Physics collision shape | `box`, `sphere`, `mesh`, `none` |
| `material_id` | int | PBR material index | 0-15 |
| `walkable` | bool | Can entities walk on this? | `true`, `false` |
| `blocks_sight` | bool | Blocks line of sight? | `true`, `false` |

## Material IDs

Material IDs reference PBR materials in the DeferredRenderer. **Textures are optional** - the engine works seamlessly with both textured and untextured meshes.

| ID | Material | PBR Properties (Albedo RGB, Roughness, Metallic) |
|----|----------|---------------------------------------------------|
| 0  | Default/Concrete | (0.5, 0.5, 0.5), 0.8, 0.0 |
| 1  | Metal | (0.7, 0.7, 0.7), 0.3, 1.0 |
| 2  | Painted Metal | (0.6, 0.6, 0.6), 0.5, 0.0 |
| 3  | Wood | (0.4, 0.3, 0.2), 0.7, 0.0 |
| 4  | Plastic | (0.8, 0.8, 0.8), 0.4, 0.0 |
| 5  | Lab Equipment | (0.7, 0.7, 0.8), 0.4, 0.2 |
| 6  | Crate Wood | (0.5, 0.4, 0.3), 0.8, 0.0 |
| 7  | Electronics | (0.2, 0.2, 0.3), 0.3, 0.5 |
| 8  | Glass | (0.9, 0.9, 0.9), 0.0, 0.0 |

**Texture-Agnostic Design**:
- **With textures**: Engine uses albedo/normal/roughness/metallic texture maps
- **Without textures**: Engine uses solid colors with PBR properties (shown above)
- Loading and rendering work identically in both cases
- Material system automatically falls back to defaults if textures are missing
- This allows rapid prototyping with untextured geometry from Blender

## Lighting in Tiled

Add light objects to your Tiled maps:

1. Create Object Layer: "lights"
2. Add Point object
3. Set properties:
   - `light_type`: "point", "directional", or "spot"
   - `intensity`: 100.0 (adjust as needed)
   - `color_r`: 1.0
   - `color_g`: 0.95
   - `color_b`: 0.9
   - `radius`: 10.0 (for point/spot lights)

These will be imported as lights in the DeferredRenderer.

## Workflow Summary

1. Model tiles in Blender 4.5 (1m = 1 unit)
2. Export to `data/meshes/[category]/`
3. Update `lore_world.json` with mesh paths
4. Create tileset in Tiled 1.11.2
5. Design maps in Tiled
6. Export as JSON (.tmj)
7. Engine imports via TiledImporter
8. Preview in Blender using Tiled MCP server (future)