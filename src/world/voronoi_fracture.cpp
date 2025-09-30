#include <lore/world/voronoi_fracture.hpp>
#include <lore/core/log.hpp>

#include <random>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <chrono>

using namespace lore::core;

namespace lore::world {

// Helper: Hash function for Vec3 (for unordered_set)
struct Vec3Hash {
    std::size_t operator()(const math::Vec3& v) const {
        std::size_t h1 = std::hash<float>{}(v.x);
        std::size_t h2 = std::hash<float>{}(v.y);
        std::size_t h3 = std::hash<float>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct Vec3Equal {
    bool operator()(const math::Vec3& a, const math::Vec3& b) const {
        constexpr float epsilon = 1e-6f;
        return std::abs(a.x - b.x) < epsilon &&
               std::abs(a.y - b.y) < epsilon &&
               std::abs(a.z - b.z) < epsilon;
    }
};

// Poisson disk sampling using Bridson's algorithm
std::vector<math::Vec3> VoronoiFracture::generate_poisson_samples(
    const math::Vec3& aabb_min,
    const math::Vec3& aabb_max,
    float min_distance,
    uint32_t max_points,
    uint32_t seed)
{
    // Use time-based seed if not provided
    if (seed == 0) {
        seed = static_cast<uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        );
    }

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_x(aabb_min.x, aabb_max.x);
    std::uniform_real_distribution<float> dist_y(aabb_min.y, aabb_max.y);
    std::uniform_real_distribution<float> dist_z(aabb_min.z, aabb_max.z);
    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> dist_radius(min_distance, 2.0f * min_distance);

    std::vector<math::Vec3> points;
    std::vector<math::Vec3> active_list;

    // Start with random point
    math::Vec3 first_point{dist_x(rng), dist_y(rng), dist_z(rng)};
    points.push_back(first_point);
    active_list.push_back(first_point);

    const uint32_t k = 30; // Number of attempts per point

    while (!active_list.empty() && points.size() < max_points) {
        // Pick random point from active list
        std::uniform_int_distribution<size_t> active_dist(0, active_list.size() - 1);
        size_t active_idx = active_dist(rng);
        math::Vec3 center = active_list[active_idx];

        bool found_valid = false;

        // Try k times to place point around center
        for (uint32_t attempt = 0; attempt < k; ++attempt) {
            // Generate point in spherical shell around center
            float theta = dist_angle(rng);
            float phi = std::acos(2.0f * dist_angle(rng) / (2.0f * 3.14159265f) - 1.0f);
            float radius = dist_radius(rng);

            math::Vec3 candidate;
            candidate.x = center.x + radius * std::sin(phi) * std::cos(theta);
            candidate.y = center.y + radius * std::sin(phi) * std::sin(theta);
            candidate.z = center.z + radius * std::cos(phi);

            // Check if candidate is within bounds
            if (candidate.x < aabb_min.x || candidate.x > aabb_max.x ||
                candidate.y < aabb_min.y || candidate.y > aabb_max.y ||
                candidate.z < aabb_min.z || candidate.z > aabb_max.z) {
                continue;
            }

            // Check minimum distance to existing points
            bool too_close = false;
            for (const auto& p : points) {
                float dx = candidate.x - p.x;
                float dy = candidate.y - p.y;
                float dz = candidate.z - p.z;
                float dist_sq = dx*dx + dy*dy + dz*dz;
                if (dist_sq < min_distance * min_distance) {
                    too_close = true;
                    break;
                }
            }

            if (!too_close) {
                points.push_back(candidate);
                active_list.push_back(candidate);
                found_valid = true;
                break;
            }
        }

        // Remove from active list if no valid point found
        if (!found_valid) {
            active_list.erase(active_list.begin() + active_idx);
        }
    }

    LOG_DEBUG(Game, "Poisson sampling generated {} points (requested: {})",
             points.size(), max_points);

    return points;
}

// Generate stress-guided Poisson samples for directional fracture
std::vector<math::Vec3> VoronoiFracture::generate_stress_guided_samples(
    const math::Vec3& aabb_min,
    const math::Vec3& aabb_max,
    float base_min_distance,
    uint32_t max_points,
    const ImpactData& impact,
    const MaterialFractureParams& material,
    uint32_t seed)
{
    // Use time-based seed if not provided
    if (seed == 0) {
        seed = static_cast<uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        );
    }

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_01(0.0f, 1.0f);

    // Calculate mesh center for reference
    math::Vec3 mesh_center = {
        (aabb_min.x + aabb_max.x) * 0.5f,
        (aabb_min.y + aabb_max.y) * 0.5f,
        (aabb_min.z + aabb_max.z) * 0.5f
    };

