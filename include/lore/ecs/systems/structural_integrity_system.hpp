#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/components/world_mesh_material_component.hpp>
#include <lore/ecs/components/transform_component.hpp>
#include <lore/math/math.hpp>

namespace lore::ecs {

/**
 * @brief System that calculates structural loads, stress, and handles collapse
 *
 * This system manages:
 * - Gravitational load distribution through structures
 * - Stress calculations from external forces (projectiles, explosions)
 * - Structural failure detection
 * - Building collapse when critical elements fail
 * - Fracture propagation in brittle materials
 *
 * Integration:
 * - Operates on entities with WorldMeshMaterialComponent
 * - Can run load calculations on GPU (Task 8) for performance
 * - Triggers FractureSystem when material fails
 *
 * Example:
 * @code
 * auto& system = world.get_system<StructuralIntegritySystem>();
 * system.set_gravity({0.0f, -9.81f, 0.0f});
 * system.update(world, delta_time); // Calculate loads and check failures
 * @endcode
 */
class StructuralIntegritySystem : public System {
public:
    StructuralIntegritySystem() = default;
    ~StructuralIntegritySystem() override = default;

    /**
     * @brief Initialize system
     */
    void init(World& world) override {
        // Initialize GPU compute pipelines if available (Task 8)
        // For now, runs on CPU
        gravity_ = math::Vec3{0.0f, -9.81f, 0.0f};
    }

    /**
     * @brief Update structural integrity for all structures
     *
     * Process:
     * 1. Calculate gravitational loads (GPU-accelerated when available)
     * 2. Apply stress from external forces (impacts, explosions)
     * 3. Check for structural failure
     * 4. Handle collapse for failed structures
     * 5. Propagate fractures in brittle materials
     */
    void update(World& world, float delta_time) override {
        // Query all entities with structural materials
        world.query<WorldMeshMaterialComponent, TransformComponent>(
            [this, delta_time](Entity entity,
                             WorldMeshMaterialComponent& materials,
                             TransformComponent& transform) {
                // Reset stress for new frame
                materials.reset_stress();

                // 1. Calculate gravitational loads
                materials.calculate_loads(gravity_);

                // 2. Calculate stress from loads
                // For now, use simplified vertex areas
                // TODO: Get actual vertex areas from mesh component
                std::vector<float> vertex_areas(materials.vertex_masses.size(), 0.01f); // 1cm² per vertex
                materials.calculate_stress_from_loads(vertex_areas);

                // 3. Check for structural failure
                auto failed_vertices = materials.check_structural_failure();

                if (!failed_vertices.empty()) {
                    // 4. Handle collapse
                    handle_structural_failure(entity, materials, failed_vertices);
                }

                // 5. Check for critical failure (imminent collapse)
                if (materials.has_critical_failure()) {
                    trigger_structural_collapse(entity, materials);
                }
            });
    }

    /**
     * @brief Shutdown system
     */
    void shutdown(World& world) override {
        // Clean up GPU resources if allocated
    }

    /**
     * @brief Set gravity vector
     */
    void set_gravity(const math::Vec3& gravity) noexcept {
        gravity_ = gravity;
    }

    /**
     * @brief Get gravity vector
     */
    const math::Vec3& get_gravity() const noexcept {
        return gravity_;
    }

    /**
     * @brief Apply projectile impact to structure
     *
     * Called by ballistics system when projectile hits world geometry.
     *
     * @param entity Entity hit by projectile
     * @param hit_position World position of impact
     * @param impact_direction Projectile direction (normalized)
     * @param kinetic_energy_j Projectile kinetic energy (Joules)
     */
    void apply_projectile_impact(
        World& world,
        Entity entity,
        const math::Vec3& hit_position,
        const math::Vec3& impact_direction,
        float kinetic_energy_j
    ) {
        auto* materials = world.try_get<WorldMeshMaterialComponent>(entity);
        if (!materials) return;

        // Find closest vertex to impact point
        // TODO: Get actual mesh vertices from MeshComponent
        // For now, use simplified approach
        uint32_t closest_vertex = 0;
        float min_distance = std::numeric_limits<float>::max();

        // Calculate vertex normal (simplified - use impact direction)
        math::Vec3 vertex_normal = -impact_direction;

        // Apply impact to closest vertex
        materials->apply_impact(closest_vertex, impact_direction, kinetic_energy_j, vertex_normal);

        // Check for immediate fracture
        auto failed_vertices = materials->check_structural_failure();
        if (!failed_vertices.empty()) {
            handle_structural_failure(entity, *materials, failed_vertices);
        }
    }

