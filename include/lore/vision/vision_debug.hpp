#pragma once

#include <lore/math/math.hpp>
#include <lore/vision/vision_profile.hpp>
#include <lore/vision/vision_world_interface.hpp>
#include <lore/vision/bresenham_3d.hpp>
#include <lore/vision/shadow_casting_fov.hpp>
#include <vector>
#include <string>

namespace lore::vision {

// Debug visualization data structures

// Line segment for debug rendering
struct DebugLine {
    math::Vec3 start;
    math::Vec3 end;
    math::Vec3 color; // RGB color (0-1)
    float thickness = 1.0f;
};

// Text label for debug rendering
struct DebugLabel {
    math::Vec3 position;
    std::string text;
    math::Vec3 color; // RGB color (0-1)
    float size = 12.0f;
};

// Box for debug rendering
struct DebugBox {
    math::Vec3 min_corner;
    math::Vec3 max_corner;
    math::Vec3 color; // RGB color (0-1)
    bool filled = false;
};

// Complete debug visualization data
struct VisionDebugData {
    std::vector<DebugLine> lines;
    std::vector<DebugLabel> labels;
    std::vector<DebugBox> boxes;
};

// Vision debug renderer - generates debug visualization data
class VisionDebug {
public:
    // Color scheme for debug visualization
    struct Colors {
        static constexpr math::Vec3 CLEAR_LOS = {0.0f, 1.0f, 0.0f};        // Green - clear line of sight
        static constexpr math::Vec3 BLOCKED_LOS = {1.0f, 0.0f, 0.0f};      // Red - blocked
        static constexpr math::Vec3 PARTIAL_LOS = {1.0f, 1.0f, 0.0f};      // Yellow - partial visibility
        static constexpr math::Vec3 VISIBLE_TILE = {0.0f, 0.8f, 0.0f};     // Green - visible tile
        static constexpr math::Vec3 OCCLUDED_TILE = {0.5f, 0.5f, 0.5f};    // Gray - occluded
        static constexpr math::Vec3 FOV_CONE = {0.0f, 0.5f, 1.0f};         // Blue - FOV cone
        static constexpr math::Vec3 VISION_RANGE = {1.0f, 0.5f, 0.0f};     // Orange - vision range
    };

    // Generate debug data for line of sight check
    static VisionDebugData debug_line_of_sight(
        const math::Vec3& from_world_pos,
        const math::Vec3& to_world_pos,
        const VisionProfile& viewer_profile,
        const EnvironmentalConditions& env,
        const IVisionWorld& world,
        bool is_focused = false
    );

    // Generate debug data for FOV calculation
    static VisionDebugData debug_field_of_view(
        const math::Vec3& viewer_world_pos,
        const VisionProfile& viewer_profile,
        const EnvironmentalConditions& env,
        const IVisionWorld& world,
        bool is_focused = false
    );

    // Generate debug data for tile occlusion
    static VisionDebugData debug_tile_occlusion(
        const TileCoord& tile_coord,
        const IVisionWorld& world,
        float eye_height_meters
    );

    // Generate debug data for vision range visualization
    static VisionDebugData debug_vision_range(
        const math::Vec3& viewer_world_pos,
        const VisionProfile& viewer_profile,
        const EnvironmentalConditions& env,
        bool is_focused = false
    );

    // Generate debug data for FOV cone (limited angle)
    static VisionDebugData debug_fov_cone(
        const math::Vec3& viewer_pos,
        const math::Vec3& viewer_forward,
        float fov_angle_degrees,
        float range_meters
    );

    // Combine multiple debug data sets
    static VisionDebugData combine(const std::vector<VisionDebugData>& data_sets);

    // Convert debug data to string description (for console logging)
    static std::string to_string(const VisionDebugData& data);

private:
    // Helper: Create line from tile coordinates
    static DebugLine create_tile_line(
        const TileCoord& start,
        const TileCoord& end,
        const IVisionWorld& world,
        const math::Vec3& color,
        float thickness = 1.0f
    );

    // Helper: Create box for tile
    static DebugBox create_tile_box(
        const TileCoord& coord,
        const IVisionWorld& world,
        const math::Vec3& color,
        bool filled = false
    );

    // Helper: Create label at tile
    static DebugLabel create_tile_label(
        const TileCoord& coord,
        const IVisionWorld& world,
        const std::string& text,
        const math::Vec3& color
    );

    // Helper: Create circle approximation for range
    static std::vector<DebugLine> create_circle(
        const math::Vec3& center,
        float radius,
        const math::Vec3& color,
        int segments = 32
    );
};

} // namespace lore::vision