    // Convert impact position to local space if needed
    math::Vec3 impact_point = impact.position;

    // Calculate maximum distance from impact point to mesh bounds
    math::Vec3 corner_distances[8] = {
        {aabb_min.x - impact_point.x, aabb_min.y - impact_point.y, aabb_min.z - impact_point.z},
        {aabb_max.x - impact_point.x, aabb_min.y - impact_point.y, aabb_min.z - impact_point.z},
        {aabb_max.x - impact_point.x, aabb_max.y - impact_point.y, aabb_min.z - impact_point.z},
        {aabb_min.x - impact_point.x, aabb_max.y - impact_point.y, aabb_min.z - impact_point.z},
        {aabb_min.x - impact_point.x, aabb_min.y - impact_point.y, aabb_max.z - impact_point.z},
        {aabb_max.x - impact_point.x, aabb_min.y - impact_point.y, aabb_max.z - impact_point.z},
        {aabb_max.x - impact_point.x, aabb_max.y - impact_point.y, aabb_max.z - impact_point.z},
        {aabb_min.x - impact_point.x, aabb_max.y - impact_point.y, aabb_max.z - impact_point.z}
    };

    float max_impact_distance = 0.0f;
    for (const auto& d : corner_distances) {
        float dist = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
        max_impact_distance = std::max(max_impact_distance, dist);
    }

    std::vector<math::Vec3> points;
    std::vector<math::Vec3> active_list;

    // Lambda: Calculate distance-based minimum spacing
    auto get_min_distance_at_point = [&](const math::Vec3& point) -> float {
        // Distance from impact point
        float dx = point.x - impact_point.x;
        float dy = point.y - impact_point.y;
        float dz = point.z - impact_point.z;
        float dist_from_impact = std::sqrt(dx*dx + dy*dy + dz*dz);

        // Normalized distance (0 at impact, 1 at farthest corner)
        float normalized_dist = std::min(1.0f, dist_from_impact / std::max(0.01f, max_impact_distance));

        // Fragment size gradient based on impact type
        float size_gradient = 1.0f;

        switch (impact.type) {
            case ImpactType::PointImpact:
                // Small cone near impact, exponential falloff
                size_gradient = 0.3f + 0.7f * (normalized_dist * normalized_dist);
                break;

            case ImpactType::BluntForce:
                // Medium depression with radial pattern
                size_gradient = 0.5f + 0.5f * normalized_dist;
                break;

            case ImpactType::Explosion:
                // More uniform, slightly smaller near center
                size_gradient = 0.7f + 0.3f * normalized_dist;
                break;

            case ImpactType::Cutting:
                // Linear gradient along impact direction
                {
                    float alignment = (dx * impact.direction.x +
                                     dy * impact.direction.y +
                                     dz * impact.direction.z) / std::max(0.01f, dist_from_impact);
                    size_gradient = 0.4f + 0.6f * std::abs(alignment);
                }
                break;

            case ImpactType::Crushing:
                // Vertical compression, horizontal spread
                size_gradient = 0.6f + 0.4f * (std::abs(point.y - impact_point.y) /
                                              std::max(0.01f, max_impact_distance));
                break;

            case ImpactType::Shearing:
                // Diagonal pattern
                size_gradient = 0.5f + 0.5f * std::abs(normalized_dist - 0.5f) * 2.0f;
                break;
        }

        // Apply material-specific variance
        size_gradient *= (1.0f + material.fragment_size_variance * (dist_01(rng) - 0.5f) * 0.5f);

        // Clamp and return
        return base_min_distance * std::max(0.2f, std::min(2.0f, size_gradient));
    };

    // Start with point near impact location
    math::Vec3 first_point = impact_point;

    // Clamp to AABB
    first_point.x = std::max(aabb_min.x, std::min(aabb_max.x, first_point.x));
    first_point.y = std::max(aabb_min.y, std::min(aabb_max.y, first_point.y));
    first_point.z = std::max(aabb_min.z, std::min(aabb_max.z, first_point.z));

    points.push_back(first_point);
    active_list.push_back(first_point);

    const uint32_t k = 30; // Number of attempts per point

    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * 3.14159265f);

