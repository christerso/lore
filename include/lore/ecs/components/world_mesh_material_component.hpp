#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <lore/physics/structural_material.hpp>
#include <vector>
#include <cstdint>

namespace lore::ecs {

/**
 * @brief Material properties and structural physics for world mesh entities
 *
 * Assigns structural materials to world geometry and calculates:
 * - Load bearing (gravitational stress on structures)
 * - Structural failure (when stress exceeds material strength)
 * - Fracture propagation (cracks spreading through brittle materials)
 *
 * NO ABSTRACT "BUILDING HEALTH" - calculate actual stress and fracture!
 *
 * Integration with existing systems:
 * - Uses physics::StructuralMaterial for material properties
 * - Works with GPU compute (Task 8) for parallel stress calculations
 * - Integrates with FractureSystem for dynamic mesh breakage
 *
 * Example:
 * @code
 * auto& materials = building_entity.add_component<WorldMeshMaterialComponent>();
 * materials.materials.push_back(physics::StructuralMaterial::create_concrete());
 * materials.calculate_loads(math::Vec3{0.0f, -9.81f, 0.0f}); // Apply gravity
 *
 * // Check for structural failure
 * auto failed_vertices = materials.check_structural_failure();
 * if (!failed_vertices.empty()) {
 *     // Building is collapsing!
 * }
 * @endcode
 */
struct WorldMeshMaterialComponent {
    /**
     * @brief Stress state per vertex (calculated per frame)
     */
    struct StressState {
        float tensile_stress_pa = 0.0f;      ///< Pulling force (Pa)
        float compressive_stress_pa = 0.0f;  ///< Crushing force (Pa)
        float shear_stress_pa = 0.0f;        ///< Sliding force (Pa)
        float von_mises_stress_pa = 0.0f;    ///< Combined stress metric (Pa)
        bool is_yielding = false;            ///< Permanent deformation?
        bool is_fractured = false;           ///< Has it broken?

        /**
         * @brief Reset stress state (called per frame before recalculation)
         */
        void reset() noexcept {
            tensile_stress_pa = 0.0f;
            compressive_stress_pa = 0.0f;
            shear_stress_pa = 0.0f;
            von_mises_stress_pa = 0.0f;
            is_yielding = false;
            // Note: is_fractured persists (fractures don't heal)
        }

        /**
         * @brief Check if stress exceeds safe limits
         */
        bool is_overstressed(const physics::StructuralMaterial& material) const noexcept {
            return tensile_stress_pa > material.tensile_strength_pa ||
                   compressive_stress_pa > material.compressive_strength_pa ||
                   von_mises_stress_pa > material.max_stress_pa;
        }
    };

    /**
     * @brief Load-bearing edge (structural connection between vertices)
     *
     * Represents a beam or column in the structure that transfers loads.
     */
    struct LoadBearingEdge {
        uint32_t vertex_a = 0;               ///< First vertex index
        uint32_t vertex_b = 0;               ///< Second vertex index
        float load_capacity_n = 10000.0f;    ///< Max load in Newtons
        float current_load_n = 0.0f;         ///< Current load (calculated)
        bool is_critical = false;            ///< Structural failure if breaks?

        /**
         * @brief Check if edge is overloaded
         */
        bool is_overloaded() const noexcept {
            return current_load_n > load_capacity_n;
        }

        /**
         * @brief Get load ratio (0.0 = no load, 1.0 = at capacity, >1.0 = overloaded)
         */
        float get_load_ratio() const noexcept {
            return (load_capacity_n > 0.0f) ? (current_load_n / load_capacity_n) : 0.0f;
        }
    };

    /// Material for each submesh/material group
    std::vector<physics::StructuralMaterial> materials;

    /// Mass per vertex (for load bearing calculations) (kg)
    std::vector<float> vertex_masses;

    /// Load on each vertex (Newtons) - calculated from gravity and supported mass
    std::vector<float> vertex_loads;

    /// Stress state per vertex
    std::vector<StressState> vertex_stress;

    /// Load-bearing connections (which vertices support which)
    std::vector<LoadBearingEdge> load_bearing_edges;

    /// Total mass of structure (kg)
    float total_mass_kg = 0.0f;

