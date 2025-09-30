#pragma once

#include <lore/math/math.hpp>
#include <lore/vision/vision_world_interface.hpp>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace lore::world {

// 3D tile coordinate (integer grid position)
struct TileCoord {
    int32_t x, y, z;

    bool operator==(const TileCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const TileCoord& other) const {
        return !(*this == other);
    }

    bool operator<(const TileCoord& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

} // namespace lore::world

// Hash function for TileCoord (for unordered_map)
namespace std {
    template<>
    struct hash<lore::world::TileCoord> {
        size_t operator()(const lore::world::TileCoord& coord) const {
            // Combine hashes using prime numbers
            size_t h1 = std::hash<int32_t>{}(coord.x);
            size_t h2 = std::hash<int32_t>{}(coord.y);
            size_t h3 = std::hash<int32_t>{}(coord.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

namespace lore::world {

// Tile definition (immutable properties shared by all tiles of this type)
struct TileDefinition {
    uint32_t id;                        // Unique definition ID
    std::string name;                   // Human-readable name (e.g., "stone_wall_3m")
    std::string mesh_path;              // Path to 3D mesh (e.g., "meshes/wall_stone.obj")

    // Physical properties
    float height_meters = 1.0f;         // Physical height in meters
    std::string collision_type = "box"; // "box", "sphere", "mesh", "none"
    bool walkable = true;               // Can entities walk on this tile?

    // Visual properties
    uint32_t material_id = 0;           // PBR material index (for DeferredRenderer)
    math::Vec3 tint_color = {1.0f, 1.0f, 1.0f}; // Color multiplier

    // Vision properties
    bool blocks_sight = false;          // Fully blocks line of sight?
    float transparency = 1.0f;          // 0.0 = opaque, 1.0 = fully transparent
    bool is_foliage = false;            // Partial occlusion (bushes, trees)

    // Gameplay properties
    bool interactable = false;          // Can be interacted with?
    std::string interaction_type;       // "door", "chest", "switch", etc.

    // Metadata
    std::map<std::string, std::string> custom_properties; // User-defined key-value pairs
};

// Tile instance (placed in world at specific coordinate)
struct TileInstance {
    uint32_t definition_id;             // References TileDefinition by ID
    TileCoord coord;                    // Position in tile grid
    float rotation_degrees = 0.0f;      // Rotation around Z-axis (0, 90, 180, 270)

    // Runtime state
    bool is_active = true;              // If false, tile is hidden/disabled
    float health = 1.0f;                // Normalized health (1.0 = undamaged, 0.0 = destroyed)

    // Per-instance overrides (optional)
    std::unique_ptr<math::Vec3> custom_tint; // Override TileDefinition tint_color
    std::unique_ptr<uint32_t> custom_material; // Override TileDefinition material_id
};

// Chunk coordinate (groups tiles for spatial partitioning)
struct ChunkCoord {
    int32_t x, y, z;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator<(const ChunkCoord& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

} // namespace lore::world

// Hash function for ChunkCoord
namespace std {
    template<>
    struct hash<lore::world::ChunkCoord> {
        size_t operator()(const lore::world::ChunkCoord& coord) const {
            size_t h1 = std::hash<int32_t>{}(coord.x);
            size_t h2 = std::hash<int32_t>{}(coord.y);
            size_t h3 = std::hash<int32_t>{}(coord.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

namespace lore::world {

// Chunk of tiles (spatial partition for efficient queries)
struct TileChunk {
    ChunkCoord coord;
    std::vector<TileInstance> tiles;    // Tiles in this chunk
    bool is_loaded = true;              // If false, chunk is unloaded from memory
    bool needs_mesh_rebuild = true;     // If true, render mesh needs regeneration
};

// TilemapWorldSystem - Manages 3D tile-based world with infinite streaming
class TilemapWorldSystem {
public:
    // Configuration
    static constexpr int32_t CHUNK_SIZE = 16; // Tiles per chunk edge (16x16x16)
    static constexpr float TILE_SIZE = 1.0f;  // Physical size of one tile in meters

    TilemapWorldSystem();
    ~TilemapWorldSystem();

    // Tile definitions management
    // Register a new tile definition (returns assigned ID)
    uint32_t register_tile_definition(const TileDefinition& definition);

    // Get tile definition by ID (returns nullptr if not found)
    const TileDefinition* get_tile_definition(uint32_t definition_id) const;

    // Update existing tile definition
    void update_tile_definition(uint32_t definition_id, const TileDefinition& definition);

    // Get all registered tile definitions
    const std::map<uint32_t, TileDefinition>& get_all_tile_definitions() const;

    // Tile instance management
    // Place tile at coordinate (overwrites existing tile if present)
    void set_tile(const TileCoord& coord, uint32_t definition_id, float rotation_degrees = 0.0f);

    // Remove tile at coordinate
    void remove_tile(const TileCoord& coord);

    // Get tile instance at coordinate (returns nullptr if empty)
    const TileInstance* get_tile(const TileCoord& coord) const;
    TileInstance* get_tile_mutable(const TileCoord& coord);

    // Check if tile exists at coordinate
    bool has_tile(const TileCoord& coord) const;

    // Get all tiles in world
    std::vector<const TileInstance*> get_all_tiles() const;

    // Get tiles in axis-aligned bounding box
    std::vector<const TileInstance*> get_tiles_in_box(
        const TileCoord& min_coord,
        const TileCoord& max_coord
    ) const;

    // Coordinate conversion
    // Convert world position to tile coordinate (floor to nearest tile)
    TileCoord world_to_tile(const math::Vec3& world_pos) const;

    // Convert tile coordinate to world position (center of tile)
    math::Vec3 tile_to_world(const TileCoord& coord) const;

    // Convert tile coordinate to world position (corner of tile)
    math::Vec3 tile_to_world_corner(const TileCoord& coord) const;

    // Chunk management
    // Get chunk coordinate for tile coordinate
    ChunkCoord tile_to_chunk(const TileCoord& tile_coord) const;

    // Get or create chunk at coordinate
    TileChunk* get_or_create_chunk(const ChunkCoord& chunk_coord);

    // Get chunk at coordinate (returns nullptr if not loaded)
    const TileChunk* get_chunk(const ChunkCoord& chunk_coord) const;
    TileChunk* get_chunk_mutable(const ChunkCoord& chunk_coord);

    // Unload chunk from memory (saves to disk if persistence enabled)
    void unload_chunk(const ChunkCoord& chunk_coord);

    // Mark chunk for mesh rebuild (call after modifying tiles)
    void mark_chunk_dirty(const ChunkCoord& chunk_coord);

    // Get all loaded chunks
    const std::unordered_map<ChunkCoord, std::unique_ptr<TileChunk>>& get_all_chunks() const;

    // Spatial queries
    // Raycast from start to end, returns first hit tile (if any)
    struct RaycastHit {
        bool hit = false;
        TileCoord coord;
        math::Vec3 hit_position;
        math::Vec3 hit_normal;
        float distance = 0.0f;
        const TileInstance* tile = nullptr;
    };
    RaycastHit raycast(const math::Vec3& start, const math::Vec3& end) const;

    // Check if world position is walkable (no blocking tile)
    bool is_walkable(const math::Vec3& world_pos) const;

    // Get ground height at XY position (highest walkable tile)
    float get_ground_height(float world_x, float world_y) const;

    // Serialization
    // Save world to JSON file
    void save_to_file(const std::string& file_path) const;

    // Load world from JSON file
    void load_from_file(const std::string& file_path);

    // Clear all tiles and chunks
    void clear();

    // Statistics
    struct Statistics {
        size_t total_tiles = 0;
        size_t loaded_chunks = 0;
        size_t total_tile_definitions = 0;
    };
    Statistics get_statistics() const;

private:
    // Tile definitions (shared immutable data)
    std::map<uint32_t, TileDefinition> tile_definitions_;
    uint32_t next_definition_id_ = 1;

    // Chunks (spatial partition)
    std::unordered_map<ChunkCoord, std::unique_ptr<TileChunk>> chunks_;

    // Tile lookup (maps tile coord to chunk + index within chunk)
    struct TileLookup {
        ChunkCoord chunk_coord;
        size_t tile_index;
    };
    std::unordered_map<TileCoord, TileLookup> tile_lookup_;

    // Helper: Get tile index within chunk
    const TileInstance* get_tile_in_chunk(const TileChunk& chunk, const TileCoord& coord) const;
    TileInstance* get_tile_in_chunk_mutable(TileChunk& chunk, const TileCoord& coord);
};

// Adapter to use TilemapWorldSystem with vision library
class TilemapVisionAdapter : public vision::IVisionWorld {
public:
    explicit TilemapVisionAdapter(const TilemapWorldSystem& tilemap);

    const vision::TileVisionData* get_tile_vision_data(const vision::TileCoord& coord) const override;
    vision::TileCoord world_to_tile(const math::Vec3& world_pos) const override;
    math::Vec3 tile_to_world(const vision::TileCoord& tile) const override;

private:
    const TilemapWorldSystem& tilemap_;
    mutable vision::TileVisionData cached_vision_data_; // Mutable for caching
};

} // namespace lore::world