    while (!active_list.empty() && points.size() < max_points) {
        // Pick random point from active list
        std::uniform_int_distribution<size_t> active_dist(0, active_list.size() - 1);
        size_t active_idx = active_dist(rng);
        math::Vec3 center = active_list[active_idx];

        // Get minimum distance at this center point
        float center_min_dist = get_min_distance_at_point(center);

        bool found_valid = false;

        // Try k times to place point around center
        for (uint32_t attempt = 0; attempt < k; ++attempt) {
            // Generate point in spherical shell
            float theta = dist_angle(rng);
            float phi = std::acos(2.0f * dist_01(rng) - 1.0f);
            float radius = center_min_dist + dist_01(rng) * center_min_dist;

            // Apply directional bias for anisotropic materials (wood grain)
            math::Vec3 sample_dir = {
                std::sin(phi) * std::cos(theta),
                std::sin(phi) * std::sin(theta),
                std::cos(phi)
            };

            // Bias along grain direction for wood
            if (material.grain_direction.x != 0.0f ||
                material.grain_direction.y != 0.0f ||
                material.grain_direction.z != 0.0f)
            {
                // Dot product with grain direction
                float grain_alignment = sample_dir.x * material.grain_direction.x +
                                       sample_dir.y * material.grain_direction.y +
                                       sample_dir.z * material.grain_direction.z;

                // Stretch radius along grain
                radius *= (1.0f + 0.5f * std::abs(grain_alignment));
            }

            math::Vec3 candidate;
            candidate.x = center.x + radius * sample_dir.x;
            candidate.y = center.y + radius * sample_dir.y;
            candidate.z = center.z + radius * sample_dir.z;

            // Check if candidate is within bounds
            if (candidate.x < aabb_min.x || candidate.x > aabb_max.x ||
                candidate.y < aabb_min.y || candidate.y > aabb_max.y ||
                candidate.z < aabb_min.z || candidate.z > aabb_max.z) {
                continue;
            }

            // Get minimum distance at candidate position
            float candidate_min_dist = get_min_distance_at_point(candidate);

            // Check minimum distance to existing points
            bool too_close = false;
            for (const auto& p : points) {
                float dx = candidate.x - p.x;
                float dy = candidate.y - p.y;
                float dz = candidate.z - p.z;
                float dist_sq = dx*dx + dy*dy + dz*dz;

                // Use average of candidate and existing point min distances
                float required_min_dist = get_min_distance_at_point(p);
                float avg_min_dist = (candidate_min_dist + required_min_dist) * 0.5f;

                if (dist_sq < avg_min_dist * avg_min_dist) {
                    too_close = true;
                    break;
                }
            }

            if (!too_close) {
                points.push_back(candidate);
                active_list.push_back(candidate);
                found_valid = true;
                break;
            }
        }

        // Remove from active list if no valid point found
        if (!found_valid) {
            active_list.erase(active_list.begin() + active_idx);
        }
    }

    LOG_DEBUG(Game, "Stress-guided sampling generated {} points (requested: {}, impact type: {})",
             points.size(), max_points, static_cast<int>(impact.type));

    return points;
}

