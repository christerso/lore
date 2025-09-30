#include <lore/vision/shadow_casting_fov.hpp>
#include <lore/vision/bresenham_3d.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace lore::vision {

// Hash function for TileCoord (for unordered_set)
struct TileCoordHash {
    size_t operator()(const TileCoord& coord) const {
        size_t h1 = std::hash<int32_t>{}(coord.x);
        size_t h2 = std::hash<int32_t>{}(coord.y);
        size_t h3 = std::hash<int32_t>{}(coord.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

float ShadowCastingFOV::calculate_effective_range(
    const VisionProfile& profile,
    const EnvironmentalConditions& env,
    bool is_focused)
{
    float base_range = profile.base_range_meters;

    // Apply focus bonus
    if (is_focused) {
        base_range *= profile.focus_range_bonus;
    }

    // Apply lighting modifier
    float light_level = env.get_effective_light_level();
    float light_modifier = 1.0f;

    if (light_level < 0.3f) {
        // Dark conditions - reduce range based on night vision
        float darkness_penalty = 1.0f - (1.0f - light_level) * (1.0f - profile.night_vision);
        light_modifier = darkness_penalty;
    }

    // Apply weather modifier
    float weather_modifier = env.get_weather_visibility_modifier();

    // Combine modifiers
    float effective_range = base_range * light_modifier * weather_modifier;

    // Minimum range (always see at least 1 meter)
    return std::max(1.0f, effective_range);
}

FOVResult ShadowCastingFOV::calculate_fov(
    const math::Vec3& viewer_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused)
{
    FOVResult result;

    // Check for blind status
    if (viewer_profile.is_blind) {
        return result; // Empty result
    }

    // Use callback to collect tiles
    std::unordered_set<TileCoord, TileCoordHash> visible_set;
    std::unordered_map<TileCoord, float, TileCoordHash> visibility_map;

    auto callback = [&](const TileCoord& coord, float visibility) {
        if (visible_set.insert(coord).second) {
            // New tile
            result.visible_tiles.push_back(coord);
            result.visibility_factors.push_back(visibility);
            visibility_map[coord] = visibility;
        } else {
            // Already visible - take maximum visibility
            auto it = visibility_map.find(coord);
            if (it != visibility_map.end() && visibility > it->second) {
                it->second = visibility;
                // Update in result vectors
                for (size_t i = 0; i < result.visible_tiles.size(); ++i) {
                    if (result.visible_tiles[i] == coord) {
                        result.visibility_factors[i] = visibility;
                        break;
                    }
                }
            }
        }
    };

    calculate_fov_with_callback(viewer_world_pos, viewer_profile, env, world, callback, is_focused);

    return result;
}

void ShadowCastingFOV::calculate_fov_with_callback(
    const math::Vec3& viewer_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    TileVisibilityCallback callback,
    bool is_focused)
{
    // Check for blind status
    if (viewer_profile.is_blind) {
        return;
    }

    // Convert world position to tile coordinate
    TileCoord origin = world.world_to_tile(viewer_world_pos);

    // Calculate effective range
    float effective_range = calculate_effective_range(viewer_profile, env, is_focused);
    float max_range_tiles = effective_range; // 1 tile = 1 meter

    // Origin is always visible
    callback(origin, 1.0f);

    // Cast shadow for each octant (8 octants = 360° coverage)
    // Octants are defined by dx/dy sign combinations
    int32_t octants[8][2] = {
        { 1,  0}, // East
        { 0,  1}, // North
        {-1,  0}, // West
        { 0, -1}, // South
        { 1,  1}, // Northeast
        {-1,  1}, // Northwest
        {-1, -1}, // Southwest
        { 1, -1}  // Southeast
    };

    for (int i = 0; i < 8; ++i) {
        cast_octant(
            origin,
            world,
            viewer_profile.eye_height_meters,
            max_range_tiles,
            env,
            callback,
            octants[i][0],
            octants[i][1]
        );
    }
}

void ShadowCastingFOV::cast_octant(
    const TileCoord& origin,
    const IVisionWorld& world,
    float eye_height_meters,
    float max_range_tiles,
    const EnvironmentalConditions& env,
    TileVisibilityCallback callback,
    int32_t dx, int32_t dy)
{
    std::vector<Shadow> shadows;

    // Cast each row in octant
    int32_t max_row = static_cast<int32_t>(std::ceil(max_range_tiles));
    for (int32_t row = 1; row <= max_row; ++row) {
        cast_row(origin, world, eye_height_meters, max_range_tiles, env,
                 callback, row, -1.0f, 1.0f, shadows, dx, dy);

        // If entire row is in shadow, no need to continue
        if (shadows.size() == 1 && shadows[0].start_slope == -1.0f && shadows[0].end_slope == 1.0f) {
            break;
        }
    }
}

void ShadowCastingFOV::cast_row(
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
    int32_t dx, int32_t dy)
{
    if (start_slope > end_slope) {
        return; // No visible area
    }

    // Calculate column range for this row
    int32_t min_col = static_cast<int32_t>(std::floor(row * start_slope));
    int32_t max_col = static_cast<int32_t>(std::ceil(row * end_slope));

    float accumulated_transparency = 1.0f;

    for (int32_t col = min_col; col <= max_col; ++col) {
        // Calculate tile coordinate
        TileCoord tile{
            origin.x + col * dx,
            origin.y + row * dy,
            origin.z
        };

        // Check distance
        float distance = std::sqrt(static_cast<float>(col * col + row * row));
        if (distance > max_range_tiles) {
            continue;
        }

        // Calculate slopes for this tile
        float left_slope = slope(col, row, -0.5f, 0.5f);
        float right_slope = slope(col, row, 0.5f, 0.5f);

        // Check if tile is in shadow
        if (is_in_shadow(shadows, left_slope) && is_in_shadow(shadows, right_slope)) {
            continue; // Fully in shadow
        }

        // Get tile vision data
        const TileVisionData* tile_data = world.get_tile_vision_data(tile);

        // Calculate visibility factor
        float visibility = calculate_visibility_factor(distance, accumulated_transparency, env);

        // Tile is visible
        callback(tile, visibility);

        // Check if tile blocks light
        if (tile_data) {
            if (blocks_light(tile_data, eye_height_meters)) {
                // Add shadow
                Shadow new_shadow{left_slope, right_slope};
                add_shadow(shadows, new_shadow);

                // Update accumulated transparency
                accumulated_transparency *= tile_data->transparency;
            }
        }
    }
}

bool ShadowCastingFOV::is_in_shadow(const std::vector<Shadow>& shadows, float slope) {
    for (const Shadow& shadow : shadows) {
        if (slope >= shadow.start_slope && slope <= shadow.end_slope) {
            return true;
        }
    }
    return false;
}

void ShadowCastingFOV::add_shadow(std::vector<Shadow>& shadows, const Shadow& new_shadow) {
    // Find position to insert
    size_t index = 0;
    for (; index < shadows.size(); ++index) {
        if (shadows[index].start_slope >= new_shadow.start_slope) {
            break;
        }
    }

    // Check for overlap with previous shadow
    size_t overlap_start = index;
    if (index > 0 && shadows[index - 1].end_slope >= new_shadow.start_slope) {
        overlap_start = index - 1;
    }

    // Check for overlap with following shadows
    size_t overlap_end = index;
    while (overlap_end < shadows.size() && shadows[overlap_end].start_slope <= new_shadow.end_slope) {
        overlap_end++;
    }

    // Merge overlapping shadows
    if (overlap_start < overlap_end || (index < shadows.size() && shadows[index].start_slope <= new_shadow.end_slope)) {
        float merged_start = new_shadow.start_slope;
        float merged_end = new_shadow.end_slope;

        if (overlap_start < shadows.size()) {
            merged_start = std::min(merged_start, shadows[overlap_start].start_slope);
        }

        if (overlap_end > 0 && overlap_end <= shadows.size()) {
            merged_end = std::max(merged_end, shadows[overlap_end - 1].end_slope);
        }

        // Remove old shadows
        shadows.erase(shadows.begin() + overlap_start, shadows.begin() + overlap_end);

        // Insert merged shadow
        shadows.insert(shadows.begin() + overlap_start, Shadow{merged_start, merged_end});
    } else {
        // No overlap, just insert
        shadows.insert(shadows.begin() + index, new_shadow);
    }
}

float ShadowCastingFOV::slope(int32_t col, int32_t row, float col_offset, float row_offset) {
    if (row == 0) {
        return 0.0f; // Avoid division by zero
    }
    return (static_cast<float>(col) + col_offset) / (static_cast<float>(row) + row_offset);
}

bool ShadowCastingFOV::blocks_light(const TileVisionData* data, float eye_height_meters) {
    if (!data) {
        return false; // Air doesn't block
    }

    // Check height-based occlusion
    if (data->height_meters <= eye_height_meters) {
        return false; // Can see over low obstacles
    }

    // Check if fully opaque
    if (data->blocks_sight) {
        return true;
    }

    // Partially transparent tiles (windows, foliage) still cast some shadow
    return data->transparency < 0.9f;
}

float ShadowCastingFOV::calculate_visibility_factor(
    float distance,
    float accumulated_transparency,
    const EnvironmentalConditions& env)
{
    // Start with full visibility
    float visibility = 1.0f;

    // Apply distance falloff (linear for simplicity)
    // visibility = 1.0 at distance 0, 0.5 at max_range
    float distance_factor = 1.0f - (distance * 0.01f); // Gradual falloff
    distance_factor = std::max(0.1f, distance_factor);
    visibility *= distance_factor;

    // Apply accumulated transparency from passed-through tiles
    visibility *= accumulated_transparency;

    // Apply environmental visibility modifier
    float env_modifier = env.get_weather_visibility_modifier();
    visibility *= env_modifier;

    // Clamp to [0, 1]
    return std::clamp(visibility, 0.0f, 1.0f);
}

} // namespace lore::vision