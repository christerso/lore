#include <lore/vision/vision_debug.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace lore::vision {

VisionDebugData VisionDebug::debug_line_of_sight(
    const math::Vec3& from_world_pos,
    const math::Vec3& to_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused)
{
    VisionDebugData data;

    // Perform LOS check
    DetailedLOSResult result = check_line_of_sight(
        from_world_pos,
        to_world_pos,
        viewer_profile,
        env,
        world,
        is_focused
    );

    // Draw line from viewer to target
    math::Vec3 line_color;
    switch (result.result) {
        case LOSResult::Clear:
            line_color = Colors::CLEAR_LOS;
            break;
        case LOSResult::PartiallyVisible:
            line_color = Colors::PARTIAL_LOS;
            break;
        default:
            line_color = Colors::BLOCKED_LOS;
            break;
    }

    DebugLine los_line;
    los_line.start = from_world_pos;
    los_line.end = to_world_pos;
    los_line.color = line_color;
    los_line.thickness = 2.0f;
    data.lines.push_back(los_line);

    // If blocked, draw X at blocking tile
    if (result.result == LOSResult::Blocked || result.result == LOSResult::PartiallyVisible) {
        // Convert blocking tile to world position
        math::Vec3 block_pos = world.tile_to_world(result.blocking_tile);
        float cross_size = 0.3f;

        // X cross at blocking tile
        data.lines.push_back({{block_pos.x - cross_size, block_pos.y - cross_size, block_pos.z},
                              {block_pos.x + cross_size, block_pos.y + cross_size, block_pos.z},
                              Colors::BLOCKED_LOS, 2.0f});
        data.lines.push_back({{block_pos.x + cross_size, block_pos.y - cross_size, block_pos.z},
                              {block_pos.x - cross_size, block_pos.y + cross_size, block_pos.z},
                              Colors::BLOCKED_LOS, 2.0f});

        // Label with blocking tile
        DebugLabel label;
        label.position = block_pos + math::Vec3(0.0f, 0.0f, 0.5f);
        std::stringstream ss;
        ss << "Blocked at (" << result.blocking_tile.x << ", "
           << result.blocking_tile.y << ", " << result.blocking_tile.z << ")";
        label.text = ss.str();
        label.color = Colors::BLOCKED_LOS;
        data.labels.push_back(label);
    }

    // Add label with result
    DebugLabel result_label;
    result_label.position = from_world_pos + math::Vec3(0.0f, 0.0f, 1.0f);
    std::stringstream ss;
    ss << "LOS: ";
    switch (result.result) {
        case LOSResult::Clear: ss << "CLEAR"; break;
        case LOSResult::Blocked: ss << "BLOCKED"; break;
        case LOSResult::PartiallyVisible: ss << "PARTIAL"; break;
        case LOSResult::TooFar: ss << "TOO FAR"; break;
        case LOSResult::OutOfFOV: ss << "OUT OF FOV"; break;
    }
    ss << " (" << std::fixed << std::setprecision(1) << result.visibility_factor * 100.0f << "%)";
    result_label.text = ss.str();
    result_label.color = line_color;
    data.labels.push_back(result_label);

    return data;
}

VisionDebugData VisionDebug::debug_field_of_view(
    const math::Vec3& viewer_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused)
{
    VisionDebugData data;

    // Calculate FOV
    FOVResult fov = ShadowCastingFOV::calculate_fov(
        viewer_world_pos,
        viewer_profile,
        env,
        world,
        is_focused
    );

    // Draw boxes for visible tiles
    for (size_t i = 0; i < fov.visible_tiles.size(); ++i) {
        float visibility = fov.visibility_factors[i];

        // Color based on visibility
        math::Vec3 color = Colors::VISIBLE_TILE;
        if (visibility < 0.5f) {
            color = Colors::PARTIAL_LOS;
        }

        DebugBox box = create_tile_box(fov.visible_tiles[i], world, color, false);
        data.boxes.push_back(box);
    }

    // Add viewer position marker
    DebugLabel viewer_label;
    viewer_label.position = viewer_world_pos + math::Vec3(0.0f, 0.0f, 1.0f);
    std::stringstream ss;
    ss << "Viewer (visible: " << fov.visible_tiles.size() << " tiles)";
    viewer_label.text = ss.str();
    viewer_label.color = Colors::FOV_CONE;
    data.labels.push_back(viewer_label);

    // Add vision range circle
    float effective_range = ShadowCastingFOV::calculate_effective_range(viewer_profile, env, is_focused);
    auto range_lines = create_circle(viewer_world_pos, effective_range, Colors::VISION_RANGE, 32);
    data.lines.insert(data.lines.end(), range_lines.begin(), range_lines.end());

    return data;
}

