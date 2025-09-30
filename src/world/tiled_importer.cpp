#include <lore/world/tiled_importer.hpp>
#include <lore/world/tilemap_world_system.hpp>
#include <lore/core/log.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace lore::world {

// Helper: Extract string from JSON with default
static std::string json_get_string(const json& j, const std::string& key, const std::string& default_val = "") {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return default_val;
}

// Helper: Extract int from JSON with default
static int32_t json_get_int(const json& j, const std::string& key, int32_t default_val = 0) {
    if (j.contains(key) && j[key].is_number_integer()) {
        return j[key].get<int32_t>();
    }
    return default_val;
}

// Helper: Extract float from JSON with default
static float json_get_float(const json& j, const std::string& key, float default_val = 0.0f) {
    if (j.contains(key) && j[key].is_number()) {
        return j[key].get<float>();
    }
    return default_val;
}

// Helper: Extract bool from JSON with default
static bool json_get_bool(const json& j, const std::string& key, bool default_val = false) {
    if (j.contains(key) && j[key].is_boolean()) {
        return j[key].get<bool>();
    }
    return default_val;
}

// Parse custom properties array from Tiled JSON
static std::map<std::string, std::string> parse_properties(const json& props_array) {
    std::map<std::string, std::string> result;

    if (!props_array.is_array()) {
        return result;
    }

    for (const auto& prop : props_array) {
        std::string name = json_get_string(prop, "name");
        if (name.empty()) continue;

        // Convert all property values to string for simplicity
        // (Caller can parse as needed)
        if (prop.contains("value")) {
            const auto& val = prop["value"];
            if (val.is_string()) {
                result[name] = val.get<std::string>();
            } else if (val.is_number_integer()) {
                result[name] = std::to_string(val.get<int64_t>());
            } else if (val.is_number_float()) {
                result[name] = std::to_string(val.get<double>());
            } else if (val.is_boolean()) {
                result[name] = val.get<bool>() ? "true" : "false";
            }
        }
    }

    return result;
}

// Parse TiledTileProperties from tile JSON
TiledTileProperties TiledImporter::parse_tile_properties(const void* tile_json_ptr) {
    const json& tile_json = *static_cast<const json*>(tile_json_ptr);
    TiledTileProperties props;

    // Parse custom properties array
    if (tile_json.contains("properties")) {
        auto prop_map = parse_properties(tile_json["properties"]);

        if (prop_map.count("mesh_path")) {
            props.mesh_path = prop_map["mesh_path"];
        }
        if (prop_map.count("height")) {
            props.height = std::stof(prop_map["height"]);
        }
        if (prop_map.count("collision_type")) {
            props.collision_type = prop_map["collision_type"];
        }
        if (prop_map.count("material_id")) {
            props.material_id = std::stoul(prop_map["material_id"]);
        }
        if (prop_map.count("walkable")) {
            props.walkable = (prop_map["walkable"] == "true");
        }
        if (prop_map.count("blocks_sight")) {
            props.blocks_sight = (prop_map["blocks_sight"] == "true");
        }
    }

    return props;
}

// Parse TiledLayer from layer JSON
TiledLayer TiledImporter::parse_layer(const void* layer_json_ptr) {
    const json& layer_json = *static_cast<const json*>(layer_json_ptr);
    TiledLayer layer;

    layer.name = json_get_string(layer_json, "name");
    layer.type = json_get_string(layer_json, "type", "tilelayer");
    layer.width = json_get_int(layer_json, "width");
    layer.height = json_get_int(layer_json, "height");
    layer.visible = json_get_bool(layer_json, "visible", true);

    // Parse tile data array (for tilelayers)
    if (layer.type == "tilelayer" && layer_json.contains("data")) {
        const auto& data_array = layer_json["data"];
        if (data_array.is_array()) {
            for (const auto& gid : data_array) {
                layer.data.push_back(gid.get<uint32_t>());
            }
        }
    }

    // Parse custom properties
    if (layer_json.contains("properties")) {
        layer.properties = parse_properties(layer_json["properties"]);

        // Extract z_offset if specified
        if (layer.properties.count("z_offset")) {
            layer.z_offset = std::stoi(layer.properties["z_offset"]);
        }
    }

    return layer;
}

