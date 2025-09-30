#pragma once

#include <lore/math/math.hpp>
#include <vector>
#include <cstdint>

namespace lore::world {

/**
 * @brief Voxel cell for internal fluid simulation approximation
 *
 * Each debris piece has an internal 4×4×4 voxel grid for smoke/liquid
 * collision detection. Voxels are NEVER rendered - only used for physics.
 */
struct VoxelCell {
    math::Vec3 position;        // Local position within debris piece
    float density = 0.0f;       // Fluid density (0-1)
    bool is_occupied = false;   // Is this voxel inside the debris geometry?
};

/**
 * @brief Single debris piece from Voronoi fracture
 *
 * Contains full geometry (vertices, indices, normals, UVs) for rendering
 * plus physics properties for realistic tumbling and rotation.
 *
 * Memory: ~50KB per piece (20 pieces × 200 verts × 12 bytes)
 */
struct DebrisPiece {
    // Visual geometry (local space)
    std::vector<math::Vec3> vertices;       // Local space vertex positions
    std::vector<uint32_t> indices;          // Triangle indices (groups of 3)
    std::vector<math::Vec3> normals;        // Per-vertex normals
    std::vector<math::Vec2> uvs;            // Texture coordinates

    // Physics properties
    math::Vec3 centroid;                    // Center of mass (local space)
    math::Vec3 position;                    // World position
    math::Quat rotation;                    // Orientation quaternion
    math::Vec3 velocity;                    // Linear velocity (m/s)
    math::Vec3 angular_velocity;            // Rotational velocity (rad/s)
    float mass = 1.0f;                      // Mass in kg
    math::Vec3 inertia_tensor;              // Diagonal inertia tensor

    // Visual properties
    uint32_t material_id = 0;               // PBR material index

    // Internal voxel approximation for fluid simulation (4×4×4 = 64 cells)
    std::vector<VoxelCell> voxel_approximation;

    // Bounding volume
    math::Vec3 aabb_min;
    math::Vec3 aabb_max;

    // State
    bool is_sleeping = false;               // Has velocity dropped below threshold?
    float time_since_creation = 0.0f;       // For lifetime management
};

/**
 * @brief Impact data for directional fracture (Task 26)
 *
 * Controls how fracture patterns are influenced by impact type and direction.
 * Different impact types create different fragment distributions.
 */
enum class ImpactType : uint32_t {
    PointImpact = 0,    // Bullet: small cone, many tiny fragments
    BluntForce = 1,     // Hammer: depression with radial cracks
    Explosion = 2,      // Spherical distribution, fragments blown outward
    Cutting = 3,        // Axe/sword: clean split along direction
    Crushing = 4,       // Weight: vertical compression, horizontal spreading
    Shearing = 5        // Lateral force: diagonal fracture pattern
};

struct ImpactData {
    math::Vec3 position;            // Impact point (world space)
    math::Vec3 direction;           // Impact direction (normalized)
    float force;                    // Impact force (Newtons)
    ImpactType type;                // Type of impact
    float impulse_duration;         // Duration of impulse (seconds)
};

/**
 * @brief Material-specific fracture parameters
 *
 * Different materials fracture differently:
 * - Stone: 5-8 large angular chunks
 * - Wood: 15-25 splinter-like shards along grain
 * - Glass: 20-40 sharp triangular pieces
 */
struct MaterialFractureParams {
    uint32_t min_fragments = 15;        // Minimum debris pieces
    uint32_t max_fragments = 30;        // Maximum debris pieces
    float fragment_size_variance = 0.5f; // 0=uniform, 1=extreme variance
    float angular_bias = 0.0f;          // 0=smooth, 1=angular chunks
    math::Vec3 grain_direction{0,0,0};  // For anisotropic materials (wood)
    float brittleness = 0.5f;           // 0=ductile, 1=brittle
};

/**
 * @brief Voronoi fracture configuration
 */
struct VoronoiFractureConfig {
    uint32_t num_fragments = 20;            // Target number of debris pieces
    float poisson_min_distance = 0.1f;      // Minimum distance between fracture points
    MaterialFractureParams material_params; // Material-specific settings
    ImpactData* impact = nullptr;           // Optional: directional fracture
    uint32_t random_seed = 0;               // 0 = use time-based seed
    bool generate_voxel_approximation = true; // Create internal voxels for fluid sim
};

/**
 * @brief Voronoi fracture system
 *
 * Fractures a mesh into 15-30 irregular Voronoi polyhedra.
 * Far superior to cubic voxels for visual quality.
 *
 * Algorithm:
 * 1. Generate fracture points using Poisson disk sampling
 * 2. Compute 3D Voronoi diagram
 * 3. Clip Voronoi cells against original mesh AABB
 * 4. Calculate physics properties (mass, inertia, centroid)
 * 5. Transfer UVs and materials to fragments
 *
 * Performance: <20ms per tile on modern hardware
 */
class VoronoiFracture {
public:
    /**
     * @brief Fracture a mesh into debris pieces
     *
     * @param vertices Input mesh vertices (local space)
     * @param indices Input mesh indices
     * @param normals Input mesh normals
     * @param uvs Input mesh UVs
     * @param config Fracture configuration
     * @return Vector of debris pieces (empty if fracture failed)
     *
     * Thread-safe: Can be called from multiple threads simultaneously.
     */
    static std::vector<DebrisPiece> fracture_mesh(
        const std::vector<math::Vec3>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<math::Vec3>& normals,
        const std::vector<math::Vec2>& uvs,
        const VoronoiFractureConfig& config
    );

