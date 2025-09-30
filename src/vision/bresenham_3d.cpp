#include <lore/vision/bresenham_3d.hpp>
#include <lore/core/log.hpp>
#include <cmath>
#include <algorithm>

namespace lore::vision {

// Calculate 3D Euclidean distance between two tile coordinates
float Bresenham3D::tile_distance(const TileCoord& a, const TileCoord& b) {
    int32_t dx = b.x - a.x;
    int32_t dy = b.y - a.y;
    int32_t dz = b.z - a.z;
    return std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz));
}

// 3D Bresenham line algorithm
// Based on "A Fast Voxel Traversal Algorithm for Ray Tracing" by John Amanatides and Andrew Woo
void Bresenham3D::bresenham_3d_impl(
    int32_t x0, int32_t y0, int32_t z0,
    int32_t x1, int32_t y1, int32_t z1,
    TileVisitor visitor)
{
    int32_t dx = std::abs(x1 - x0);
    int32_t dy = std::abs(y1 - y0);
    int32_t dz = std::abs(z1 - z0);

    int32_t xs = (x1 > x0) ? 1 : -1;
    int32_t ys = (y1 > y0) ? 1 : -1;
    int32_t zs = (z1 > z0) ? 1 : -1;

    int32_t x = x0;
    int32_t y = y0;
    int32_t z = z0;

    TileCoord start{x0, y0, z0};
    TileCoord current{x, y, z};

    // Visit starting tile
    float dist = tile_distance(start, current);
    if (!visitor(current, dist)) {
        return;
    }

    // Driving axis is the axis with the largest delta
    if (dx >= dy && dx >= dz) {
        // X is driving axis
        int32_t p1 = 2 * dy - dx;
        int32_t p2 = 2 * dz - dx;

        while (x != x1) {
            x += xs;
            if (p1 >= 0) {
                y += ys;
                p1 -= 2 * dx;
            }
            if (p2 >= 0) {
                z += zs;
                p2 -= 2 * dx;
            }
            p1 += 2 * dy;
            p2 += 2 * dz;

            current = TileCoord{x, y, z};
            dist = tile_distance(start, current);
            if (!visitor(current, dist)) {
                return;
            }
        }
    } else if (dy >= dx && dy >= dz) {
        // Y is driving axis
        int32_t p1 = 2 * dx - dy;
        int32_t p2 = 2 * dz - dy;

        while (y != y1) {
            y += ys;
            if (p1 >= 0) {
                x += xs;
                p1 -= 2 * dy;
            }
            if (p2 >= 0) {
                z += zs;
                p2 -= 2 * dy;
            }
            p1 += 2 * dx;
            p2 += 2 * dz;

            current = TileCoord{x, y, z};
            dist = tile_distance(start, current);
            if (!visitor(current, dist)) {
                return;
            }
        }
    } else {
        // Z is driving axis
        int32_t p1 = 2 * dy - dz;
        int32_t p2 = 2 * dx - dz;

        while (z != z1) {
            z += zs;
            if (p1 >= 0) {
                y += ys;
                p1 -= 2 * dz;
            }
            if (p2 >= 0) {
                x += xs;
                p2 -= 2 * dz;
            }
            p1 += 2 * dy;
            p2 += 2 * dx;

            current = TileCoord{x, y, z};
            dist = tile_distance(start, current);
            if (!visitor(current, dist)) {
                return;
            }
        }
    }
}

// Public API: Trace line with visitor callback
void Bresenham3D::trace_line(
    const TileCoord& start,
    const TileCoord& end,
    TileVisitor visitor)
{
    bresenham_3d_impl(start.x, start.y, start.z, end.x, end.y, end.z, visitor);
}

// Public API: Get all tiles along line
std::vector<TileCoord> Bresenham3D::get_line_tiles(
    const TileCoord& start,
    const TileCoord& end)
{
    std::vector<TileCoord> tiles;
    tiles.reserve(static_cast<size_t>(tile_distance(start, end)) + 1);

    trace_line(start, end, [&tiles](const TileCoord& tile, float /*distance*/) {
        tiles.push_back(tile);
        return true; // Continue tracing
    });

    return tiles;
}