// Parse TiledObject from object JSON
TiledObject TiledImporter::parse_object(const void* obj_json_ptr) {
    const json& obj_json = *static_cast<const json*>(obj_json_ptr);
    TiledObject obj;

    obj.id = json_get_int(obj_json, "id");
    obj.name = json_get_string(obj_json, "name");
    obj.type = json_get_string(obj_json, "type");
    obj.x = json_get_float(obj_json, "x");
    obj.y = json_get_float(obj_json, "y");
    obj.width = json_get_float(obj_json, "width");
    obj.height = json_get_float(obj_json, "height");
    obj.rotation = json_get_float(obj_json, "rotation");

    // Parse custom properties
    if (obj_json.contains("properties")) {
        obj.properties = parse_properties(obj_json["properties"]);
    }

    return obj;
}

// Parse TilesetInfo from tileset JSON
TiledMap::TilesetInfo TiledImporter::parse_tileset_info(const void* tileset_json_ptr) {
    const json& tileset_json = *static_cast<const json*>(tileset_json_ptr);
    TiledMap::TilesetInfo info;

    info.first_gid = json_get_int(tileset_json, "firstgid", 1);
    info.name = json_get_string(tileset_json, "name");
    info.source = json_get_string(tileset_json, "source");

    return info;
}

// Load external tileset file (.tsx as JSON)
void TiledImporter::load_external_tileset(
    const std::string& tileset_path,
    uint32_t first_gid,
    std::map<uint32_t, TiledTileProperties>& out_properties)
{
    std::ifstream file(tileset_path);
    if (!file.is_open()) {
        LOG_WARNING(Game, "Failed to open external tileset: {}", tileset_path);
        return;
    }

    json tileset_json;
    try {
        file >> tileset_json;
    } catch (const std::exception& e) {
        LOG_ERROR(Game, "Failed to parse tileset JSON: {} - {}", tileset_path, e.what());
        return;
    }

    // Parse tiles array
    if (tileset_json.contains("tiles") && tileset_json["tiles"].is_array()) {
        for (const auto& tile_json : tileset_json["tiles"]) {
            uint32_t tile_id = json_get_int(tile_json, "id");
            uint32_t gid = first_gid + tile_id;

            TiledTileProperties props = parse_tile_properties(&tile_json);
            out_properties[gid] = props;
        }
    }
}

// Load and parse Tiled map JSON (.tmj)
TiledMap TiledImporter::load_tiled_map(const std::string& json_path) {
    LOG_INFO(Game, "Loading Tiled map: {}", json_path);

    // Open and parse JSON file
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open Tiled map file: " + json_path);
    }

    json map_json;
    try {
        file >> map_json;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse Tiled map JSON: " + std::string(e.what()));
    }

    TiledMap map;

    // Parse map dimensions
    map.width = json_get_int(map_json, "width");
    map.height = json_get_int(map_json, "height");
    map.tile_width = json_get_int(map_json, "tilewidth", 32);
    map.tile_height = json_get_int(map_json, "tileheight", 32);
    map.orientation = json_get_string(map_json, "orientation", "orthogonal");

    LOG_INFO(Game, "Map size: {}x{} tiles ({}x{} px per tile)",
             map.width, map.height, map.tile_width, map.tile_height);

    // Parse tilesets
    if (map_json.contains("tilesets") && map_json["tilesets"].is_array()) {
        for (const auto& tileset_json : map_json["tilesets"]) {
            TiledMap::TilesetInfo info = parse_tileset_info(&tileset_json);
            map.tilesets.push_back(info);

            // If tileset has inline tiles, parse their properties
            if (tileset_json.contains("tiles") && tileset_json["tiles"].is_array()) {
                for (const auto& tile_json : tileset_json["tiles"]) {
                    uint32_t tile_id = json_get_int(tile_json, "id");
                    uint32_t gid = info.first_gid + tile_id;

                    TiledTileProperties props = parse_tile_properties(&tile_json);
                    map.tile_properties[gid] = props;
                }
            }

            // If tileset is external, load from file
            if (!info.source.empty()) {
                // Resolve path relative to map file
                std::string tileset_path = info.source;
                // TODO: Proper path resolution
                load_external_tileset(tileset_path, info.first_gid, map.tile_properties);
            }
        }
    }

    LOG_INFO(Game, "Loaded {} tilesets with {} tile definitions",
             map.tilesets.size(), map.tile_properties.size());

    // Parse layers
    if (map_json.contains("layers") && map_json["layers"].is_array()) {
        for (const auto& layer_json : map_json["layers"]) {
            TiledLayer layer = parse_layer(&layer_json);
            map.layers.push_back(layer);

            // If this is an object layer, extract objects
            if (layer.type == "objectgroup" && layer_json.contains("objects")) {
                const auto& objects_array = layer_json["objects"];
                if (objects_array.is_array()) {
                    for (const auto& obj_json : objects_array) {
                        TiledObject obj = parse_object(&obj_json);
                        map.objects.push_back(obj);
                    }
                }
            }
        }
    }

    LOG_INFO(Game, "Parsed {} layers with {} objects",
             map.layers.size(), map.objects.size());

    return map;
}

