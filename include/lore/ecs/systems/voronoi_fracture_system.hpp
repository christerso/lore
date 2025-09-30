#pragma once

#include "lore/ecs/world.hpp"
#include "lore/ecs/system.hpp"
#include "lore/ecs/components/fracture_properties.hpp"
#include "lore/ecs/components/world_mesh_material_component.hpp"
#include "lore/ecs/components/transform_component.hpp"
#include "lore/ecs/systems/debris_manager.hpp"
#include "lore/math/vec3.hpp"
#include "lore/physics/rigid_body_component.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace lore::ecs {

/**
 * @brief Voronoi diagram for mesh fracture
 *
 * Represents a 3D Voronoi diagram used to slice meshes into realistic fracture pieces.
 * Generated on GPU for performance (research shows 500 cells at 60 FPS).
 */
struct VoronoiDiagram {
    std::vector<math::Vec3> seed_points;        ///< Voronoi cell centers
    std::vector<uint32_t> cell_vertex_counts;   ///< Vertices per cell
    std::vector<uint32_t> cell_vertex_offsets;  ///< Offset into vertex buffer
    std::vector<math::Vec3> cell_vertices;      ///< All cell vertices (packed)
    std::vector<uint32_t> cell_indices;         ///< Triangle indices (packed)

    math::Vec3 bounds_min{-1.0f, -1.0f, -1.0f};
    math::Vec3 bounds_max{1.0f, 1.0f, 1.0f};

    bool is_gpu_generated = false;              ///< Was this generated on GPU?
    uint32_t compute_time_us = 0;               ///< GPU generation time
};

/**
 * @brief Mesh fragment from Voronoi fracture
 *
 * A single piece of a fractured mesh with its own geometry and physics.
 */
struct MeshFragment {
    std::vector<math::Vec3> vertices;
    std::vector<math::Vec3> normals;
    std::vector<uint32_t> indices;

    math::Vec3 center_of_mass{0.0f};
    float mass_kg = 0.0f;
    float volume_m3 = 0.0f;

    // Physics initial conditions
    math::Vec3 initial_velocity{0.0f};
    math::Vec3 initial_angular_velocity{0.0f};

    uint32_t source_cell_index = 0;             ///< Which Voronoi cell created this
};

/**
 * @brief Configuration for Voronoi fracture generation
 */
struct VoronoiFractureConfig {
    uint32_t seed_count = 10;                   ///< Number of fracture pieces
    float seed_clustering = 0.3f;               ///< 0=uniform, 1=highly clustered near impact

    bool use_gpu_generation = true;             ///< Use compute shader for Voronoi?
    bool weld_small_fragments = true;           ///< Merge fragments < threshold
    float min_fragment_volume_m3 = 0.001f;      ///< Minimum fragment size (1cm³)

    float impact_velocity_scale = 1.0f;         ///< Multiplier for fragment velocities
    float angular_velocity_scale = 1.0f;        ///< Multiplier for fragment rotation

    bool create_interior_faces = true;          ///< Generate faces inside the mesh
    float edge_sharpness = 1.0f;                ///< How sharp fracture edges are (0-1)
};

/**
 * @brief System that fractures meshes using GPU-accelerated Voronoi diagrams
 *
 * Implements dynamic mesh fracture with realistic material-specific patterns.
 * Uses GPU compute shaders for Voronoi diagram generation (500 cells at 60 FPS).
 *
 * Features:
 * - GPU-accelerated Voronoi generation
 * - Material-specific fracture patterns (glass, metal, wood, concrete)
 * - Impact-centered seed clustering
 * - Fragment welding for small pieces
 * - Automatic debris management integration
 * - Energy-based fracture triggering
 *
 * Research: Liu et al., "Real-time Dynamic Fracture with Volumetric Approximate Convex Decompositions"
 * GPU performance: 500 Voronoi cells at 60 FPS on modern hardware
 *
 * Example usage:
 * @code
 * auto fracture_system = std::make_shared<VoronoiFractureSystem>();
 * world.add_system(fracture_system);
 *
 * // Fracture a wall from projectile impact
 * VoronoiFractureConfig config;
 * config.seed_count = 15;
 * config.seed_clustering = 0.7f; // Cluster near impact
 *
 * fracture_system->fracture_mesh_at_point(
 *     world, wall_entity,
 *     impact_point, impact_direction,
 *     kinetic_energy_joules, config
 * );
 * @endcode
 */
class VoronoiFractureSystem : public System {
public:
    VoronoiFractureSystem();
    ~VoronoiFractureSystem() override = default;

    void update(World& world, float delta_time) override;