    /**
     * @brief Generate Poisson disk sample points within AABB
     *
     * Uses Bridson's algorithm for uniform distribution with minimum distance.
     *
     * @param aabb_min Bounding box minimum
     * @param aabb_max Bounding box maximum
     * @param min_distance Minimum distance between points
     * @param max_points Maximum number of points to generate
     * @param seed Random seed (0 = time-based)
     * @return Vector of sample points
     */
    static std::vector<math::Vec3> generate_poisson_samples(
        const math::Vec3& aabb_min,
        const math::Vec3& aabb_max,
        float min_distance,
        uint32_t max_points,
        uint32_t seed = 0
    );

    /**
     * @brief Calculate physics properties for a debris piece
     *
     * Computes centroid, mass, and diagonal inertia tensor from geometry.
     * Assumes uniform density.
     *
     * @param piece Debris piece to compute properties for (modifies in-place)
     */
    static void calculate_physics_properties(DebrisPiece& piece);

    /**
     * @brief Generate internal voxel approximation for fluid simulation
     *
     * Creates 4×4×4 voxel grid inside debris piece for smoke/liquid collision.
     * Marks voxels as occupied if they're inside the mesh geometry.
     *
     * @param piece Debris piece to create voxels for
     */
    static void generate_voxel_approximation(DebrisPiece& piece);

private:
    /**
     * @brief Compute 3D Voronoi diagram
     *
     * Creates Voronoi cells for each sample point within the bounding box.
     *
     * @param sample_points Voronoi seed points
     * @param aabb_min Bounding box minimum
     * @param aabb_max Bounding box maximum
     * @return Vector of Voronoi cells (one per sample point)
     */
    static std::vector<std::vector<math::Vec3>> compute_voronoi_cells(
        const std::vector<math::Vec3>& sample_points,
        const math::Vec3& aabb_min,
        const math::Vec3& aabb_max
    );

    /**
     * @brief Clip Voronoi cell against mesh geometry
     *
     * Intersects Voronoi polyhedron with original mesh triangles.
     * Generates final debris geometry.
     *
     * @param voronoi_cell Voronoi cell vertices
     * @param mesh_vertices Original mesh vertices
     * @param mesh_indices Original mesh indices
     * @return Clipped geometry (vertices and indices)
     */
    struct ClippedGeometry {
        std::vector<math::Vec3> vertices;
        std::vector<uint32_t> indices;
    };

    static ClippedGeometry clip_cell_against_mesh(
        const std::vector<math::Vec3>& voronoi_cell,
        const std::vector<math::Vec3>& mesh_vertices,
        const std::vector<uint32_t>& mesh_indices
    );

    /**
     * @brief Transfer UVs from original mesh to debris piece
     *
     * Projects debris vertices onto original mesh surface to get UVs.
     *
     * @param piece Debris piece (modifies uvs in-place)
     * @param original_vertices Original mesh vertices
     * @param original_indices Original mesh indices
     * @param original_uvs Original mesh UVs
     */
    static void transfer_uvs(
        DebrisPiece& piece,
        const std::vector<math::Vec3>& original_vertices,
        const std::vector<uint32_t>& original_indices,
        const std::vector<math::Vec2>& original_uvs
    );

    /**
     * @brief Calculate AABB for a set of vertices
     */
    static void calculate_aabb(
        const std::vector<math::Vec3>& vertices,
        math::Vec3& out_min,
        math::Vec3& out_max
    );

    /**
     * @brief Check if point is inside mesh using ray casting
     */
    static bool is_point_inside_mesh(
        const math::Vec3& point,
        const std::vector<math::Vec3>& mesh_vertices,
        const std::vector<uint32_t>& mesh_indices
    );
};

} // namespace lore::world