// Validate TiledMap before import
std::vector<std::string> TiledImporter::validate_map(const TiledMap& tiled_map) {
    std::vector<std::string> errors;

    // Check map dimensions
    if (tiled_map.width <= 0 || tiled_map.height <= 0) {
        errors.push_back("Invalid map dimensions: " +
                        std::to_string(tiled_map.width) + "x" +
                        std::to_string(tiled_map.height));
    }

    // Check for layers
    if (tiled_map.layers.empty()) {
        errors.push_back("Map has no layers");
    }

    // Validate each layer
    for (const auto& layer : tiled_map.layers) {
        if (layer.type == "tilelayer") {
            // Check data array size
            size_t expected_size = static_cast<size_t>(layer.width) * layer.height;
            if (layer.data.size() != expected_size) {
                errors.push_back("Layer '" + layer.name + "' data size mismatch: expected " +
                                std::to_string(expected_size) + ", got " +
                                std::to_string(layer.data.size()));
            }

            // Check for invalid GIDs (referencing non-existent tiles)
            for (uint32_t gid : layer.data) {
                if (gid != 0 && tiled_map.tile_properties.find(gid) == tiled_map.tile_properties.end()) {
                    errors.push_back("Layer '" + layer.name + "' references undefined tile GID: " +
                                    std::to_string(gid));
                    break; // Only report once per layer
                }
            }
        }
    }

    // Validate tile properties
    for (const auto& [gid, props] : tiled_map.tile_properties) {
        if (props.mesh_path.empty()) {
            errors.push_back("Tile GID " + std::to_string(gid) + " has no mesh_path defined");
        }
        if (props.height <= 0.0f || props.height > 16.0f) {
            errors.push_back("Tile GID " + std::to_string(gid) + " has invalid height: " +
                            std::to_string(props.height));
        }
    }

    return errors;
}

// Convert pixel coordinates to tile coordinates
TileCoord TiledImporter::pixel_to_tile_coord(
    float pixel_x,
    float pixel_y,
    int32_t tile_width,
    int32_t tile_height,
    int32_t z_offset)
{
    return TileCoord{
        static_cast<int32_t>(pixel_x / tile_width),
        static_cast<int32_t>(pixel_y / tile_height),
        z_offset
    };
}

