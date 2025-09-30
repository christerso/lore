#pragma once

#include <lore/math/math.hpp>
#include <lore/vision/vision_profile.hpp>
#include <lore/vision/vision_world_interface.hpp>
#include <vector>
#include <functional>

namespace lore::vision {

// Result of FOV calculation - set of visible tiles
struct FOVResult {
    std::vector<TileCoord> visible_tiles;  // All visible tiles
    std::vector<float> visibility_factors;  // Visibility factor per tile (0.0-1.0)
};

// Shadow Casting FOV algorithm
// Calculates 360° field of view from a viewer position
// Accounts for occlusion, environmental factors, and entity vision profile
class ShadowCastingFOV {
public:
    // Calculate FOV from world position
    // Returns all visible tiles within effective range
    static FOVResult calculate_fov(
        const math::Vec3& viewer_world_pos,
        const VisionProfile& viewer_profile,
        const EnvironmentalConditions& env,
        const IVisionWorld& world,
        bool is_focused = false
    );

    // Calculate effective vision range based on profile and environment
    static float calculate_effective_range(
        const VisionProfile& profile,
        const EnvironmentalConditions& env,
        bool is_focused
    );

    // Callback for iterating visible tiles
    using TileVisibilityCallback = std::function<void(const TileCoord&, float visibility)>;

    // Calculate FOV with callback (more efficient for large FOV)
    static void calculate_fov_with_callback(
        const math::Vec3& viewer_world_pos,
        const VisionProfile& viewer_profile,
        const EnvironmentalConditions& env,
        const IVisionWorld& world,
        TileVisibilityCallback callback,
        bool is_focused = false
    );

private:
    // Shadow casting for one octant (1/8th of circle)
    // Octant is defined by x/y/z coordinate swapping and sign flipping
    static void cast_octant(
        const TileCoord& origin,
        const IVisionWorld& world,
        float eye_height_meters,
        float max_range_tiles,
        const EnvironmentalConditions& env,
        TileVisibilityCallback callback,
        int32_t dx, int32_t dy  // Direction multipliers (-1 or +1)
    );

    // Recursive shadow casting for one octant row
    struct Shadow {
        float start_slope;
        float end_slope;
    };

    static void cast_row(
        const TileCoord& origin,
        const IVisionWorld& world,
        float eye_height_meters,
        float max_range_tiles,
        const EnvironmentalConditions& env,
        TileVisibilityCallback callback,
        int32_t row,
        float start_slope,
        float end_slope,
        std::vector<Shadow>& shadows,
        int32_t dx, int32_t dy
    );

    // Check if slope is in shadow
    static bool is_in_shadow(const std::vector<Shadow>& shadows, float slope);

    // Add shadow to shadow list
    static void add_shadow(std::vector<Shadow>& shadows, const Shadow& new_shadow);

    // Calculate slope from origin to tile corner
    static float slope(int32_t col, int32_t row, float col_offset, float row_offset);

    // Check if tile blocks light (fully or partially)
    static bool blocks_light(const TileVisionData* data, float eye_height_meters);

    // Calculate visibility factor based on distance, environment, and transparency
    static float calculate_visibility_factor(
        float distance,
        float accumulated_transparency,
        const EnvironmentalConditions& env
    );
};

} // namespace lore::vision