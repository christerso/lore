#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <lore/ecs/components/fracture_properties.hpp>
#include <vector>
#include <cstdint>

namespace lore::ecs {

/**
 * @brief Surface damage types
 */
enum class DamageType : uint8_t {
    BulletHole,  ///< Small hole from projectile
    Chip,        ///< Small piece broken off surface
    Dent,        ///< Surface deformation (ductile materials)
    Scratch,     ///< Surface scratching
    Burn,        ///< Heat damage (laser, fire)
    Crack        ///< Surface crack (brittle materials)
};

/**
 * @brief Surface damage tracking for world geometry
 *
 * Handles cosmetic and minor structural damage without full mesh fracture:
 * - Bullet holes (geometry modification)
 * - Surface chips (brittle materials)
 * - Dents (ductile materials)
 * - Cracks (brittle materials under stress)
 * - Burn marks (thermal damage)
 *
 * When polygon budget exceeded, falls back to decals.
 *
 * Integration:
 * - Works with WorldMeshMaterialComponent for material properties
 * - Used by StructuralIntegritySystem for minor impacts
 * - Escalates to VoronoiFractureSystem for major damage
 *
 * Example:
 * @code
 * auto& damage = entity.add_component<SurfaceDamageComponent>();
 * damage.apply_projectile_damage(hit_pos, hit_dir, 1500.0f, fracture_props, mesh);
 * // Creates bullet hole with geometry modification
 * @endcode
 */
struct SurfaceDamageComponent {
    /**
     * @brief Individual damage mark on surface
     */
    struct DamageMark {
        math::Vec3 position;              ///< World position of damage
        math::Vec3 normal;                ///< Surface normal at damage point
        float radius = 0.01f;             ///< Damage radius (meters)
        float depth = 0.005f;             ///< Penetration depth (meters)
        DamageType type = DamageType::BulletHole;
        std::vector<uint32_t> affected_vertices; ///< Vertices modified by this damage

        /**
         * @brief Get bounding sphere for damage mark
         */
        math::geometry::Sphere get_bounding_sphere() const noexcept {
            return math::geometry::Sphere{position, radius};
        }
    };

    /// All damage marks on this surface
    std::vector<DamageMark> damage_marks;

    /// Maximum damage marks before falling back to decals
    uint32_t max_damage_marks = 100;

    /// Total vertices affected by damage (for polygon budget)
    uint32_t total_affected_vertices = 0;

    /// Maximum vertices that can be affected before fallback
    uint32_t max_affected_vertices = 1000;

    /**
     * @brief Apply projectile damage (bullet hole)
     *
     * Creates bullet hole by displacing vertices inward.
     * May chip off small pieces for brittle materials.
     *
     * @param impact_point Impact position (world space)
     * @param impact_direction Projectile direction (normalized)
     * @param kinetic_energy_j Projectile kinetic energy (Joules)
     * @param material_props Material fracture properties
     * @param mesh_vertices Mesh vertex buffer to modify
     * @param mesh_normals Mesh normal buffer
     * @return true if damage applied, false if budget exceeded (use decal)
     */
    template<typename MeshType>
    bool apply_projectile_damage(
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float kinetic_energy_j,
        const FractureProperties& material_props,
        MeshType& mesh_vertices,
        const std::vector<math::Vec3>& mesh_normals
    ) {
        // Check if we've hit budget limits
        if (damage_marks.size() >= max_damage_marks ||
            total_affected_vertices >= max_affected_vertices) {
            return false; // Fall back to decal
        }

        // Calculate hole radius based on projectile energy
        // Empirical formula: sqrt(energy / 1000) = radius in cm
        float hole_radius = std::sqrt(kinetic_energy_j / 1000.0f) * 0.01f; // Convert to meters
        hole_radius = math::clamp(hole_radius, 0.005f, 0.05f); // 0.5cm to 5cm

        // Calculate penetration depth
        float penetration_depth = calculate_penetration_depth(kinetic_energy_j, material_props);

        // Find affected vertices
        auto affected_verts = find_vertices_in_sphere(mesh_vertices, impact_point, hole_radius);

        // Check if adding these vertices would exceed budget
        if (total_affected_vertices + affected_verts.size() > max_affected_vertices) {
            return false; // Fall back to decal
        }

        // Create damage mark
        DamageMark mark{
            .position = impact_point,
            .normal = -impact_direction,
            .radius = hole_radius,
            .depth = penetration_depth,
            .type = DamageType::BulletHole,
            .affected_vertices = affected_verts
        };

        // Apply geometry modification (displace vertices inward)
        for (uint32_t vert_idx : affected_verts) {
            float distance = math::length(mesh_vertices[vert_idx] - impact_point);
            float falloff = 1.0f - (distance / hole_radius); // Linear falloff

            // Displace vertex inward along impact direction
            float displacement = penetration_depth * falloff;
            mesh_vertices[vert_idx] += impact_direction * displacement;
        }

        // For brittle materials, create surface chips
        if (material_props.behavior == FractureBehavior::Brittle &&
            kinetic_energy_j > 500.0f) { // Threshold for chipping
            create_surface_chips(impact_point, impact_direction, hole_radius, mesh_vertices);
        }

        damage_marks.push_back(mark);
        total_affected_vertices += static_cast<uint32_t>(affected_verts.size());

        return true;
    }

