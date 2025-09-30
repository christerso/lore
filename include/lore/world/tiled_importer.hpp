#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace lore::world {

// Forward declarations
class TilemapWorldSystem;
struct TileCoord;

// Parsed Tiled tile properties (custom properties set in Tiled editor)
struct TiledTileProperties {
    std::string mesh_path;           // Path to 3D mesh (e.g., "meshes/wall_stone.obj")
    float height = 1.0f;             // Vertical height in meters (supports fractional values like 0.1)
    std::string collision_type = "box"; // "box", "sphere", "mesh", "none"
    uint32_t material_id = 0;        // PBR material index
    bool walkable = true;            // Can entities walk on this?
    bool blocks_sight = false;       // Blocks line of sight?
};

// Parsed Tiled layer (tilelayer or objectgroup)
struct TiledLayer {
    std::string name;                // Layer name (e.g., "floor", "walls_3")
    std::string type;                // "tilelayer" or "objectgroup"
    int32_t width = 0;               // Width in tiles
    int32_t height = 0;              // Height in tiles
    std::vector<uint32_t> data;      // Tile GIDs (0 = empty, only for tilelayers)
    int32_t z_offset = 0;            // Vertical offset for this layer
    bool visible = true;             // Is layer visible?

    // Custom layer properties (key-value pairs)
    std::map<std::string, std::string> properties;
};

// Parsed Tiled object (spawn points, triggers, etc.)
struct TiledObject {
    uint32_t id;                     // Unique object ID
    std::string name;                // Object name (e.g., "spawn_player")
    std::string type;                // Object type (e.g., "spawn_point", "trigger")
    float x = 0.0f;                  // X position in pixels
    float y = 0.0f;                  // Y position in pixels
    float width = 0.0f;              // Width in pixels
    float height = 0.0f;             // Height in pixels
    float rotation = 0.0f;           // Rotation in degrees

    // Custom object properties (key-value pairs)
    std::map<std::string, std::string> properties;
};

// Complete parsed Tiled map
struct TiledMap {
    int32_t width = 0;               // Map width in tiles
    int32_t height = 0;              // Map height in tiles
    int32_t tile_width = 32;         // Tile width in pixels (visual only)
    int32_t tile_height = 32;        // Tile height in pixels (visual only)
    std::string orientation = "orthogonal"; // Map orientation

    std::vector<TiledLayer> layers;  // All layers (tile layers and object layers)
    std::vector<TiledObject> objects; // All objects (flattened from object layers)

    // Tile properties by GID (Global Tile ID)
    std::map<uint32_t, TiledTileProperties> tile_properties;

    // Tileset metadata
    struct TilesetInfo {
        uint32_t first_gid;          // First GID for this tileset
        std::string name;            // Tileset name
        std::string source;          // External tileset file path (optional)
    };
    std::vector<TilesetInfo> tilesets;
};

// TiledImporter - Parses Tiled JSON/TMX files and imports to TilemapWorldSystem
class TiledImporter {
public:
    // Parse Tiled JSON (.tmj) file
    // Returns parsed TiledMap structure
    // Throws std::runtime_error on parse failure
    static TiledMap load_tiled_map(const std::string& json_path);

    // Convert TiledMap to TilemapWorldSystem
    // Spawns tiles in the world based on layer data and tile properties
    // world_origin: World-space offset for the entire map (default: 0,0,0)
    static void import_to_world(
        TilemapWorldSystem& world,
        const TiledMap& tiled_map,
        const float world_origin_x = 0.0f,
        const float world_origin_y = 0.0f,
        const float world_origin_z = 0.0f
    );

    // Validate TiledMap before import
    // Checks for missing tile properties, invalid GIDs, etc.
    // Returns error messages (empty if valid)
    static std::vector<std::string> validate_map(const TiledMap& tiled_map);

private:
    // JSON parsing helpers
    static TiledLayer parse_layer(const void* layer_json);
    static TiledTileProperties parse_tile_properties(const void* tile_json);
    static TiledObject parse_object(const void* obj_json);
    static TiledMap::TilesetInfo parse_tileset_info(const void* tileset_json);

    // Load external tileset file (.tsx) and extract tile properties
    static void load_external_tileset(
        const std::string& tileset_path,
        uint32_t first_gid,
        std::map<uint32_t, TiledTileProperties>& out_properties
    );

    // Coordinate conversion: Tiled pixel coords → tile coords
    static TileCoord pixel_to_tile_coord(
        float pixel_x,
        float pixel_y,
        int32_t tile_width,
        int32_t tile_height,
        int32_t z_offset
    );
};

} // namespace lore::world