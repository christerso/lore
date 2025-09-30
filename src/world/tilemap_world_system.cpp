#include <lore/world/tilemap_world_system.hpp>
#include <lore/core/log.hpp>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace lore::world {

TilemapWorldSystem::TilemapWorldSystem() {
    LOG_INFO(Game, "TilemapWorldSystem initialized");
}

TilemapWorldSystem::~TilemapWorldSystem() {
    LOG_INFO(Game, "TilemapWorldSystem destroyed");
}

// Tile definitions management

uint32_t TilemapWorldSystem::register_tile_definition(const TileDefinition& definition) {
    uint32_t id = next_definition_id_++;
    TileDefinition def_copy = definition;
    def_copy.id = id;
    tile_definitions_[id] = def_copy;
    LOG_DEBUG(Game, "Registered tile definition: id={}, name={}", id, definition.name);
    return id;
}

const TileDefinition* TilemapWorldSystem::get_tile_definition(uint32_t definition_id) const {
    auto it = tile_definitions_.find(definition_id);
    return it != tile_definitions_.end() ? &it->second : nullptr;
}

void TilemapWorldSystem::update_tile_definition(uint32_t definition_id, const TileDefinition& definition) {
    auto it = tile_definitions_.find(definition_id);
    if (it != tile_definitions_.end()) {
        TileDefinition def_copy = definition;
        def_copy.id = definition_id;
        it->second = def_copy;
        LOG_DEBUG(Game, "Updated tile definition: id={}, name={}", definition_id, definition.name);
    } else {
        LOG_WARNING(Game, "Attempted to update non-existent tile definition: id={}", definition_id);
    }
}

const std::map<uint32_t, TileDefinition>& TilemapWorldSystem::get_all_tile_definitions() const {
    return tile_definitions_;
}

// Tile instance management

void TilemapWorldSystem::set_tile(const TileCoord& coord, uint32_t definition_id, float rotation_degrees) {
    // Verify definition exists
    if (!get_tile_definition(definition_id)) {
        LOG_ERROR(Game, "Cannot set tile: definition_id={} does not exist", definition_id);
        return;
    }

    // Get or create chunk
    ChunkCoord chunk_coord = tile_to_chunk(coord);
    TileChunk* chunk = get_or_create_chunk(chunk_coord);

    // Check if tile already exists at this coordinate
    auto lookup_it = tile_lookup_.find(coord);
    if (lookup_it != tile_lookup_.end()) {
        // Update existing tile
        TileInstance& existing_tile = chunk->tiles[lookup_it->second.tile_index];
        existing_tile.definition_id = definition_id;
        existing_tile.rotation_degrees = rotation_degrees;
        LOG_DEBUG(Game, "Updated tile at ({}, {}, {}) to definition_id={}",
                  coord.x, coord.y, coord.z, definition_id);
    } else {
        // Add new tile
        TileInstance new_tile;
        new_tile.definition_id = definition_id;
        new_tile.coord = coord;
        new_tile.rotation_degrees = rotation_degrees;

        size_t tile_index = chunk->tiles.size();
        chunk->tiles.push_back(std::move(new_tile));

        // Update lookup
        TileLookup lookup;
        lookup.chunk_coord = chunk_coord;
        lookup.tile_index = tile_index;
        tile_lookup_[coord] = lookup;

        LOG_DEBUG(Game, "Added tile at ({}, {}, {}) with definition_id={}",
                  coord.x, coord.y, coord.z, definition_id);
    }

    // Mark chunk dirty for mesh rebuild
    chunk->needs_mesh_rebuild = true;
}

void TilemapWorldSystem::remove_tile(const TileCoord& coord) {
    auto lookup_it = tile_lookup_.find(coord);
    if (lookup_it == tile_lookup_.end()) {
        LOG_DEBUG(Game, "No tile to remove at ({}, {}, {})", coord.x, coord.y, coord.z);
        return;
    }

    ChunkCoord chunk_coord = lookup_it->second.chunk_coord;
    size_t tile_index = lookup_it->second.tile_index;

    TileChunk* chunk = get_chunk_mutable(chunk_coord);
    if (!chunk) {
        LOG_ERROR(Game, "Chunk not found when removing tile at ({}, {}, {})", coord.x, coord.y, coord.z);
        tile_lookup_.erase(lookup_it);
        return;
    }

    // Remove tile from chunk (swap with last tile)
    if (tile_index < chunk->tiles.size() - 1) {
        // Swap with last tile
        chunk->tiles[tile_index] = std::move(chunk->tiles.back());

        // Update lookup for swapped tile
        TileCoord swapped_coord = chunk->tiles[tile_index].coord;
        tile_lookup_[swapped_coord].tile_index = tile_index;
    }
    chunk->tiles.pop_back();

    // Remove lookup entry
    tile_lookup_.erase(lookup_it);

    // Mark chunk dirty
    chunk->needs_mesh_rebuild = true;

    LOG_DEBUG(Game, "Removed tile at ({}, {}, {})", coord.x, coord.y, coord.z);
}

