#pragma once

#include <lore/physics/physics.hpp>
#include <lore/math/math.hpp>

namespace lore::physics {

/**
 * @brief Structural material properties for load-bearing and fracture physics
 *
 * Extends physics::Material with structural engineering properties:
 * - Tensile/compressive/shear strength (how much force before breaking)
 * - Elastic modulus (stiffness)
 * - Fracture behavior (brittle vs ductile)
 *
 * Uses REAL material science values (Pascals, GPa, etc.)
 *
 * Integration with existing systems:
 * - Extends physics::Material (density, hardness, friction)
 * - Used by WorldMeshMaterialComponent for structural calculations
 * - Used by FractureSystem for material-specific breakage patterns
 *
 * Example:
 * @code
 * auto concrete = StructuralMaterial::create_concrete();
 * // Weak in tension (3 MPa), strong in compression (30 MPa)
 * // Brittle - shatters rather than bending
 * @endcode
 */
struct StructuralMaterial {
    /// Base physical properties (inherited from physics::Material)
    Material base_material;

    /// Strength Properties (Pascals - force per area)
    float tensile_strength_pa = 100e6f;      ///< Pulling force before break (Pa)
    float compressive_strength_pa = 100e6f;  ///< Crushing force before break (Pa)
    float shear_strength_pa = 50e6f;         ///< Sliding force before break (Pa)
    float yield_strength_pa = 80e6f;         ///< Permanent deformation point (Pa)

    /// Elastic Properties (material stiffness and deformation)
    float youngs_modulus_pa = 200e9f;        ///< Stiffness (stress/strain ratio) (Pa)
    float poissons_ratio = 0.3f;             ///< Transverse strain (0-0.5, dimensionless)
    float bulk_modulus_pa = 160e9f;          ///< Resistance to uniform compression (Pa)

    /// Fracture Properties (how material breaks)
    float fracture_toughness = 50.0f;        ///< Resistance to crack propagation (MPa·m^1/2)
    float critical_stress_intensity = 45.0f; ///< Stress at crack tip to propagate
    bool is_brittle = false;                 ///< Brittle (shatters) vs ductile (bends)

    /// Load Bearing Limits
    float max_stress_pa = 120e6f;            ///< Maximum stress before failure (Pa)
    float fatigue_limit_pa = 50e6f;          ///< Stress sustainable indefinitely (Pa)

    /**
     * @brief Get density from base material (kg/m³)
     */
    float get_density() const noexcept {
        return base_material.get_density();
    }

    /**
     * @brief Get hardness from base material (0-10 Mohs scale)
     */
    float get_hardness() const noexcept {
        return base_material.get_hardness();
    }

    /**
     * @brief Get friction from base material
     */
    float get_friction() const noexcept {
        return base_material.get_friction();
    }

    /**
     * @brief Check if stress exceeds material strength
     *
     * @param tensile_stress Tensile stress (Pa)
     * @param compressive_stress Compressive stress (Pa)
     * @param shear_stress Shear stress (Pa)
     * @return true if material will fail under these stresses
     */
    bool will_fail(
        float tensile_stress,
        float compressive_stress,
        float shear_stress
    ) const noexcept {
        return tensile_stress > tensile_strength_pa ||
               compressive_stress > compressive_strength_pa ||
               shear_stress > shear_strength_pa;
    }

    /**
     * @brief Calculate von Mises stress (combined stress metric)
     *
     * Used to determine if material will yield under complex stress states.
     *
     * @param sigma_x Normal stress in x direction (Pa)
     * @param sigma_y Normal stress in y direction (Pa)
     * @param sigma_z Normal stress in z direction (Pa)
     * @param tau_xy Shear stress in xy plane (Pa)
     * @param tau_yz Shear stress in yz plane (Pa)
     * @param tau_zx Shear stress in zx plane (Pa)
     * @return von Mises stress (Pa)
     */
    static float calculate_von_mises_stress(
        float sigma_x, float sigma_y, float sigma_z,
        float tau_xy, float tau_yz, float tau_zx
    ) noexcept {
        // von Mises stress formula (for 3D stress state)
        const float term1 = (sigma_x - sigma_y) * (sigma_x - sigma_y);
        const float term2 = (sigma_y - sigma_z) * (sigma_y - sigma_z);
        const float term3 = (sigma_z - sigma_x) * (sigma_z - sigma_x);
        const float shear_term = 6.0f * (tau_xy * tau_xy + tau_yz * tau_yz + tau_zx * tau_zx);

        return std::sqrt(0.5f * (term1 + term2 + term3 + shear_term));
    }