VisionDebugData VisionDebug::debug_tile_occlusion(
    const TileCoord& tile_coord,
    const IVisionWorld& world,
    float eye_height_meters)
{
    VisionDebugData data;

    const TileVisionData* tile_data = world.get_tile_vision_data(tile_coord);

    // Draw tile box
    math::Vec3 color = tile_data ? Colors::OCCLUDED_TILE : math::Vec3(0.2f, 0.2f, 0.2f);
    if (tile_data && !tile_data->blocks_sight) {
        color = Colors::VISIBLE_TILE;
    }

    DebugBox box = create_tile_box(tile_coord, world, color, tile_data != nullptr);
    data.boxes.push_back(box);

    // Add label with tile info
    if (tile_data) {
        DebugLabel label = create_tile_label(tile_coord, world, "", color);
        std::stringstream ss;
        ss << "Height: " << std::fixed << std::setprecision(1) << tile_data->height_meters << "m\n";
        ss << "Blocks: " << (tile_data->blocks_sight ? "YES" : "NO") << "\n";
        ss << "Transparency: " << std::fixed << std::setprecision(2) << tile_data->transparency << "\n";
        ss << "Eye height: " << std::fixed << std::setprecision(1) << eye_height_meters << "m";
        label.text = ss.str();
        data.labels.push_back(label);

        // Draw height indicator
        math::Vec3 tile_world = world.tile_to_world(tile_coord);
        DebugLine height_line;
        height_line.start = tile_world;
        height_line.end = tile_world + math::Vec3(0.0f, 0.0f, tile_data->height_meters);
        height_line.color = Colors::VISION_RANGE;
        height_line.thickness = 3.0f;
        data.lines.push_back(height_line);
    }

    return data;
}

VisionDebugData VisionDebug::debug_vision_range(
    const math::Vec3& viewer_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    bool is_focused)
{
    VisionDebugData data;

    float effective_range = ShadowCastingFOV::calculate_effective_range(viewer_profile, env, is_focused);

    // Draw range circle
    auto range_lines = create_circle(viewer_world_pos, effective_range, Colors::VISION_RANGE, 48);
    data.lines.insert(data.lines.end(), range_lines.begin(), range_lines.end());

    // Add label
    DebugLabel label;
    label.position = viewer_world_pos + math::Vec3(0.0f, effective_range, 0.5f);
    std::stringstream ss;
    ss << "Range: " << std::fixed << std::setprecision(1) << effective_range << "m";
    label.text = ss.str();
    label.color = Colors::VISION_RANGE;
    data.labels.push_back(label);

    return data;
}

VisionDebugData VisionDebug::debug_fov_cone(
    const math::Vec3& viewer_pos,
    const math::Vec3& viewer_forward,
    float fov_angle_degrees,
    float range_meters)
{
    VisionDebugData data;

    // Normalize forward vector
    math::Vec3 forward = viewer_forward;
    float len = glm::length(forward);
    if (len > 0.001f) {
        forward = forward / len;
    } else {
        forward = math::Vec3(1.0f, 0.0f, 0.0f); // Default forward
    }

    // Calculate cone edges
    float half_angle_rad = (fov_angle_degrees * 0.5f) * (3.14159265f / 180.0f);

    // Rotate forward vector by half angle (left and right)
    // Simplified 2D rotation in XY plane
    float cos_angle = std::cos(half_angle_rad);
    float sin_angle = std::sin(half_angle_rad);

    math::Vec3 left_edge(
        forward.x * cos_angle - forward.y * sin_angle,
        forward.x * sin_angle + forward.y * cos_angle,
        forward.z
    );

    math::Vec3 right_edge(
        forward.x * cos_angle + forward.y * sin_angle,
        -forward.x * sin_angle + forward.y * cos_angle,
        forward.z
    );

    // Draw cone edges
    data.lines.push_back({viewer_pos, viewer_pos + left_edge * range_meters, Colors::FOV_CONE, 2.0f});
    data.lines.push_back({viewer_pos, viewer_pos + right_edge * range_meters, Colors::FOV_CONE, 2.0f});
    data.lines.push_back({viewer_pos, viewer_pos + forward * range_meters, Colors::FOV_CONE, 2.0f});

    // Draw arc at end of cone
    int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float t1 = static_cast<float>(i) / segments;
        float t2 = static_cast<float>(i + 1) / segments;

        float angle1 = -half_angle_rad + t1 * (2.0f * half_angle_rad);
        float angle2 = -half_angle_rad + t2 * (2.0f * half_angle_rad);

        math::Vec3 p1(
            forward.x * std::cos(angle1) - forward.y * std::sin(angle1),
            forward.x * std::sin(angle1) + forward.y * std::cos(angle1),
            forward.z
        );

        math::Vec3 p2(
            forward.x * std::cos(angle2) - forward.y * std::sin(angle2),
            forward.x * std::sin(angle2) + forward.y * std::cos(angle2),
            forward.z
        );

        data.lines.push_back({
            viewer_pos + p1 * range_meters,
            viewer_pos + p2 * range_meters,
            Colors::FOV_CONE,
            1.0f
        });
    }

    return data;
}