const TileInstance* TilemapWorldSystem::get_tile(const TileCoord& coord) const {
    auto lookup_it = tile_lookup_.find(coord);
    if (lookup_it == tile_lookup_.end()) {
        return nullptr;
    }

    const TileChunk* chunk = get_chunk(lookup_it->second.chunk_coord);
    if (!chunk || lookup_it->second.tile_index >= chunk->tiles.size()) {
        return nullptr;
    }

    return &chunk->tiles[lookup_it->second.tile_index];
}

TileInstance* TilemapWorldSystem::get_tile_mutable(const TileCoord& coord) {
    auto lookup_it = tile_lookup_.find(coord);
    if (lookup_it == tile_lookup_.end()) {
        return nullptr;
    }

    TileChunk* chunk = get_chunk_mutable(lookup_it->second.chunk_coord);
    if (!chunk || lookup_it->second.tile_index >= chunk->tiles.size()) {
        return nullptr;
    }

    return &chunk->tiles[lookup_it->second.tile_index];
}

bool TilemapWorldSystem::has_tile(const TileCoord& coord) const {
    return tile_lookup_.find(coord) != tile_lookup_.end();
}

std::vector<const TileInstance*> TilemapWorldSystem::get_all_tiles() const {
    std::vector<const TileInstance*> all_tiles;
    for (const auto& [chunk_coord, chunk] : chunks_) {
        for (const TileInstance& tile : chunk->tiles) {
            all_tiles.push_back(&tile);
        }
    }
    return all_tiles;
}

std::vector<const TileInstance*> TilemapWorldSystem::get_tiles_in_box(
    const TileCoord& min_coord,
    const TileCoord& max_coord) const
{
    std::vector<const TileInstance*> tiles_in_box;

    for (int32_t x = min_coord.x; x <= max_coord.x; ++x) {
        for (int32_t y = min_coord.y; y <= max_coord.y; ++y) {
            for (int32_t z = min_coord.z; z <= max_coord.z; ++z) {
                TileCoord coord{x, y, z};
                const TileInstance* tile = get_tile(coord);
                if (tile) {
                    tiles_in_box.push_back(tile);
                }
            }
        }
    }

    return tiles_in_box;
}

// Coordinate conversion

TileCoord TilemapWorldSystem::world_to_tile(const math::Vec3& world_pos) const {
    return TileCoord{
        static_cast<int32_t>(std::floor(world_pos.x / TILE_SIZE)),
        static_cast<int32_t>(std::floor(world_pos.y / TILE_SIZE)),
        static_cast<int32_t>(std::floor(world_pos.z / TILE_SIZE))
    };
}

math::Vec3 TilemapWorldSystem::tile_to_world(const TileCoord& coord) const {
    return math::Vec3(
        (static_cast<float>(coord.x) + 0.5f) * TILE_SIZE,
        (static_cast<float>(coord.y) + 0.5f) * TILE_SIZE,
        (static_cast<float>(coord.z) + 0.5f) * TILE_SIZE
    );
}

math::Vec3 TilemapWorldSystem::tile_to_world_corner(const TileCoord& coord) const {
    return math::Vec3(
        static_cast<float>(coord.x) * TILE_SIZE,
        static_cast<float>(coord.y) * TILE_SIZE,
        static_cast<float>(coord.z) * TILE_SIZE
    );
}

// Chunk management