    //=====================================================================
    // Material Presets (Real-World Values)
    //=====================================================================

    /**
     * @brief Create wood structural material (pine)
     *
     * Properties:
     * - Moderate strength in tension (40 MPa)
     * - Weaker in compression (30 MPa)
     * - Ductile - bends before breaking
     * - Light (600 kg/m³)
     */
    static StructuralMaterial create_wood() {
        return StructuralMaterial{
            .base_material = {
                .density = 600.0f,              // kg/m³ (pine)
                .friction = 0.4f,
                .restitution = 0.3f,
                .hardness = 2.0f                // Soft wood (Mohs scale)
            },
            .tensile_strength_pa = 40e6f,       // 40 MPa (parallel to grain)
            .compressive_strength_pa = 30e6f,   // 30 MPa
            .shear_strength_pa = 5e6f,          // 5 MPa (weak perpendicular to grain)
            .yield_strength_pa = 25e6f,
            .youngs_modulus_pa = 11e9f,         // 11 GPa
            .poissons_ratio = 0.3f,
            .bulk_modulus_pa = 10e9f,
            .fracture_toughness = 0.5f,
            .critical_stress_intensity = 0.4f,
            .is_brittle = false,                // Wood bends before breaking
            .max_stress_pa = 35e6f,
            .fatigue_limit_pa = 15e6f
        };
    }

    /**
     * @brief Create concrete structural material
     *
     * Properties:
     * - Very weak in tension (3 MPa) - needs reinforcement
     * - Strong in compression (30 MPa)
     * - Brittle - shatters rather than bending
     * - Heavy (2400 kg/m³)
     */
    static StructuralMaterial create_concrete() {
        return StructuralMaterial{
            .base_material = {
                .density = 2400.0f,             // kg/m³
                .friction = 0.6f,
                .restitution = 0.1f,
                .hardness = 7.0f                // Hard material
            },
            .tensile_strength_pa = 3e6f,        // 3 MPa (VERY weak in tension)
            .compressive_strength_pa = 30e6f,   // 30 MPa (strong in compression)
            .shear_strength_pa = 10e6f,
            .yield_strength_pa = 25e6f,
            .youngs_modulus_pa = 30e9f,         // 30 GPa
            .poissons_ratio = 0.2f,
            .bulk_modulus_pa = 25e9f,
            .fracture_toughness = 0.2f,         // Low - cracks propagate easily
            .critical_stress_intensity = 0.15f,
            .is_brittle = true,                 // Concrete shatters
            .max_stress_pa = 25e6f,
            .fatigue_limit_pa = 10e6f
        };
    }

    /**
     * @brief Create steel structural material (mild steel)
     *
     * Properties:
     * - Very strong in tension (400 MPa)
     * - Very strong in compression (400 MPa)
     * - Ductile - deforms before breaking
     * - Very heavy (7850 kg/m³)
     * - Toughest material
     */
    static StructuralMaterial create_steel() {
        return StructuralMaterial{
            .base_material = {
                .density = 7850.0f,             // kg/m³ (mild steel)
                .friction = 0.7f,
                .restitution = 0.4f,
                .hardness = 4.5f                // Mohs 4-5
            },
            .tensile_strength_pa = 400e6f,      // 400 MPa (very strong)
            .compressive_strength_pa = 400e6f,  // 400 MPa
            .shear_strength_pa = 250e6f,        // 250 MPa
            .yield_strength_pa = 250e6f,
            .youngs_modulus_pa = 200e9f,        // 200 GPa (very stiff)
            .poissons_ratio = 0.3f,
            .bulk_modulus_pa = 160e9f,
            .fracture_toughness = 50.0f,        // Very tough
            .critical_stress_intensity = 45.0f,
            .is_brittle = false,                // Steel deforms before breaking
            .max_stress_pa = 350e6f,
            .fatigue_limit_pa = 200e6f
        };
    }