    /**
     * @brief Fracture a mesh at a specific point
     *
     * Generates Voronoi diagram centered at impact point, slices mesh into fragments,
     * creates debris entities with physics, and removes original entity.
     *
     * @param world ECS world
     * @param entity Entity with mesh to fracture
     * @param impact_point World-space impact location
     * @param impact_direction Direction of force
     * @param kinetic_energy_j Impact energy in Joules
     * @param config Fracture generation parameters
     * @return Number of fragments created
     */
    uint32_t fracture_mesh_at_point(
        World& world,
        Entity entity,
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float kinetic_energy_j,
        const VoronoiFractureConfig& config
    );

    /**
     * @brief Fracture along structural failure lines
     *
     * Uses stress analysis from WorldMeshMaterialComponent to generate natural
     * fracture patterns along lines of weakness.
     *
     * @param world ECS world
     * @param entity Entity with mesh to fracture
     * @param failed_vertices Vertices that exceeded strength limits
     * @param config Fracture generation parameters
     * @return Number of fragments created
     */
    uint32_t fracture_along_stress_lines(
        World& world,
        Entity entity,
        const std::vector<uint32_t>& failed_vertices,
        const VoronoiFractureConfig& config
    );

    /**
     * @brief Generate Voronoi diagram on GPU
     *
     * Uses compute shader to generate 3D Voronoi diagram from seed points.
     * Falls back to CPU generation if GPU unavailable.
     *
     * @param seed_points Voronoi cell centers
     * @param bounds_min Minimum bounds for generation
     * @param bounds_max Maximum bounds for generation
     * @return Generated Voronoi diagram
     */
    VoronoiDiagram generate_voronoi_diagram_gpu(
        const std::vector<math::Vec3>& seed_points,
        const math::Vec3& bounds_min,
        const math::Vec3& bounds_max
    );

    /**
     * @brief Generate Voronoi diagram on CPU (fallback)
     *
     * CPU-based Voronoi generation for when GPU is unavailable or for small
     * cell counts (< 10 cells).
     *
     * @param seed_points Voronoi cell centers
     * @param bounds_min Minimum bounds
     * @param bounds_max Maximum bounds
     * @return Generated Voronoi diagram
     */
    VoronoiDiagram generate_voronoi_diagram_cpu(
        const std::vector<math::Vec3>& seed_points,
        const math::Vec3& bounds_min,
        const math::Vec3& bounds_max
    );

    /**
     * @brief Slice mesh by Voronoi cells
     *
     * Intersects mesh triangles with Voronoi cell boundaries to generate fragments.
     *
     * @param mesh_vertices Original mesh vertices
     * @param mesh_normals Original mesh normals
     * @param mesh_indices Original mesh triangle indices
     * @param voronoi Voronoi diagram to slice by
     * @param material_density Density for mass calculation (kg/m³)
     * @return Vector of mesh fragments
     */
    std::vector<MeshFragment> slice_mesh_by_voronoi(
        const std::vector<math::Vec3>& mesh_vertices,
        const std::vector<math::Vec3>& mesh_normals,
        const std::vector<uint32_t>& mesh_indices,
        const VoronoiDiagram& voronoi,
        float material_density
    );

    /**
     * @brief Create debris entities from fragments
     *
     * Instantiates ECS entities for each fragment with:
     * - MeshComponent for rendering
     * - RigidBodyComponent for physics
     * - FractureProperties for material behavior
     * - DebrisComponent for lifetime tracking
     *
     * @param world ECS world
     * @param fragments Mesh fragments to create
     * @param material_props Material properties from original mesh
     * @param impact_velocity Initial velocity for fragments
     * @return Vector of created debris entities
     */
    std::vector<Entity> create_debris_entities(
        World& world,
        const std::vector<MeshFragment>& fragments,
        const FractureProperties& material_props,
        const math::Vec3& impact_velocity
    );

    /**
     * @brief Set debris manager for integration
     *
     * Links this fracture system with the debris manager for polygon budget
     * enforcement and lifetime tracking.
     *
     * @param debris_manager Shared pointer to debris manager
     */
    void set_debris_manager(std::shared_ptr<DebrisManager> debris_manager) {
        debris_manager_ = debris_manager;
    }

    /**
     * @brief Get GPU compute shader handle
     *
     * Returns the handle to the Voronoi compute shader for GPU generation.
     * Returns 0 if GPU generation is disabled or shader failed to compile.
     *
     * @return Shader handle (platform-specific)
     */
    uint32_t get_voronoi_compute_shader_handle() const noexcept {
        return voronoi_compute_shader_handle_;
    }

    /**
     * @brief Check if GPU generation is available
     *
     * @return true if GPU compute is initialized and functional
     */
    bool is_gpu_generation_available() const noexcept {
        return gpu_generation_available_;
    }

private:
    /**
     * @brief Generate seed points for Voronoi cells
     *
     * Creates seed points with optional clustering near impact point.
     * Uses material properties to influence fracture pattern.
     *
     * @param impact_point Center point for clustering
     * @param bounds_min Mesh bounding box min
     * @param bounds_max Mesh bounding box max
     * @param material_props Material fracture properties
     * @param config Fracture configuration
     * @return Vector of seed points
     */
    std::vector<math::Vec3> generate_seed_points(
        const math::Vec3& impact_point,
        const math::Vec3& bounds_min,
        const math::Vec3& bounds_max,
        const FractureProperties& material_props,
        const VoronoiFractureConfig& config
    );