    /**
     * @brief Initialize component for mesh with vertex count
     *
     * @param vertex_count Number of vertices in mesh
     * @param default_material Default structural material to use
     */
    void initialize(
        uint32_t vertex_count,
        const physics::StructuralMaterial& default_material = physics::StructuralMaterial::create_concrete()
    ) {
        materials.push_back(default_material);
        vertex_masses.resize(vertex_count, 1.0f);
        vertex_loads.resize(vertex_count, 0.0f);
        vertex_stress.resize(vertex_count);

        // Calculate total mass
        total_mass_kg = 0.0f;
        for (float mass : vertex_masses) {
            total_mass_kg += mass;
        }
    }

    /**
     * @brief Calculate gravitational loads on structure
     *
     * Propagates loads down through load_bearing_edges.
     * This should run on GPU (Task 8: GPU Compute Systems) for performance.
     *
     * @param gravity Gravity vector (e.g., {0, -9.81, 0})
     */
    void calculate_loads(const math::Vec3& gravity) {
        const float gravity_magnitude = math::length(gravity);

        // Reset loads
        std::fill(vertex_loads.begin(), vertex_loads.end(), 0.0f);

        // Apply direct gravitational force to each vertex
        for (size_t i = 0; i < vertex_masses.size(); ++i) {
            vertex_loads[i] = vertex_masses[i] * gravity_magnitude;
        }

        // Propagate loads through load-bearing edges
        // This is simplified - real version uses iterative solver or GPU compute
        // Multiple passes to propagate loads down through structure
        const int propagation_passes = 5;
        for (int pass = 0; pass < propagation_passes; ++pass) {
            for (auto& edge : load_bearing_edges) {
                // Vertex B supports vertex A
                // Transfer portion of A's load to B
                const float transfer_ratio = 0.5f; // Simplification
                const float transferred_load = vertex_loads[edge.vertex_a] * transfer_ratio;

                vertex_loads[edge.vertex_b] += transferred_load;
                edge.current_load_n = transferred_load;
            }
        }
    }

    /**
     * @brief Calculate stress from loads (load / area)
     *
     * @param vertex_areas Cross-sectional area for each vertex (m²)
     */
    void calculate_stress_from_loads(const std::vector<float>& vertex_areas) {
        for (size_t i = 0; i < vertex_loads.size(); ++i) {
            if (i >= vertex_areas.size() || vertex_areas[i] <= 0.0f) continue;

            // Stress = Force / Area (Pa = N/m²)
            const float stress_pa = vertex_loads[i] / vertex_areas[i];

            // Assume compressive stress (gravity pushes down)
            vertex_stress[i].compressive_stress_pa = stress_pa;

            // Calculate von Mises stress (simplified)
            vertex_stress[i].von_mises_stress_pa = stress_pa;

            // Check if yielding
            const auto& material = materials[i % materials.size()];
            vertex_stress[i].is_yielding = (stress_pa > material.yield_strength_pa);
        }
    }

    /**
     * @brief Check structural failure (vertices exceeding material strength)
     *
     * @return List of failed vertex indices
     */
    std::vector<uint32_t> check_structural_failure() {
        std::vector<uint32_t> failed_vertices;

        for (size_t i = 0; i < vertex_stress.size(); ++i) {
            const auto& stress = vertex_stress[i];
            const auto& material = materials[i % materials.size()];

            // Check if stress exceeds material strength
            if (stress.tensile_stress_pa > material.tensile_strength_pa ||
                stress.compressive_stress_pa > material.compressive_strength_pa ||
                stress.von_mises_stress_pa > material.max_stress_pa) {

                failed_vertices.push_back(static_cast<uint32_t>(i));
                vertex_stress[i].is_fractured = true;
            }
        }

        return failed_vertices;
    }