ChunkCoord TilemapWorldSystem::tile_to_chunk(const TileCoord& tile_coord) const {
    auto floor_div = [](int32_t a, int32_t b) -> int32_t {
        // Floor division for negative numbers
        int32_t q = a / b;
        int32_t r = a % b;
        return (r != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
    };

    return ChunkCoord{
        floor_div(tile_coord.x, CHUNK_SIZE),
        floor_div(tile_coord.y, CHUNK_SIZE),
        floor_div(tile_coord.z, CHUNK_SIZE)
    };
}

TileChunk* TilemapWorldSystem::get_or_create_chunk(const ChunkCoord& chunk_coord) {
    auto it = chunks_.find(chunk_coord);
    if (it != chunks_.end()) {
        return it->second.get();
    }

    // Create new chunk
    auto new_chunk = std::make_unique<TileChunk>();
    new_chunk->coord = chunk_coord;
    new_chunk->is_loaded = true;
    new_chunk->needs_mesh_rebuild = true;

    TileChunk* chunk_ptr = new_chunk.get();
    chunks_[chunk_coord] = std::move(new_chunk);

    LOG_DEBUG(Game, "Created chunk at ({}, {}, {})", chunk_coord.x, chunk_coord.y, chunk_coord.z);
    return chunk_ptr;
}

const TileChunk* TilemapWorldSystem::get_chunk(const ChunkCoord& chunk_coord) const {
    auto it = chunks_.find(chunk_coord);
    return it != chunks_.end() ? it->second.get() : nullptr;
}

TileChunk* TilemapWorldSystem::get_chunk_mutable(const ChunkCoord& chunk_coord) {
    auto it = chunks_.find(chunk_coord);
    return it != chunks_.end() ? it->second.get() : nullptr;
}

void TilemapWorldSystem::unload_chunk(const ChunkCoord& chunk_coord) {
    auto it = chunks_.find(chunk_coord);
    if (it == chunks_.end()) {
        return;
    }

    // Remove all tile lookups for this chunk
    const TileChunk& chunk = *it->second;
    for (const TileInstance& tile : chunk.tiles) {
        tile_lookup_.erase(tile.coord);
    }

    // Remove chunk
    chunks_.erase(it);
    LOG_DEBUG(Game, "Unloaded chunk at ({}, {}, {})", chunk_coord.x, chunk_coord.y, chunk_coord.z);
}

void TilemapWorldSystem::mark_chunk_dirty(const ChunkCoord& chunk_coord) {
    TileChunk* chunk = get_chunk_mutable(chunk_coord);
    if (chunk) {
        chunk->needs_mesh_rebuild = true;
    }
}

const std::unordered_map<ChunkCoord, std::unique_ptr<TileChunk>>& TilemapWorldSystem::get_all_chunks() const {
    return chunks_;
}

// Spatial queries

TilemapWorldSystem::RaycastHit TilemapWorldSystem::raycast(const math::Vec3& start, const math::Vec3& end) const {
    RaycastHit hit;

    math::Vec3 ray_dir = end - start;
    float ray_length = glm::length(ray_dir);
    if (ray_length < 0.001f) {
        return hit; // No distance to trace
    }
    ray_dir = ray_dir / ray_length; // Normalize

    // DDA algorithm for voxel traversal
    TileCoord current = world_to_tile(start);
    TileCoord end_tile = world_to_tile(end);

    int32_t step_x = ray_dir.x > 0 ? 1 : -1;
    int32_t step_y = ray_dir.y > 0 ? 1 : -1;
    int32_t step_z = ray_dir.z > 0 ? 1 : -1;

    // Calculate t_max and t_delta for each axis
    auto calc_t_max = [&](float pos, float dir, int32_t tile, int32_t step) -> float {
        if (std::abs(dir) < 0.0001f) return 1e30f; // Infinite
        float boundary = (tile + (step > 0 ? 1.0f : 0.0f)) * TILE_SIZE;
        return (boundary - pos) / dir;
    };

    auto calc_t_delta = [&](float dir, [[maybe_unused]] int32_t step) -> float {
        if (std::abs(dir) < 0.0001f) return 1e30f;
        return std::abs(TILE_SIZE / dir);
    };

    float t_max_x = calc_t_max(start.x, ray_dir.x, current.x, step_x);
    float t_max_y = calc_t_max(start.y, ray_dir.y, current.y, step_y);
    float t_max_z = calc_t_max(start.z, ray_dir.z, current.z, step_z);

    float t_delta_x = calc_t_delta(ray_dir.x, step_x);
    float t_delta_y = calc_t_delta(ray_dir.y, step_y);
    float t_delta_z = calc_t_delta(ray_dir.z, step_z);

    // Traverse voxels
    int32_t max_steps = 1000; // Safety limit
    for (int32_t step = 0; step < max_steps; ++step) {
        // Check current tile
        const TileInstance* tile = get_tile(current);
        if (tile) {
            const TileDefinition* def = get_tile_definition(tile->definition_id);
            if (def && def->collision_type != "none") {
                // Hit!
                hit.hit = true;
                hit.coord = current;
                hit.tile = tile;

                // Calculate hit position and normal
                float t = std::min({t_max_x, t_max_y, t_max_z}) - 0.001f; // Slightly inside tile
                hit.hit_position = start + ray_dir * t;
                hit.distance = t;

                // Determine normal based on which axis was crossed
                if (t_max_x < t_max_y && t_max_x < t_max_z) {
                    hit.hit_normal = math::Vec3(-static_cast<float>(step_x), 0.0f, 0.0f);
                } else if (t_max_y < t_max_z) {
                    hit.hit_normal = math::Vec3(0.0f, -static_cast<float>(step_y), 0.0f);
                } else {
                    hit.hit_normal = math::Vec3(0.0f, 0.0f, -static_cast<float>(step_z));
                }

                return hit;
            }
        }

        // Reached end
        if (current == end_tile) {
            break;
        }

        // Step to next voxel
        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            t_max_x += t_delta_x;
            current.x += step_x;
        } else if (t_max_y < t_max_z) {
            t_max_y += t_delta_y;
            current.y += step_y;
        } else {
            t_max_z += t_delta_z;
            current.z += step_z;
        }
    }

    return hit; // No hit
}