    /**
     * @brief Create glass structural material
     *
     * Properties:
     * - Moderate tension strength (50 MPa)
     * - Extremely strong in compression (1000 MPa)
     * - Extremely brittle - shatters spectacularly
     * - Heavy (2500 kg/m³)
     */
    static StructuralMaterial create_glass() {
        return StructuralMaterial{
            .base_material = {
                .density = 2500.0f,             // kg/m³
                .friction = 0.4f,
                .restitution = 0.6f,            // Glass is bouncy
                .hardness = 6.0f                // Hard but brittle
            },
            .tensile_strength_pa = 50e6f,       // 50 MPa
            .compressive_strength_pa = 1000e6f, // 1 GPa (very strong in compression)
            .shear_strength_pa = 35e6f,
            .yield_strength_pa = 45e6f,
            .youngs_modulus_pa = 70e9f,         // 70 GPa
            .poissons_ratio = 0.22f,
            .bulk_modulus_pa = 40e9f,
            .fracture_toughness = 0.7f,         // Low - cracks easily
            .critical_stress_intensity = 0.6f,
            .is_brittle = true,                 // Glass shatters spectacularly
            .max_stress_pa = 45e6f,
            .fatigue_limit_pa = 20e6f
        };
    }

    /**
     * @brief Create brick structural material
     *
     * Properties:
     * - Weak in tension (2 MPa)
     * - Moderate compression (20 MPa)
     * - Brittle - cracks and crumbles
     * - Heavy (1800 kg/m³)
     */
    static StructuralMaterial create_brick() {
        return StructuralMaterial{
            .base_material = {
                .density = 1800.0f,             // kg/m³
                .friction = 0.7f,
                .restitution = 0.2f,
                .hardness = 4.0f
            },
            .tensile_strength_pa = 2e6f,        // 2 MPa (very weak in tension)
            .compressive_strength_pa = 20e6f,   // 20 MPa
            .shear_strength_pa = 8e6f,
            .yield_strength_pa = 15e6f,
            .youngs_modulus_pa = 15e9f,         // 15 GPa
            .poissons_ratio = 0.15f,
            .bulk_modulus_pa = 12e9f,
            .fracture_toughness = 0.3f,
            .critical_stress_intensity = 0.25f,
            .is_brittle = true,                 // Brick crumbles
            .max_stress_pa = 18e6f,
            .fatigue_limit_pa = 8e6f
        };
    }

    /**
     * @brief Create stone structural material (granite)
     *
     * Properties:
     * - Moderate tension (10 MPa)
     * - Very strong compression (200 MPa)
     * - Brittle but tough
     * - Very heavy (2700 kg/m³)
     */
    static StructuralMaterial create_stone() {
        return StructuralMaterial{
            .base_material = {
                .density = 2700.0f,             // kg/m³ (granite)
                .friction = 0.6f,
                .restitution = 0.3f,
                .hardness = 6.5f                // Hard stone
            },
            .tensile_strength_pa = 10e6f,       // 10 MPa
            .compressive_strength_pa = 200e6f,  // 200 MPa (very strong)
            .shear_strength_pa = 20e6f,
            .yield_strength_pa = 180e6f,
            .youngs_modulus_pa = 50e9f,         // 50 GPa
            .poissons_ratio = 0.25f,
            .bulk_modulus_pa = 40e9f,
            .fracture_toughness = 1.5f,         // Tougher than concrete/brick
            .critical_stress_intensity = 1.2f,
            .is_brittle = true,                 // Still brittle
            .max_stress_pa = 180e6f,
            .fatigue_limit_pa = 80e6f
        };
    }

    /**
     * @brief Create aluminum structural material
     *
     * Properties:
     * - Strong in tension (300 MPa)
     * - Strong in compression (300 MPa)
     * - Ductile - bends before breaking
     * - Light for metal (2700 kg/m³)
     */
    static StructuralMaterial create_aluminum() {
        return StructuralMaterial{
            .base_material = {
                .density = 2700.0f,             // kg/m³
                .friction = 0.6f,
                .restitution = 0.5f,
                .hardness = 2.75f               // Softer than steel
            },
            .tensile_strength_pa = 300e6f,      // 300 MPa
            .compressive_strength_pa = 300e6f,
            .shear_strength_pa = 180e6f,
            .yield_strength_pa = 200e6f,
            .youngs_modulus_pa = 70e9f,         // 70 GPa
            .poissons_ratio = 0.33f,
            .bulk_modulus_pa = 75e9f,
            .fracture_toughness = 30.0f,
            .critical_stress_intensity = 28.0f,
            .is_brittle = false,                // Aluminum is ductile
            .max_stress_pa = 280e6f,
            .fatigue_limit_pa = 120e6f
        };
    }
};

} // namespace lore::physics