    /**
     * @brief Check if critical load-bearing edges have failed
     *
     * @return true if structure has critical failure (collapse imminent)
     */
    bool has_critical_failure() const {
        for (const auto& edge : load_bearing_edges) {
            if (edge.is_critical && edge.is_overloaded()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Apply projectile impact and calculate damage
     *
     * Uses ballistics energy to calculate stress.
     *
     * @param vertex_index Vertex hit by projectile
     * @param impact_direction Impact direction (normalized)
     * @param kinetic_energy_j Projectile kinetic energy (Joules)
     * @param vertex_normal Normal vector at hit vertex
     */
    void apply_impact(
        uint32_t vertex_index,
        const math::Vec3& impact_direction,
        float kinetic_energy_j,
        const math::Vec3& vertex_normal
    ) {
        if (vertex_index >= vertex_stress.size()) return;

        const auto& material = materials[vertex_index % materials.size()];

        // Convert kinetic energy to stress (force over area)
        // Simplified: energy / (impact area * penetration depth)
        const float impact_area_m2 = 0.001f;  // ~1cm² (bullet impact)
        const float penetration_depth_m = 0.1f; // ~10cm
        const float impact_force_n = kinetic_energy_j / penetration_depth_m;
        const float impact_stress_pa = impact_force_n / impact_area_m2;

        auto& stress = vertex_stress[vertex_index];

        // Determine stress type based on impact direction vs surface normal
        const float normal_component = math::dot(impact_direction, vertex_normal);

        if (normal_component > 0) {
            // Impact pushing into surface (compressive)
            stress.compressive_stress_pa += impact_stress_pa;
        } else {
            // Impact pulling on surface (tensile)
            stress.tensile_stress_pa += impact_stress_pa;
        }

        // Update von Mises stress
        stress.von_mises_stress_pa = std::sqrt(
            stress.tensile_stress_pa * stress.tensile_stress_pa +
            stress.compressive_stress_pa * stress.compressive_stress_pa
        );

        // Check for fracture
        if (stress.is_overstressed(material)) {
            stress.is_fractured = true;

            // Brittle materials propagate fracture
            if (material.is_brittle) {
                propagate_fracture(vertex_index);
            }
        }
    }

    /**
     * @brief Propagate fracture to adjacent vertices (for brittle materials)
     *
     * Cracks spread through brittle materials like glass and concrete.
     * This should run on GPU (Task 8) for performance.
     *
     * @param origin_vertex Vertex where fracture started
     */
    void propagate_fracture(uint32_t origin_vertex) {
        if (origin_vertex >= vertex_stress.size()) return;

        const auto& material = materials[origin_vertex % materials.size()];
        if (!material.is_brittle) return;

        // Find connected vertices through load-bearing edges
        std::vector<uint32_t> adjacent_vertices;
        for (const auto& edge : load_bearing_edges) {
            if (edge.vertex_a == origin_vertex) {
                adjacent_vertices.push_back(edge.vertex_b);
            } else if (edge.vertex_b == origin_vertex) {
                adjacent_vertices.push_back(edge.vertex_a);
            }
        }

        // Propagate crack based on stress intensity
        const auto& origin_stress = vertex_stress[origin_vertex];
        const float stress_intensity = origin_stress.von_mises_stress_pa / material.max_stress_pa;

        // Brittle materials crack easily (>50% stress intensity propagates)
        if (stress_intensity > 0.5f) {
            for (uint32_t adj_vertex : adjacent_vertices) {
                if (!vertex_stress[adj_vertex].is_fractured) {
                    // Transfer stress to adjacent vertex (simplified)
                    vertex_stress[adj_vertex].von_mises_stress_pa +=
                        origin_stress.von_mises_stress_pa * 0.3f;

                    // Check if adjacent vertex now fractures
                    if (vertex_stress[adj_vertex].is_overstressed(material)) {
                        vertex_stress[adj_vertex].is_fractured = true;
                        // Recursively propagate (depth-limited to prevent infinite recursion)
                        // Real version uses GPU compute
                    }
                }
            }
        }
    }

    /**
     * @brief Add load-bearing edge between vertices
     *
     * @param vertex_a First vertex
     * @param vertex_b Second vertex
     * @param load_capacity Max load edge can support (Newtons)
     * @param is_critical Is this edge critical for structural integrity?
     */
    void add_load_bearing_edge(
        uint32_t vertex_a,
        uint32_t vertex_b,
        float load_capacity,
        bool is_critical = false
    ) {
        load_bearing_edges.push_back(LoadBearingEdge{
            vertex_a, vertex_b, load_capacity, 0.0f, is_critical
        });
    }

    /**
     * @brief Get material for vertex
     */
    const physics::StructuralMaterial& get_material(uint32_t vertex_index) const {
        return materials[vertex_index % materials.size()];
    }

    /**
     * @brief Get stress state for vertex
     */
    const StressState& get_stress(uint32_t vertex_index) const {
        return vertex_stress[vertex_index];
    }

    /**
     * @brief Reset all stress states (called per frame before recalculation)
     */
    void reset_stress() {
        for (auto& stress : vertex_stress) {
            stress.reset();
        }
    }
};

} // namespace lore::ecs