VisionDebugData VisionDebug::combine(const std::vector<VisionDebugData>& data_sets) {
    VisionDebugData combined;

    for (const auto& data : data_sets) {
        combined.lines.insert(combined.lines.end(), data.lines.begin(), data.lines.end());
        combined.labels.insert(combined.labels.end(), data.labels.begin(), data.labels.end());
        combined.boxes.insert(combined.boxes.end(), data.boxes.begin(), data.boxes.end());
    }

    return combined;
}

std::string VisionDebug::to_string(const VisionDebugData& data) {
    std::stringstream ss;
    ss << "VisionDebugData:\n";
    ss << "  Lines: " << data.lines.size() << "\n";
    ss << "  Labels: " << data.labels.size() << "\n";
    ss << "  Boxes: " << data.boxes.size() << "\n";

    for (const auto& label : data.labels) {
        ss << "  - " << label.text << " at ("
           << std::fixed << std::setprecision(1)
           << label.position.x << ", " << label.position.y << ", " << label.position.z << ")\n";
    }

    return ss.str();
}

DebugLine VisionDebug::create_tile_line(
    const TileCoord& start,
    const TileCoord& end,
    const IVisionWorld& world,
    const math::Vec3& color,
    float thickness)
{
    DebugLine line;
    line.start = world.tile_to_world(start);
    line.end = world.tile_to_world(end);
    line.color = color;
    line.thickness = thickness;
    return line;
}

DebugBox VisionDebug::create_tile_box(
    const TileCoord& coord,
    const IVisionWorld& world,
    const math::Vec3& color,
    bool filled)
{
    DebugBox box;

    // Get tile center and calculate corners
    math::Vec3 center = world.tile_to_world(coord);
    float half_size = 0.5f; // Assuming 1m tiles

    box.min_corner = center - math::Vec3(half_size, half_size, half_size);
    box.max_corner = center + math::Vec3(half_size, half_size, half_size);
    box.color = color;
    box.filled = filled;

    return box;
}

DebugLabel VisionDebug::create_tile_label(
    const TileCoord& coord,
    const IVisionWorld& world,
    const std::string& text,
    const math::Vec3& color)
{
    DebugLabel label;
    label.position = world.tile_to_world(coord) + math::Vec3(0.0f, 0.0f, 0.5f);
    label.text = text;
    label.color = color;
    return label;
}

std::vector<DebugLine> VisionDebug::create_circle(
    const math::Vec3& center,
    float radius,
    const math::Vec3& color,
    int segments)
{
    std::vector<DebugLine> lines;

    for (int i = 0; i < segments; ++i) {
        float angle1 = (static_cast<float>(i) / segments) * 2.0f * 3.14159265f;
        float angle2 = (static_cast<float>(i + 1) / segments) * 2.0f * 3.14159265f;

        math::Vec3 p1 = center + math::Vec3(std::cos(angle1) * radius, std::sin(angle1) * radius, 0.0f);
        math::Vec3 p2 = center + math::Vec3(std::cos(angle2) * radius, std::sin(angle2) * radius, 0.0f);

        DebugLine line;
        line.start = p1;
        line.end = p2;
        line.color = color;
        line.thickness = 1.0f;
        lines.push_back(line);
    }

    return lines;
}

} // namespace lore::vision