    /**
     * @brief Apply surface dent (ductile materials)
     *
     * Creates depression without breaking material.
     *
     * @param impact_point Impact position
     * @param impact_direction Impact direction
     * @param force_n Impact force (Newtons)
     * @param mesh_vertices Mesh to modify
     * @return true if applied, false if budget exceeded
     */
    template<typename MeshType>
    bool apply_dent(
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float force_n,
        MeshType& mesh_vertices
    ) {
        if (damage_marks.size() >= max_damage_marks) {
            return false;
        }

        // Dent radius proportional to force
        float dent_radius = std::sqrt(force_n / 10000.0f) * 0.01f;
        dent_radius = math::clamp(dent_radius, 0.01f, 0.1f); // 1cm to 10cm

        float dent_depth = dent_radius * 0.3f; // Shallow dent

        auto affected_verts = find_vertices_in_sphere(mesh_vertices, impact_point, dent_radius);

        if (total_affected_vertices + affected_verts.size() > max_affected_vertices) {
            return false;
        }

        // Apply smooth dent deformation
        for (uint32_t vert_idx : affected_verts) {
            float distance = math::length(mesh_vertices[vert_idx] - impact_point);
            float falloff = 1.0f - (distance / dent_radius);
            falloff = falloff * falloff; // Squared falloff for smoother dent

            float displacement = dent_depth * falloff;
            mesh_vertices[vert_idx] += impact_direction * displacement;
        }

        DamageMark mark{
            .position = impact_point,
            .normal = -impact_direction,
            .radius = dent_radius,
            .depth = dent_depth,
            .type = DamageType::Dent,
            .affected_vertices = affected_verts
        };

        damage_marks.push_back(mark);
        total_affected_vertices += static_cast<uint32_t>(affected_verts.size());

        return true;
    }

    /**
     * @brief Clear all damage marks (for cleanup/reset)
     */
    void clear_damage() noexcept {
        damage_marks.clear();
        total_affected_vertices = 0;
    }

    /**
     * @brief Check if damage budget is exhausted
     */
    bool is_budget_exhausted() const noexcept {
        return damage_marks.size() >= max_damage_marks ||
               total_affected_vertices >= max_affected_vertices;
    }

private:
    /**
     * @brief Calculate penetration depth from kinetic energy
     */
    float calculate_penetration_depth(
        float kinetic_energy_j,
        const FractureProperties& material_props
    ) const noexcept {
        // Empirical formula based on material hardness
        // Brittle materials have shallow, wide holes
        // Ductile materials have deeper, narrower holes

        float base_depth = std::sqrt(kinetic_energy_j / 1000.0f) * 0.01f; // Base depth in meters

        // Adjust for material behavior
        switch (material_props.behavior) {
            case FractureBehavior::Brittle:
                return base_depth * 0.5f; // Shallow holes
            case FractureBehavior::Ductile:
                return base_depth * 1.5f; // Deeper penetration
            case FractureBehavior::Fibrous:
                return base_depth * 0.8f; // Moderate
            case FractureBehavior::Granular:
                return base_depth * 0.6f; // Crumbles rather than penetrates
            default:
                return base_depth;
        }
    }

    /**
     * @brief Find vertices within sphere
     */
    template<typename MeshType>
    std::vector<uint32_t> find_vertices_in_sphere(
        const MeshType& vertices,
        const math::Vec3& center,
        float radius
    ) const {
        std::vector<uint32_t> result;
        const float radius_sq = radius * radius;

        for (size_t i = 0; i < vertices.size(); ++i) {
            float dist_sq = math::length_squared(vertices[i] - center);
            if (dist_sq <= radius_sq) {
                result.push_back(static_cast<uint32_t>(i));
            }
        }

        return result;
    }

    /**
     * @brief Create surface chips for brittle materials
     *
     * Small pieces break off around impact point.
     */
    template<typename MeshType>
    void create_surface_chips(
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float chip_radius,
        MeshType& mesh_vertices
    ) {
        // Create 2-4 small chips around impact
        uint32_t num_chips = 2 + (rand() % 3);

        for (uint32_t i = 0; i < num_chips; ++i) {
            // Random offset around impact point
            float angle = (static_cast<float>(i) / static_cast<float>(num_chips)) * 2.0f * math::pi<float>();
            math::Vec3 offset{
                std::cos(angle) * chip_radius * 0.5f,
                std::sin(angle) * chip_radius * 0.5f,
                0.0f
            };

            math::Vec3 chip_pos = impact_point + offset;

            // Find nearby vertex to displace (create chip)
            auto chip_verts = find_vertices_in_sphere(mesh_vertices, chip_pos, chip_radius * 0.3f);

            for (uint32_t vert_idx : chip_verts) {
                // Displace vertex outward slightly (chip breaking off)
                mesh_vertices[vert_idx] -= impact_direction * 0.002f; // 2mm
            }
        }
    }
};

} // namespace lore::ecs