bool TilemapWorldSystem::is_walkable(const math::Vec3& world_pos) const {
    TileCoord coord = world_to_tile(world_pos);
    const TileInstance* tile = get_tile(coord);
    if (!tile) {
        return true; // No tile = walkable air
    }

    const TileDefinition* def = get_tile_definition(tile->definition_id);
    return def && def->walkable;
}

float TilemapWorldSystem::get_ground_height(float world_x, float world_y) const {
    // Search downward from Z=0 to find highest walkable tile
    float highest_walkable = 0.0f;
    bool found = false;

    for (int32_t z = 0; z >= -100; --z) { // Search down to -100
        TileCoord coord{
            static_cast<int32_t>(std::floor(world_x / TILE_SIZE)),
            static_cast<int32_t>(std::floor(world_y / TILE_SIZE)),
            z
        };

        const TileInstance* tile = get_tile(coord);
        if (tile) {
            const TileDefinition* def = get_tile_definition(tile->definition_id);
            if (def && def->walkable) {
                float tile_top = (static_cast<float>(z) + 1.0f) * TILE_SIZE;
                if (!found || tile_top > highest_walkable) {
                    highest_walkable = tile_top;
                    found = true;
                }
            }
        }
    }

    return highest_walkable;
}

// Serialization

void TilemapWorldSystem::save_to_file(const std::string& file_path) const {
    json j;

    // Save tile definitions
    json definitions_array = json::array();
    for (const auto& [id, def] : tile_definitions_) {
        json def_json;
        def_json["id"] = def.id;
        def_json["name"] = def.name;
        def_json["mesh_path"] = def.mesh_path;
        def_json["height_meters"] = def.height_meters;
        def_json["collision_type"] = def.collision_type;
        def_json["walkable"] = def.walkable;
        def_json["material_id"] = def.material_id;
        def_json["tint_color"] = {def.tint_color.x, def.tint_color.y, def.tint_color.z};
        def_json["blocks_sight"] = def.blocks_sight;
        def_json["transparency"] = def.transparency;
        def_json["is_foliage"] = def.is_foliage;
        def_json["interactable"] = def.interactable;
        def_json["interaction_type"] = def.interaction_type;
        def_json["custom_properties"] = def.custom_properties;
        definitions_array.push_back(def_json);
    }
    j["tile_definitions"] = definitions_array;

    // Save tiles
    json tiles_array = json::array();
    for (const auto& [chunk_coord, chunk] : chunks_) {
        for (const TileInstance& tile : chunk->tiles) {
            json tile_json;
            tile_json["definition_id"] = tile.definition_id;
            tile_json["coord"] = {tile.coord.x, tile.coord.y, tile.coord.z};
            tile_json["rotation_degrees"] = tile.rotation_degrees;
            tile_json["is_active"] = tile.is_active;
            tile_json["health"] = tile.health;

            if (tile.custom_tint) {
                tile_json["custom_tint"] = {tile.custom_tint->x, tile.custom_tint->y, tile.custom_tint->z};
            }
            if (tile.custom_material) {
                tile_json["custom_material"] = *tile.custom_material;
            }

            tiles_array.push_back(tile_json);
        }
    }
    j["tiles"] = tiles_array;

    // Write to file
    std::ofstream file(file_path);
    if (!file.is_open()) {
        LOG_ERROR(Game, "Failed to open file for writing: {}", file_path);
        return;
    }

    file << j.dump(2); // Pretty print with 2-space indent
    LOG_INFO(Game, "Saved world to: {}", file_path);
}