// Import TiledMap to TilemapWorldSystem
void TiledImporter::import_to_world(
    TilemapWorldSystem& world,
    const TiledMap& tiled_map,
    const float world_origin_x,
    const float world_origin_y,
    const float world_origin_z)
{
    LOG_INFO(Game, "Importing Tiled map to world (origin: {}, {}, {})",
             world_origin_x, world_origin_y, world_origin_z);

    // Validate map before import
    auto errors = validate_map(tiled_map);
    if (!errors.empty()) {
        LOG_ERROR(Game, "Tiled map validation failed:");
        for (const auto& error : errors) {
            LOG_ERROR(Game, "  - {}", error);
        }
        throw std::runtime_error("Tiled map validation failed with " +
                                std::to_string(errors.size()) + " errors");
    }

    // Cache for registered tile definitions (GID -> definition_id)
    std::map<uint32_t, uint32_t> gid_to_def_id;

    // Process each layer
    for (const auto& layer : tiled_map.layers) {
        if (layer.type != "tilelayer") {
            continue; // Skip object layers (handled separately)
        }

        if (!layer.visible) {
            LOG_DEBUG(Game, "Skipping invisible layer: {}", layer.name);
            continue;
        }

        LOG_INFO(Game, "Processing layer: {} ({}x{}, z_offset={})",
                 layer.name, layer.width, layer.height, layer.z_offset);

        // Iterate through all tiles in layer
        for (int32_t y = 0; y < layer.height; y++) {
            for (int32_t x = 0; x < layer.width; x++) {
                size_t index = x + y * layer.width;
                uint32_t gid = layer.data[index];

                if (gid == 0) {
                    continue; // Empty tile
                }

                // Get tile properties
                auto props_it = tiled_map.tile_properties.find(gid);
                if (props_it == tiled_map.tile_properties.end()) {
                    LOG_WARNING(Game, "Tile GID {} has no properties, skipping", gid);
                    continue;
                }

                const TiledTileProperties& props = props_it->second;

                // Check if we already registered this GID as a definition
                uint32_t def_id = 0;
                auto cached_it = gid_to_def_id.find(gid);
                if (cached_it != gid_to_def_id.end()) {
                    def_id = cached_it->second;
                } else {
                    // Create TileDefinition from Tiled properties
                    TileDefinition def;
                    def.name = layer.name + "_tile_" + std::to_string(gid);
                    def.mesh_path = props.mesh_path;
                    def.height_meters = props.height; // Height in meters (supports fractional values)
                    def.collision_type = props.collision_type;
                    def.walkable = props.walkable;
                    def.material_id = props.material_id;
                    def.blocks_sight = props.blocks_sight;
                    def.transparency = 1.0f; // Default fully transparent
                    def.is_foliage = false;
                    def.tint_color = math::Vec3(1.0f, 1.0f, 1.0f);

                    // Register tile definition
                    def_id = world.register_tile_definition(def);
                    gid_to_def_id[gid] = def_id;
                }

                // Place single tile at this coordinate
                // Height is stored in the tile definition, not as vertical stacking
                TileCoord coord{
                    x + static_cast<int32_t>(world_origin_x),
                    y + static_cast<int32_t>(world_origin_y),
                    layer.z_offset + static_cast<int32_t>(world_origin_z)
                };

                // Place tile in world
                world.set_tile(coord, def_id, 0.0f);
            }
        }
    }

    // Process object layers (spawn points, lights, triggers)
    LOG_INFO(Game, "Processing {} objects", tiled_map.objects.size());
    for (const auto& obj : tiled_map.objects) {
        TileCoord spawn_coord = pixel_to_tile_coord(
            obj.x,
            obj.y,
            tiled_map.tile_width,
            tiled_map.tile_height,
            0
        );

        spawn_coord.x += static_cast<int32_t>(world_origin_x);
        spawn_coord.y += static_cast<int32_t>(world_origin_y);
        spawn_coord.z += static_cast<int32_t>(world_origin_z);

        // Handle different object types
        if (obj.type == "spawn_point") {
            LOG_INFO(Game, "Found spawn point '{}' at tile ({}, {}, {})",
                     obj.name, spawn_coord.x, spawn_coord.y, spawn_coord.z);

            // TODO: Spawn entity based on properties
            // Example: world.spawn_entity(spawn_coord, obj.properties["entity_type"]);
        }
        else if (obj.type == "light") {
            LOG_INFO(Game, "Found light '{}' at tile ({}, {}, {})",
                     obj.name, spawn_coord.x, spawn_coord.y, spawn_coord.z);

            // TODO: Create light in DeferredRenderer
            // Extract light_type, intensity, color from obj.properties
        }
        else if (obj.type == "trigger") {
            LOG_INFO(Game, "Found trigger '{}' at tile ({}, {}, {})",
                     obj.name, spawn_coord.x, spawn_coord.y, spawn_coord.z);

            // TODO: Create trigger volume
        }
    }

    LOG_INFO(Game, "Tiled map import complete");
}

} // namespace lore::world