    /**
     * @brief Apply material-specific seed distribution
     *
     * Modifies seed points based on material behavior:
     * - Brittle: Radial pattern from impact
     * - Ductile: Linear tears along stress
     * - Fibrous: Aligned with grain direction
     * - Granular: Random irregular chunks
     *
     * @param seed_points Seeds to modify (in/out)
     * @param impact_point Impact location
     * @param impact_direction Impact force direction
     * @param material_props Material properties
     */
    void apply_material_seed_pattern(
        std::vector<math::Vec3>& seed_points,
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        const FractureProperties& material_props
    );

    /**
     * @brief Calculate fragment initial velocities
     *
     * Distributes kinetic energy to fragments based on distance from impact,
     * fragment mass, and material properties.
     *
     * @param fragments Fragments to apply velocities to (in/out)
     * @param impact_point Impact location
     * @param impact_direction Impact force direction
     * @param kinetic_energy_j Total impact energy
     * @param config Velocity scaling parameters
     */
    void calculate_fragment_velocities(
        std::vector<MeshFragment>& fragments,
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float kinetic_energy_j,
        const VoronoiFractureConfig& config
    );

    /**
     * @brief Weld small fragments together
     *
     * Merges fragments smaller than threshold into nearby larger fragments
     * to reduce polygon count and entity count.
     *
     * @param fragments Fragments to weld (in/out)
     * @param min_volume_m3 Minimum fragment volume threshold
     */
    void weld_small_fragments(
        std::vector<MeshFragment>& fragments,
        float min_volume_m3
    );

    /**
     * @brief Calculate mesh bounding box
     *
     * @param vertices Mesh vertices
     * @param bounds_min Output minimum bounds
     * @param bounds_max Output maximum bounds
     */
    void calculate_mesh_bounds(
        const std::vector<math::Vec3>& vertices,
        math::Vec3& bounds_min,
        math::Vec3& bounds_max
    );

    /**
     * @brief Initialize GPU compute pipeline
     *
     * Compiles Voronoi compute shader and sets up GPU buffers.
     * Called automatically on first use.
     *
     * @return true if initialization succeeded
     */
    bool initialize_gpu_compute();

    /**
     * @brief Build Voronoi cell geometry on GPU
     *
     * Executes compute shader to construct Voronoi cell vertices and faces.
     *
     * @param seed_points Seed point positions
     * @param bounds_min Bounding box min
     * @param bounds_max Bounding box max
     * @param output_diagram Output Voronoi diagram
     * @return true if GPU generation succeeded
     */
    bool execute_voronoi_compute_shader(
        const std::vector<math::Vec3>& seed_points,
        const math::Vec3& bounds_min,
        const math::Vec3& bounds_max,
        VoronoiDiagram& output_diagram
    );

    /**
     * @brief Intersect triangle with Voronoi cell
     *
     * Clips triangle against Voronoi cell boundaries using Sutherland-Hodgman algorithm.
     *
     * @param tri_verts Triangle vertices (3 points)
     * @param tri_normal Triangle normal
     * @param cell_index Voronoi cell to test against
     * @param voronoi Voronoi diagram
     * @param output_verts Output clipped vertices
     * @param output_normal Output normal
     * @return true if triangle intersects cell
     */
    bool clip_triangle_to_cell(
        const math::Vec3 tri_verts[3],
        const math::Vec3& tri_normal,
        uint32_t cell_index,
        const VoronoiDiagram& voronoi,
        std::vector<math::Vec3>& output_verts,
        math::Vec3& output_normal
    );

    /**
     * @brief Generate interior faces for fracture surfaces
     *
     * Creates geometry on the inside of fractured pieces where the cut occurred.
     *
     * @param fragment Fragment to add interior faces to (in/out)
     * @param cell_index Voronoi cell that created this fragment
     * @param voronoi Voronoi diagram
     * @param edge_sharpness How sharp the edges should be (0-1)
     */
    void generate_interior_faces(
        MeshFragment& fragment,
        uint32_t cell_index,
        const VoronoiDiagram& voronoi,
        float edge_sharpness
    );

private:
    std::shared_ptr<DebrisManager> debris_manager_;

    // GPU compute resources
    uint32_t voronoi_compute_shader_handle_ = 0;
    uint32_t seed_buffer_handle_ = 0;
    uint32_t output_buffer_handle_ = 0;
    bool gpu_generation_available_ = false;
    bool gpu_compute_initialized_ = false;

    // Statistics
    uint32_t total_fractures_created_ = 0;
    uint32_t total_fragments_created_ = 0;
    uint64_t total_gpu_compute_time_us_ = 0;
};

} // namespace lore::ecs