void TilemapWorldSystem::load_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        LOG_ERROR(Game, "Failed to open file for reading: {}", file_path);
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        LOG_ERROR(Game, "Failed to parse JSON: {}", e.what());
        return;
    }

    clear();

    // Load tile definitions
    if (j.contains("tile_definitions")) {
        for (const json& def_json : j["tile_definitions"]) {
            TileDefinition def;
            def.id = def_json.value("id", 0);
            def.name = def_json.value("name", "");
            def.mesh_path = def_json.value("mesh_path", "");
            def.height_meters = def_json.value("height_meters", 1.0f);
            def.collision_type = def_json.value("collision_type", "box");
            def.walkable = def_json.value("walkable", true);
            def.material_id = def_json.value("material_id", 0);

            if (def_json.contains("tint_color")) {
                auto color = def_json["tint_color"];
                def.tint_color = math::Vec3(color[0], color[1], color[2]);
            }

            def.blocks_sight = def_json.value("blocks_sight", false);
            def.transparency = def_json.value("transparency", 1.0f);
            def.is_foliage = def_json.value("is_foliage", false);
            def.interactable = def_json.value("interactable", false);
            def.interaction_type = def_json.value("interaction_type", "");

            if (def_json.contains("custom_properties")) {
                def.custom_properties = def_json["custom_properties"].get<std::map<std::string, std::string>>();
            }

            tile_definitions_[def.id] = def;
            next_definition_id_ = std::max(next_definition_id_, def.id + 1);
        }
    }

    // Load tiles
    if (j.contains("tiles")) {
        for (const json& tile_json : j["tiles"]) {
            uint32_t definition_id = tile_json.value("definition_id", 0);

            TileCoord coord;
            if (tile_json.contains("coord")) {
                auto c = tile_json["coord"];
                coord = TileCoord{c[0], c[1], c[2]};
            }

            float rotation = tile_json.value("rotation_degrees", 0.0f);
            set_tile(coord, definition_id, rotation);

            // Set runtime state
            TileInstance* tile = get_tile_mutable(coord);
            if (tile) {
                tile->is_active = tile_json.value("is_active", true);
                tile->health = tile_json.value("health", 1.0f);

                if (tile_json.contains("custom_tint")) {
                    auto tint = tile_json["custom_tint"];
                    tile->custom_tint = std::make_unique<math::Vec3>(tint[0], tint[1], tint[2]);
                }

                if (tile_json.contains("custom_material")) {
                    tile->custom_material = std::make_unique<uint32_t>(tile_json["custom_material"]);
                }
            }
        }
    }

    LOG_INFO(Game, "Loaded world from: {}", file_path);
}

void TilemapWorldSystem::clear() {
    chunks_.clear();
    tile_lookup_.clear();
    LOG_INFO(Game, "Cleared all tiles and chunks");
}

TilemapWorldSystem::Statistics TilemapWorldSystem::get_statistics() const {
    Statistics stats;
    stats.total_tile_definitions = tile_definitions_.size();
    stats.loaded_chunks = chunks_.size();
    stats.total_tiles = 0;
    for (const auto& [coord, chunk] : chunks_) {
        stats.total_tiles += chunk->tiles.size();
    }
    return stats;
}

// TilemapVisionAdapter implementation

TilemapVisionAdapter::TilemapVisionAdapter(const TilemapWorldSystem& tilemap)
    : tilemap_(tilemap) {}

const vision::TileVisionData* TilemapVisionAdapter::get_tile_vision_data(const vision::TileCoord& coord) const {
    // Convert vision::TileCoord to world::TileCoord
    world::TileCoord world_coord{coord.x, coord.y, coord.z};

    const TileInstance* tile = tilemap_.get_tile(world_coord);
    if (!tile) {
        return nullptr; // No tile = air
    }

    const TileDefinition* def = tilemap_.get_tile_definition(tile->definition_id);
    if (!def) {
        return nullptr;
    }

    // Populate cached vision data
    cached_vision_data_.blocks_sight = def->blocks_sight;
    cached_vision_data_.transparency = def->transparency;
    cached_vision_data_.height_meters = def->height_meters;
    cached_vision_data_.is_foliage = def->is_foliage;

    return &cached_vision_data_;
}

vision::TileCoord TilemapVisionAdapter::world_to_tile(const math::Vec3& world_pos) const {
    world::TileCoord world_coord = tilemap_.world_to_tile(world_pos);
    return vision::TileCoord{world_coord.x, world_coord.y, world_coord.z};
}

math::Vec3 TilemapVisionAdapter::tile_to_world(const vision::TileCoord& tile) const {
    world::TileCoord world_coord{tile.x, tile.y, tile.z};
    return tilemap_.tile_to_world(world_coord);
}

} // namespace lore::world