    /**
     * @brief Apply explosion pressure wave to structures in radius
     *
     * @param world ECS world
     * @param explosion_position Explosion center (world space)
     * @param explosion_energy_j Explosion energy (Joules)
     * @param max_radius Maximum effect radius (meters)
     */
    void apply_explosion(
        World& world,
        const math::Vec3& explosion_position,
        float explosion_energy_j,
        float max_radius
    ) {
        world.query<WorldMeshMaterialComponent, TransformComponent>(
            [&](Entity entity,
                WorldMeshMaterialComponent& materials,
                TransformComponent& transform) {

                // Check if entity is within explosion radius
                float distance = math::length(transform.position - explosion_position);
                if (distance > max_radius) return;

                // Calculate pressure based on inverse square law
                const float min_distance = 0.1f; // Prevent division by zero
                distance = std::max(distance, min_distance);
                float pressure_pa = explosion_energy_j / (4.0f * math::pi<float>() * distance * distance);

                // Apply pressure to all vertices (simplified)
                for (size_t v = 0; v < materials.vertex_stress.size(); ++v) {
                    math::Vec3 direction = math::normalize(transform.position - explosion_position);

                    // Simplified vertex normal
                    math::Vec3 vertex_normal = direction;

                    // Apply scaled pressure as impact
                    materials.apply_impact(static_cast<uint32_t>(v), direction, pressure_pa * 0.01f, vertex_normal);
                }

                // Check for failures
                auto failed_vertices = materials.check_structural_failure();
                if (!failed_vertices.empty()) {
                    handle_structural_failure(entity, materials, failed_vertices);
                }

                // Check for critical failure
                if (materials.has_critical_failure()) {
                    trigger_structural_collapse(entity, materials);
                }
            });
    }

private:
    /**
     * @brief Handle structural failure of vertices
     *
     * Propagates fractures in brittle materials (glass, concrete)
     */
    void handle_structural_failure(
        Entity entity,
        WorldMeshMaterialComponent& materials,
        const std::vector<uint32_t>& failed_vertices
    ) {
        for (uint32_t vertex_id : failed_vertices) {
            const auto& material = materials.get_material(vertex_id);

            // Brittle materials (glass, concrete) propagate fractures
            if (material.is_brittle) {
                materials.propagate_fracture(vertex_id);
            }
            // Ductile materials (steel, aluminum) deform but don't shatter
            else {
                // Mark as yielding (permanent deformation)
                materials.vertex_stress[vertex_id].is_yielding = true;
            }
        }
    }

    /**
     * @brief Trigger structural collapse
     *
     * Called when critical load-bearing elements fail.
     * This would separate fractured sections and apply gravity to unsupported parts.
     *
     * TODO: Implement full collapse simulation with:
     * - Mesh separation along fracture lines
     * - Rigid body creation for debris
     * - Gravity application to unsupported sections
     */
    void trigger_structural_collapse(
        Entity entity,
        WorldMeshMaterialComponent& materials
    ) {
        // For now, mark entire structure as failed
        // Full implementation would:
        // 1. Separate mesh into disconnected components
        // 2. Create new entities for each separated piece
        // 3. Add RigidBodyComponent with physics
        // 4. Apply gravity to falling sections

        // Mark all vertices as fractured (simplified)
        for (auto& stress : materials.vertex_stress) {
            stress.is_fractured = true;
        }
    }

    /// Gravity vector (m/s²)
    math::Vec3 gravity_{0.0f, -9.81f, 0.0f};
};

} // namespace lore::ecs