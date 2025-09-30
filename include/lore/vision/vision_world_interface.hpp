#pragma once

#include <lore/math/math.hpp>
#include <cstdint>

namespace lore::vision {

// Forward declarations to avoid circular dependencies
struct TileCoord {
    int32_t x;
    int32_t y;
    int32_t z;

    bool operator==(const TileCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Minimal tile occlusion data needed by vision system
struct TileVisionData {
    bool blocks_sight = false;              // Does this tile fully block vision?
    float transparency = 1.0f;              // 0.0 = opaque, 1.0 = fully transparent
    float height_meters = 1.0f;             // Physical height of obstacle
    bool is_foliage = false;                // Vegetation (partial occlusion)
};

// Interface that any world/tilemap system must implement to work with vision library
class IVisionWorld {
public:
    virtual ~IVisionWorld() = default;

    // Get vision data for a tile at given coordinates
    // Returns nullptr if tile doesn't exist or is empty air
    virtual const TileVisionData* get_tile_vision_data(const TileCoord& coord) const = 0;

    // Convert world position to tile coordinate
    virtual TileCoord world_to_tile(const math::Vec3& world_pos) const = 0;

    // Convert tile coordinate to world position (center of tile)
    virtual math::Vec3 tile_to_world(const TileCoord& tile) const = 0;
};

} // namespace lore::vision