// Public API: Trace with occlusion checking
LineTraceResult Bresenham3D::trace_with_occlusion(
    const TileCoord& start,
    const TileCoord& end,
    const IVisionWorld& world,
    float eye_height_meters)
{
    LineTraceResult result;
    result.accumulated_transparency = 1.0f;

    trace_line(start, end, [&](const TileCoord& tile, float distance) {
        // Get tile vision data from world
        const TileVisionData* vision_data = world.get_tile_vision_data(tile);

        if (vision_data == nullptr) {
            // Air tile or empty space, continue tracing
            return true;
        }

        // Check if tile blocks sight
        if (vision_data->blocks_sight) {
            // Check height-based occlusion
            float tile_height = vision_data->height_meters;

            if (eye_height_meters < tile_height) {
                // Tile is tall enough to block vision
                result.hit = true;
                result.hit_tile = tile;
                result.distance = distance;
                // Calculate hit point (center of blocking tile)
                result.hit_point = world.tile_to_world(tile);
                return false; // Stop tracing
            }
        }

        // Apply transparency reduction
        result.accumulated_transparency *= vision_data->transparency;

        // If transparency is very low, treat as blocked
        if (result.accumulated_transparency < 0.1f) {
            result.hit = true;
            result.hit_tile = tile;
            result.distance = distance;
            result.hit_point = world.tile_to_world(tile);
            return false; // Stop tracing
        }

        return true; // Continue tracing
    });

    result.distance = tile_distance(start, end);
    return result;
}

// Check if target is within viewer's FOV cone
bool is_in_field_of_view(
    const math::Vec3& viewer_pos,
    const math::Vec3& viewer_forward,
    const math::Vec3& target_pos,
    float fov_angle_degrees)
{
    // Vector from viewer to target
    math::Vec3 to_target = target_pos - viewer_pos;
    float dist = glm::length(to_target);

    if (dist < 0.001f) {
        return true; // Target is at same position
    }

    to_target = to_target / dist; // Normalize

    // Dot product gives cosine of angle between vectors
    float dot = glm::dot(viewer_forward, to_target);

    // Convert FOV angle to half-angle cosine
    float half_fov_rad = (fov_angle_degrees * 0.5f) * (3.14159265359f / 180.0f);
    float cos_half_fov = std::cos(half_fov_rad);

    return dot >= cos_half_fov;
}

// High-level LOS check between world positions
DetailedLOSResult check_line_of_sight(
    const math::Vec3& from_world_pos,
    const math::Vec3& to_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused)
{
    DetailedLOSResult result;
    result.target_position = to_world_pos;

    // Calculate effective vision range
    result.effective_range = calculate_effective_vision_range(viewer_profile, env, is_focused);

    // Calculate actual distance
    math::Vec3 delta = to_world_pos - from_world_pos;
    result.actual_distance = glm::length(delta);

    // Check if target is too far
    if (result.actual_distance > result.effective_range) {
        result.result = LOSResult::TooFar;
        result.visibility_factor = 0.0f;
        return result;
    }

    // Convert world positions to tile coordinates using world interface
    math::Vec3 eye_pos = from_world_pos;
    eye_pos.z += viewer_profile.eye_height_meters;

    TileCoord from_tile = world.world_to_tile(eye_pos);
    TileCoord to_tile = world.world_to_tile(to_world_pos);

    // Perform Bresenham line trace with occlusion
    LineTraceResult trace = Bresenham3D::trace_with_occlusion(
        from_tile,
        to_tile,
        world,
        viewer_profile.eye_height_meters
    );

    // Determine result based on trace
    if (trace.hit) {
        result.blocking_tile = trace.hit_tile;

        if (trace.accumulated_transparency > 0.5f) {
            result.result = LOSResult::PartiallyVisible;
            result.visibility_factor = trace.accumulated_transparency;
        } else {
            result.result = LOSResult::Blocked;
            result.visibility_factor = 0.0f;
        }
    } else {
        result.result = LOSResult::Clear;

        // Calculate visibility factor based on distance and environmental conditions
        float distance_factor = 1.0f - (result.actual_distance / result.effective_range);
        float env_factor = env.get_weather_visibility_modifier();
        result.visibility_factor = distance_factor * env_factor * trace.accumulated_transparency;
    }

    return result;
}

} // namespace lore::vision