// Compute 3D Voronoi cells (simplified bounded Voronoi)
std::vector<std::vector<math::Vec3>> VoronoiFracture::compute_voronoi_cells(
    const std::vector<math::Vec3>& sample_points,
    const math::Vec3& aabb_min,
    const math::Vec3& aabb_max)
{
    std::vector<std::vector<math::Vec3>> cells;
    cells.reserve(sample_points.size());

    // For each sample point, create a bounded Voronoi cell
    for (size_t i = 0; i < sample_points.size(); ++i) {
        const math::Vec3& seed = sample_points[i];

        // Start with bounding box as initial cell
        std::vector<math::Vec3> cell_vertices = {
            {aabb_min.x, aabb_min.y, aabb_min.z},
            {aabb_max.x, aabb_min.y, aabb_min.z},
            {aabb_max.x, aabb_max.y, aabb_min.z},
            {aabb_min.x, aabb_max.y, aabb_min.z},
            {aabb_min.x, aabb_min.y, aabb_max.z},
            {aabb_max.x, aabb_min.y, aabb_max.z},
            {aabb_max.x, aabb_max.y, aabb_max.z},
            {aabb_min.x, aabb_max.y, aabb_max.z}
        };

        // Clip cell by half-spaces defined by other sample points
        // For each other point, create a clipping plane at the midpoint
        for (size_t j = 0; j < sample_points.size(); ++j) {
            if (i == j) continue;

            const math::Vec3& other = sample_points[j];

            // Midpoint between seed and other
            math::Vec3 midpoint = {
                (seed.x + other.x) * 0.5f,
                (seed.y + other.y) * 0.5f,
                (seed.z + other.z) * 0.5f
            };

            // Plane normal points from other to seed
            math::Vec3 normal = {
                seed.x - other.x,
                seed.y - other.y,
                seed.z - other.z
            };

            // Normalize
            float length = std::sqrt(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
            if (length > 1e-6f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }

            // Clip cell vertices by this half-space
            // Keep vertices on the seed side of the plane
            std::vector<math::Vec3> clipped_vertices;
            for (const auto& v : cell_vertices) {
                // Distance from vertex to plane
                float dist = (v.x - midpoint.x) * normal.x +
                            (v.y - midpoint.y) * normal.y +
                            (v.z - midpoint.z) * normal.z;

                if (dist >= -1e-6f) { // On seed side
                    clipped_vertices.push_back(v);
                }
            }

            cell_vertices = std::move(clipped_vertices);

            if (cell_vertices.empty()) {
                break; // Cell completely clipped away
            }
        }

        cells.push_back(std::move(cell_vertices));
    }

    return cells;
}

// Calculate AABB
void VoronoiFracture::calculate_aabb(
    const std::vector<math::Vec3>& vertices,
    math::Vec3& out_min,
    math::Vec3& out_max)
{
    if (vertices.empty()) {
        out_min = out_max = math::Vec3{0, 0, 0};
        return;
    }

    out_min = vertices[0];
    out_max = vertices[0];

    for (const auto& v : vertices) {
        out_min.x = std::min(out_min.x, v.x);
        out_min.y = std::min(out_min.y, v.y);
        out_min.z = std::min(out_min.z, v.z);

        out_max.x = std::max(out_max.x, v.x);
        out_max.y = std::max(out_max.y, v.y);
        out_max.z = std::max(out_max.z, v.z);
    }
}

// Clip Voronoi cell against mesh (simplified - uses Voronoi cell as-is for now)
VoronoiFracture::ClippedGeometry VoronoiFracture::clip_cell_against_mesh(
    const std::vector<math::Vec3>& voronoi_cell,
    [[maybe_unused]] const std::vector<math::Vec3>& mesh_vertices,
    [[maybe_unused]] const std::vector<uint32_t>& mesh_indices)
{
    ClippedGeometry result;

    if (voronoi_cell.size() < 4) {
        return result; // Need at least 4 vertices for a 3D shape
    }

    // Simplified implementation: Use convex hull of Voronoi cell vertices
    // Full implementation would intersect with mesh triangles

    result.vertices = voronoi_cell;

    // Generate triangulated indices (simple fan triangulation from first vertex)
    if (voronoi_cell.size() >= 4) {
        // Create tetrahedron from first 4 points
        result.indices = {0, 1, 2,  0, 2, 3,  0, 3, 1,  1, 3, 2};

        // Add more faces if more vertices exist (simplified)
        for (size_t i = 4; i < voronoi_cell.size(); ++i) {
            result.indices.push_back(0);
            result.indices.push_back(static_cast<uint32_t>(i - 1));
            result.indices.push_back(static_cast<uint32_t>(i));
        }
    }

    return result;
}

// Create material-specific fracture parameters
MaterialFractureParams VoronoiFracture::create_material_params(
    MaterialPreset preset,
    const math::Vec3& grain_direction)
{
    MaterialFractureParams params;

    switch (preset) {
        case MaterialPreset::Stone:
            params.min_fragments = 5;
            params.max_fragments = 8;
            params.fragment_size_variance = 0.6f;  // Moderate variance
            params.angular_bias = 0.8f;            // Very angular chunks
            params.grain_direction = {0, 0, 0};    // Isotropic
            params.brittleness = 0.9f;             // Very brittle
            LOG_DEBUG(Game, "Created Stone material params: 5-8 angular chunks");
            break;

        case MaterialPreset::Wood:
            params.min_fragments = 15;
            params.max_fragments = 25;
            params.fragment_size_variance = 0.8f;  // High variance (splinters)
            params.angular_bias = 0.3f;            // Some angular features
            params.grain_direction = grain_direction.x == 0.0f && grain_direction.y == 0.0f && grain_direction.z == 0.0f
                                    ? math::Vec3{0, 1, 0}  // Default vertical grain
                                    : grain_direction;
            params.brittleness = 0.5f;             // Medium brittleness
            LOG_DEBUG(Game, "Created Wood material params: 15-25 splinters along grain [{:.2f}, {:.2f}, {:.2f}]",
                     params.grain_direction.x, params.grain_direction.y, params.grain_direction.z);
            break;

        case MaterialPreset::Glass:
            params.min_fragments = 20;
            params.max_fragments = 40;
            params.fragment_size_variance = 0.9f;  // Very high variance
            params.angular_bias = 0.9f;            // Very sharp, angular
            params.grain_direction = {0, 0, 0};    // Isotropic
            params.brittleness = 1.0f;             // Maximum brittleness
            LOG_DEBUG(Game, "Created Glass material params: 20-40 sharp pieces");
            break;

        case MaterialPreset::Metal:
            params.min_fragments = 8;
            params.max_fragments = 15;
            params.fragment_size_variance = 0.4f;  // Lower variance
            params.angular_bias = 0.2f;            // Smooth, bent edges
            params.grain_direction = {0, 0, 0};    // Isotropic
            params.brittleness = 0.1f;             // Very ductile
            LOG_DEBUG(Game, "Created Metal material params: 8-15 bent pieces");
            break;

        case MaterialPreset::Concrete:
            params.min_fragments = 10;
            params.max_fragments = 20;
            params.fragment_size_variance = 0.7f;  // High variance
            params.angular_bias = 0.7f;            // Quite angular
            params.grain_direction = {0, 0, 0};    // Isotropic
            params.brittleness = 0.7f;             // Fairly brittle
            LOG_DEBUG(Game, "Created Concrete material params: 10-20 irregular chunks");
            break;

        case MaterialPreset::Custom:
        default:
            // Return defaults
            LOG_DEBUG(Game, "Created Custom material params: using defaults");
            break;
    }

    return params;
}

// Calculate physics properties
void VoronoiFracture::calculate_physics_properties(DebrisPiece& piece) {
    if (piece.vertices.empty()) {
        piece.centroid = {0, 0, 0};
        piece.mass = 0.0f;
        piece.inertia_tensor = {0, 0, 0};
        return;
    }

    // Calculate centroid
    math::Vec3 centroid{0, 0, 0};
    for (const auto& v : piece.vertices) {
        centroid.x += v.x;
        centroid.y += v.y;
        centroid.z += v.z;
    }
    float inv_count = 1.0f / static_cast<float>(piece.vertices.size());
    centroid.x *= inv_count;
    centroid.y *= inv_count;
    centroid.z *= inv_count;
    piece.centroid = centroid;

    // Estimate mass from volume (assuming unit density)
    // Simplified: mass proportional to AABB volume
    math::Vec3 aabb_min, aabb_max;
    calculate_aabb(piece.vertices, aabb_min, aabb_max);

    float dx = aabb_max.x - aabb_min.x;
    float dy = aabb_max.y - aabb_min.y;
    float dz = aabb_max.z - aabb_min.z;
    float volume = dx * dy * dz;
    piece.mass = std::max(0.1f, volume * 1000.0f); // kg (assume stone density ~1000 kg/m³)

    // Calculate diagonal inertia tensor (simplified box approximation)
    float m = piece.mass;
    float Ixx = (m / 12.0f) * (dy*dy + dz*dz);
    float Iyy = (m / 12.0f) * (dx*dx + dz*dz);
    float Izz = (m / 12.0f) * (dx*dx + dy*dy);
    piece.inertia_tensor = {Ixx, Iyy, Izz};

    // Set AABB
    piece.aabb_min = aabb_min;
    piece.aabb_max = aabb_max;

    LOG_DEBUG(Game, "Debris piece: mass={:.2f}kg, volume={:.4f}m³, vertices={}",
             piece.mass, volume, piece.vertices.size());
}

// Transfer UVs from original mesh
void VoronoiFracture::transfer_uvs(
    DebrisPiece& piece,
    [[maybe_unused]] const std::vector<math::Vec3>& original_vertices,
    [[maybe_unused]] const std::vector<uint32_t>& original_indices,
    [[maybe_unused]] const std::vector<math::Vec2>& original_uvs)
{
    piece.uvs.clear();
    piece.uvs.reserve(piece.vertices.size());

    // Simplified: Use planar projection based on dominant axis
    math::Vec3 aabb_size = {
        piece.aabb_max.x - piece.aabb_min.x,
        piece.aabb_max.y - piece.aabb_min.y,
        piece.aabb_max.z - piece.aabb_min.z
    };

    // Find dominant axis
    int dominant_axis = 0;
    if (aabb_size.y > aabb_size.x && aabb_size.y > aabb_size.z) dominant_axis = 1;
    else if (aabb_size.z > aabb_size.x && aabb_size.z > aabb_size.y) dominant_axis = 2;

    for (const auto& v : piece.vertices) {
        math::Vec2 uv;
        if (dominant_axis == 0) {
            // Project onto YZ plane
            uv.x = (v.y - piece.aabb_min.y) / std::max(0.001f, aabb_size.y);
            uv.y = (v.z - piece.aabb_min.z) / std::max(0.001f, aabb_size.z);
        } else if (dominant_axis == 1) {
            // Project onto XZ plane
            uv.x = (v.x - piece.aabb_min.x) / std::max(0.001f, aabb_size.x);
            uv.y = (v.z - piece.aabb_min.z) / std::max(0.001f, aabb_size.z);
        } else {
            // Project onto XY plane
            uv.x = (v.x - piece.aabb_min.x) / std::max(0.001f, aabb_size.x);
            uv.y = (v.y - piece.aabb_min.y) / std::max(0.001f, aabb_size.y);
        }
        piece.uvs.push_back(uv);
    }
}

// Check if point is inside mesh (ray casting)
bool VoronoiFracture::is_point_inside_mesh(
    const math::Vec3& point,
    const std::vector<math::Vec3>& mesh_vertices,
    const std::vector<uint32_t>& mesh_indices)
{
    // Cast ray from point in +X direction
    // Count intersections with mesh triangles
    // Odd count = inside, even count = outside

    int intersection_count = 0;
    math::Vec3 ray_dir{1.0f, 0.0f, 0.0f};

    for (size_t i = 0; i < mesh_indices.size(); i += 3) {
        const math::Vec3& v0 = mesh_vertices[mesh_indices[i]];
        const math::Vec3& v1 = mesh_vertices[mesh_indices[i + 1]];
        const math::Vec3& v2 = mesh_vertices[mesh_indices[i + 2]];

        // Möller-Trumbore ray-triangle intersection
        math::Vec3 edge1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        math::Vec3 edge2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

        // Cross product: ray_dir × edge2
        math::Vec3 h{
            ray_dir.y * edge2.z - ray_dir.z * edge2.y,
            ray_dir.z * edge2.x - ray_dir.x * edge2.z,
            ray_dir.x * edge2.y - ray_dir.y * edge2.x
        };

        // Dot product: edge1 · h
        float a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
        if (std::abs(a) < 1e-6f) continue; // Ray parallel to triangle

        float f = 1.0f / a;
        math::Vec3 s{point.x - v0.x, point.y - v0.y, point.z - v0.z};
        float u = f * (s.x * h.x + s.y * h.y + s.z * h.z);
        if (u < 0.0f || u > 1.0f) continue;

        // Cross product: s × edge1
        math::Vec3 q{
            s.y * edge1.z - s.z * edge1.y,
            s.z * edge1.x - s.x * edge1.z,
            s.x * edge1.y - s.y * edge1.x
        };

        float v = f * (ray_dir.x * q.x + ray_dir.y * q.y + ray_dir.z * q.z);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);
        if (t > 1e-6f) { // Intersection ahead of ray origin
            intersection_count++;
        }
    }

    return (intersection_count % 2) == 1; // Odd = inside
}

