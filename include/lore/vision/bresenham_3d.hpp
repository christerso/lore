#pragma once

#include <lore/vision/vision_world_interface.hpp>
#include <lore/vision/vision_profile.hpp>
#include <vector>
#include <functional>

namespace lore::vision {

// Result of a 3D Bresenham line trace
struct LineTraceResult {
    bool hit = false;                       // Did the line hit an obstacle?
    TileCoord hit_tile;                     // Tile that blocked the line
    math::Vec3 hit_point;                   // World-space point where line was blocked
    float distance = 0.0f;                  // Distance traveled before hit
    float accumulated_transparency = 1.0f;  // Visibility after passing through transparent tiles
};

// 3D Bresenham line algorithm
// Iterates through all tiles along a line from start to end
// Calls visitor function for each tile, visitor can stop iteration by returning false
class Bresenham3D {
public:
    // Trace a line and call visitor for each tile
    // visitor(tile_coord, distance_from_start) -> bool (return false to stop)
    using TileVisitor = std::function<bool(const TileCoord&, float)>;

    static void trace_line(
        const TileCoord& start,
        const TileCoord& end,
        TileVisitor visitor
    );

    // Trace line and return all tiles it passes through
    static std::vector<TileCoord> get_line_tiles(
        const TileCoord& start,
        const TileCoord& end
    );

    // Trace line through world checking for occlusion
    static LineTraceResult trace_with_occlusion(
        const TileCoord& start,
        const TileCoord& end,
        const IVisionWorld& world,
        float eye_height_meters
    );

private:
    // Core 3D Bresenham implementation
    static void bresenham_3d_impl(
        int32_t x0, int32_t y0, int32_t z0,
        int32_t x1, int32_t y1, int32_t z1,
        TileVisitor visitor
    );

    // Calculate 3D distance between two tile coordinates
    static float tile_distance(const TileCoord& a, const TileCoord& b);
};

// High-level LOS check between two entities
enum class LOSResult {
    Clear,              // Clear line of sight
    Blocked,            // Fully blocked by solid obstacle
    PartiallyVisible,   // Visible through window/foliage
    TooFar,             // Beyond effective vision range
    OutOfFOV            // Outside field of view cone
};

// Detailed LOS check result
struct DetailedLOSResult {
    LOSResult result = LOSResult::Blocked;
    float visibility_factor = 0.0f;         // 0.0 = invisible, 1.0 = fully visible
    float actual_distance = 0.0f;           // World-space distance
    float effective_range = 0.0f;           // Max range after environmental modifiers
    TileCoord blocking_tile;                // Tile that blocked vision (if blocked)
    math::Vec3 target_position;             // World position of target
};

// Check LOS between two world positions
DetailedLOSResult check_line_of_sight(
    const math::Vec3& from_world_pos,
    const math::Vec3& to_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused = false
);

// Check if target is within viewer's FOV cone
bool is_in_field_of_view(
    const math::Vec3& viewer_pos,
    const math::Vec3& viewer_forward,
    const math::Vec3& target_pos,
    float fov_angle_degrees
);

} // namespace lore::vision