// Generate voxel approximation
void VoronoiFracture::generate_voxel_approximation(DebrisPiece& piece) {
    constexpr int VOXEL_GRID_SIZE = 4; // 4×4×4 = 64 voxels

    piece.voxel_approximation.clear();
    piece.voxel_approximation.reserve(VOXEL_GRID_SIZE * VOXEL_GRID_SIZE * VOXEL_GRID_SIZE);

    math::Vec3 voxel_size = {
        (piece.aabb_max.x - piece.aabb_min.x) / VOXEL_GRID_SIZE,
        (piece.aabb_max.y - piece.aabb_min.y) / VOXEL_GRID_SIZE,
        (piece.aabb_max.z - piece.aabb_min.z) / VOXEL_GRID_SIZE
    };

    for (int z = 0; z < VOXEL_GRID_SIZE; ++z) {
        for (int y = 0; y < VOXEL_GRID_SIZE; ++y) {
            for (int x = 0; x < VOXEL_GRID_SIZE; ++x) {
                VoxelCell voxel;
                voxel.position = {
                    piece.aabb_min.x + (x + 0.5f) * voxel_size.x,
                    piece.aabb_min.y + (y + 0.5f) * voxel_size.y,
                    piece.aabb_min.z + (z + 0.5f) * voxel_size.z
                };

                // Check if voxel center is inside debris geometry
                voxel.is_occupied = is_point_inside_mesh(voxel.position, piece.vertices, piece.indices);

                piece.voxel_approximation.push_back(voxel);
            }
        }
    }

    int occupied_count = static_cast<int>(std::count_if(
        piece.voxel_approximation.begin(),
        piece.voxel_approximation.end(),
        [](const VoxelCell& v) { return v.is_occupied; }
    ));

    LOG_DEBUG(Game, "Voxel approximation: {}/{} occupied",
             occupied_count, piece.voxel_approximation.size());
}

// Main fracture function
std::vector<DebrisPiece> VoronoiFracture::fracture_mesh(
    const std::vector<math::Vec3>& vertices,
    const std::vector<uint32_t>& indices,
    [[maybe_unused]] const std::vector<math::Vec3>& normals,
    const std::vector<math::Vec2>& uvs,
    const VoronoiFractureConfig& config)
{
    LOG_INFO(Game, "Starting Voronoi fracture: {} vertices, {} triangles, {} fragments",
            vertices.size(), indices.size() / 3, config.num_fragments);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Calculate mesh AABB
    math::Vec3 aabb_min, aabb_max;
    calculate_aabb(vertices, aabb_min, aabb_max);

    // Generate sample points (stress-guided if impact provided, uniform otherwise)
    std::vector<math::Vec3> sample_points;

    if (config.impact != nullptr) {
        // Use stress-guided sampling for directional fracture
        sample_points = generate_stress_guided_samples(
            aabb_min,
            aabb_max,
            config.poisson_min_distance,
            config.num_fragments,
            *config.impact,
            config.material_params,
            config.random_seed
        );

        LOG_INFO(Game, "Using directional fracture (impact type: {}, force: {:.1f}N)",
                static_cast<int>(config.impact->type), config.impact->force);
    } else {
        // Use uniform Poisson disk sampling
        sample_points = generate_poisson_samples(
            aabb_min,
            aabb_max,
            config.poisson_min_distance,
            config.num_fragments,
            config.random_seed
        );
    }

    if (sample_points.empty()) {
        LOG_ERROR(Game, "Failed to generate sample points");
        return {};
    }

    // Compute Voronoi cells
    std::vector<std::vector<math::Vec3>> voronoi_cells = compute_voronoi_cells(
        sample_points,
        aabb_min,
        aabb_max
    );

    // Create debris pieces
    std::vector<DebrisPiece> debris_pieces;
    debris_pieces.reserve(voronoi_cells.size());

    for (size_t i = 0; i < voronoi_cells.size(); ++i) {
        if (voronoi_cells[i].empty()) continue;

        DebrisPiece piece;

        // Clip Voronoi cell against mesh
        ClippedGeometry clipped = clip_cell_against_mesh(
            voronoi_cells[i],
            vertices,
            indices
        );

        if (clipped.vertices.empty()) continue;

        piece.vertices = std::move(clipped.vertices);
        piece.indices = std::move(clipped.indices);

        // Generate normals (simplified - per-face normals)
        piece.normals.resize(piece.vertices.size(), math::Vec3{0, 1, 0});

        // Transfer UVs
        transfer_uvs(piece, vertices, indices, uvs);

        // Calculate physics properties
        calculate_physics_properties(piece);

        // Initialize physics state
        piece.position = piece.centroid;
        piece.rotation = {0, 0, 0, 1}; // Identity quaternion
        piece.velocity = {0, 0, 0};
        piece.angular_velocity = {0, 0, 0};

        // Apply initial velocity based on impact (if provided)
        if (config.impact != nullptr) {
            const ImpactData& impact = *config.impact;

            // Calculate vector from impact point to piece centroid
            math::Vec3 to_piece = {
                piece.centroid.x - impact.position.x,
                piece.centroid.y - impact.position.y,
                piece.centroid.z - impact.position.z
            };

            float distance_from_impact = std::sqrt(
                to_piece.x * to_piece.x +
                to_piece.y * to_piece.y +
                to_piece.z * to_piece.z
            );

            if (distance_from_impact > 1e-6f) {
                // Normalize to_piece vector
                to_piece.x /= distance_from_impact;
                to_piece.y /= distance_from_impact;
                to_piece.z /= distance_from_impact;

                // Base impulse magnitude (inversely proportional to mass and distance)
                float impulse_magnitude = impact.force * impact.impulse_duration;
                float velocity_magnitude = (impulse_magnitude / piece.mass) /
                                          std::max(0.5f, distance_from_impact);

                // Impact-type specific velocity direction
                math::Vec3 velocity_dir = to_piece;

                switch (impact.type) {
                    case ImpactType::PointImpact:
                        // Cone-shaped ejection along impact direction
                        {
                            float alignment = to_piece.x * impact.direction.x +
                                            to_piece.y * impact.direction.y +
                                            to_piece.z * impact.direction.z;
                            if (alignment > 0.0f) {
                                // Blend between radial and impact direction
                                velocity_dir.x = 0.7f * impact.direction.x + 0.3f * to_piece.x;
                                velocity_dir.y = 0.7f * impact.direction.y + 0.3f * to_piece.y;
                                velocity_dir.z = 0.7f * impact.direction.z + 0.3f * to_piece.z;

                                // Normalize
                                float len = std::sqrt(velocity_dir.x * velocity_dir.x +
                                                     velocity_dir.y * velocity_dir.y +
                                                     velocity_dir.z * velocity_dir.z);
                                if (len > 1e-6f) {
                                    velocity_dir.x /= len;
                                    velocity_dir.y /= len;
                                    velocity_dir.z /= len;
                                }
                            }
                        }
                        break;

                    case ImpactType::Explosion:
                        // Pure radial ejection
                        velocity_dir = to_piece;
                        velocity_magnitude *= 1.5f; // Explosions are more energetic
                        break;

                    case ImpactType::BluntForce:
                        // Mix of impact direction and radial
                        velocity_dir.x = 0.5f * impact.direction.x + 0.5f * to_piece.x;
                        velocity_dir.y = 0.5f * impact.direction.y + 0.5f * to_piece.y;
                        velocity_dir.z = 0.5f * impact.direction.z + 0.5f * to_piece.z;
                        {
                            float len = std::sqrt(velocity_dir.x * velocity_dir.x +
                                                 velocity_dir.y * velocity_dir.y +
                                                 velocity_dir.z * velocity_dir.z);
                            if (len > 1e-6f) {
                                velocity_dir.x /= len;
                                velocity_dir.y /= len;
                                velocity_dir.z /= len;
                            }
                        }
                        break;

                    case ImpactType::Cutting:
                        // Perpendicular to cut direction
                        {
                            // Cross product: impact.direction × to_piece
                            math::Vec3 perp = {
                                impact.direction.y * to_piece.z - impact.direction.z * to_piece.y,
                                impact.direction.z * to_piece.x - impact.direction.x * to_piece.z,
                                impact.direction.x * to_piece.y - impact.direction.y * to_piece.x
                            };

                            float len = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
                            if (len > 1e-6f) {
                                velocity_dir = perp;
                                velocity_dir.x /= len;
                                velocity_dir.y /= len;
                                velocity_dir.z /= len;
                            }
                        }
                        velocity_magnitude *= 0.7f; // Cuts have less explosive force
                        break;

                    case ImpactType::Crushing:
                        // Horizontal spread
                        velocity_dir.y *= 0.3f; // Reduce vertical component
                        {
                            float len = std::sqrt(velocity_dir.x * velocity_dir.x +
                                                 velocity_dir.y * velocity_dir.y +
                                                 velocity_dir.z * velocity_dir.z);
                            if (len > 1e-6f) {
                                velocity_dir.x /= len;
                                velocity_dir.y /= len;
                                velocity_dir.z /= len;
                            }
                        }
                        break;

                    case ImpactType::Shearing:
                        // Perpendicular to impact direction
                        velocity_dir.x = to_piece.x - impact.direction.x *
                                        (to_piece.x * impact.direction.x +
                                         to_piece.y * impact.direction.y +
                                         to_piece.z * impact.direction.z);
                        velocity_dir.y = to_piece.y - impact.direction.y *
                                        (to_piece.x * impact.direction.x +
                                         to_piece.y * impact.direction.y +
                                         to_piece.z * impact.direction.z);
                        velocity_dir.z = to_piece.z - impact.direction.z *
                                        (to_piece.x * impact.direction.x +
                                         to_piece.y * impact.direction.y +
                                         to_piece.z * impact.direction.z);
                        {
                            float len = std::sqrt(velocity_dir.x * velocity_dir.x +
                                                 velocity_dir.y * velocity_dir.y +
                                                 velocity_dir.z * velocity_dir.z);
                            if (len > 1e-6f) {
                                velocity_dir.x /= len;
                                velocity_dir.y /= len;
                                velocity_dir.z /= len;
                            }
                        }
                        break;
                }

                // Apply velocity
                piece.velocity.x = velocity_dir.x * velocity_magnitude;
                piece.velocity.y = velocity_dir.y * velocity_magnitude;
                piece.velocity.z = velocity_dir.z * velocity_magnitude;

                // Apply angular velocity (tumbling)
                // Randomized tumbling based on fragment shape
                std::mt19937 rng(config.random_seed + static_cast<uint32_t>(i));
                std::uniform_real_distribution<float> dist_angular(-1.0f, 1.0f);

                float tumble_strength = velocity_magnitude * 0.5f; // Proportional to linear velocity
                piece.angular_velocity.x = dist_angular(rng) * tumble_strength;
                piece.angular_velocity.y = dist_angular(rng) * tumble_strength;
                piece.angular_velocity.z = dist_angular(rng) * tumble_strength;
            }
        }

        // Generate voxel approximation if requested
        if (config.generate_voxel_approximation) {
            generate_voxel_approximation(piece);
        }

        debris_pieces.push_back(std::move(piece));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    LOG_INFO(Game, "Voronoi fracture complete: {} debris pieces generated in {}ms",
            debris_pieces.size(), duration_ms);

    return debris_pieces;
}

} // namespace lore::world
