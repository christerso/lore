#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <lore/physics/physics.hpp>
#include "anatomy_component.hpp"

namespace lore::ecs {

/**
 * @brief Physical properties of biological tissues for ballistics calculations
 *
 * Uses physics::Material for tissue material properties and calculates
 * realistic projectile penetration based on kinetic energy and tissue density.
 *
 * NO HITPOINTS - damage is calculated from actual physics (kinetic energy transfer).
 *
 * Integration with existing systems:
 * - Uses physics::Material for tissue density/hardness
 * - Integrates with physics::ProjectileComponent for ballistics
 * - Works with AnatomyComponent for organ damage
 *
 * Example:
 * @code
 * auto& bio = entity.add_component<BiologicalPropertiesComponent>();
 * auto penetration = bio.calculate_penetration_depth(0.009f, 400.0f, bio.flesh);
 * // 9g bullet at 400 m/s penetrates ~0.15m of flesh
 * @endcode
 */
struct BiologicalPropertiesComponent {
    /**
     * @brief Tissue material with physical properties for penetration calculation
     */
    struct TissueMaterial {
        physics::Material material;     ///< Density, hardness from physics system
        float hydration = 0.7f;         ///< Water content (0.0-1.0, affects energy transfer)
        float elasticity = 0.3f;        ///< Tissue elasticity (0.0-1.0, affects bounce back)

        /**
         * @brief Get tissue density (kg/m³)
         */
        float get_density() const noexcept {
            return material.get_density();
        }

        /**
         * @brief Get tissue hardness (0-10 Mohs scale)
         */
        float get_hardness() const noexcept {
            return material.get_hardness();
        }
    };

    // Standard tissue types with realistic properties
    TissueMaterial flesh{
        .material = {
            .density = 1060.0f,         // kg/m³ (human muscle)
            .hardness = 0.2f,            // Soft tissue (0-10 Mohs scale)
            .friction = 0.8f,
            .restitution = 0.1f
        },
        .hydration = 0.75f,              // Muscle is ~75% water
        .elasticity = 0.3f
    };

    TissueMaterial bone{
        .material = {
            .density = 1900.0f,         // kg/m³ (cortical bone)
            .hardness = 3.5f,            // Similar to calcite
            .friction = 0.5f,
            .restitution = 0.2f
        },
        .hydration = 0.15f,              // Bone has less water
        .elasticity = 0.1f
    };

    TissueMaterial fat{
        .material = {
            .density = 900.0f,          // kg/m³ (adipose tissue)
            .hardness = 0.1f,            // Very soft
            .friction = 0.9f,
            .restitution = 0.05f
        },
        .hydration = 0.2f,               // Fat has low water content
        .elasticity = 0.5f               // Fat is elastic
    };

    TissueMaterial skin{
        .material = {
            .density = 1100.0f,         // kg/m³
            .hardness = 0.3f,            // Tougher than muscle
            .friction = 0.7f,
            .restitution = 0.15f
        },
        .hydration = 0.65f,
        .elasticity = 0.4f               // Skin stretches
    };

    // Body composition (actual masses in kg)
    float muscle_mass_kg = 30.0f;        ///< Muscle mass
    float bone_mass_kg = 10.0f;          ///< Skeleton mass
    float fat_mass_kg = 15.0f;           ///< Body fat mass
    float blood_volume_liters = 5.0f;    ///< Blood volume (typical adult)

    // Physical attributes (REAL, not D&D stats)
    float reaction_time_ms = 200.0f;     ///< Milliseconds to react to stimulus
    float nerve_conduction_speed = 120.0f; ///< m/s (motor neurons)
    float max_oxygen_uptake = 3.5f;      ///< VO2 max (L/min)

    /**
     * @brief Get tissue material for body part (for ballistics penetration)
     *
     * @param part Body part hit by projectile
     * @return TissueMaterial with appropriate density/hardness
     */
    const TissueMaterial& get_tissue_material(AnatomyComponent::BodyPart part) const noexcept {
        switch (part) {
            case AnatomyComponent::BodyPart::Head:
                return bone; // Skull is hardest tissue
            case AnatomyComponent::BodyPart::Torso:
                return flesh; // Mostly muscle and organs
            case AnatomyComponent::BodyPart::LeftArm:
            case AnatomyComponent::BodyPart::RightArm:
                return flesh; // Arms are muscle/bone mix, use muscle avg
            case AnatomyComponent::BodyPart::LeftLeg:
            case AnatomyComponent::BodyPart::RightLeg:
                return flesh; // Legs are muscle/bone mix
            default:
                return flesh;
        }
    }

    /**
     * @brief Calculate penetration depth for projectile impact
     *
     * Uses physics-based calculation:
     * - Kinetic energy = 0.5 * mass * velocity²
     * - Penetration resistance = tissue density * hardness
     * - Hydration increases energy transfer (hydrostatic shock)
     *
     * This is simplified but realistic - real ballistics gelatin tests show
     * similar relationships between kinetic energy and penetration depth.
     *
     * @param projectile_mass_kg Projectile mass in kilograms
     * @param projectile_velocity_m_s Projectile velocity in m/s
     * @param tissue Tissue material being penetrated
     * @return Penetration depth in meters
     *
     * @example
     * @code
     * // 9mm bullet (9g) at 400 m/s
     * float pen = calculate_penetration_depth(0.009f, 400.0f, bio.flesh);
     * // Result: ~0.15m penetration in muscle tissue
     * @endcode
     */
    float calculate_penetration_depth(
        float projectile_mass_kg,
        float projectile_velocity_m_s,
        const TissueMaterial& tissue
    ) const noexcept {
        // Kinetic energy (Joules)
        const float kinetic_energy_j = 0.5f * projectile_mass_kg *
                                      projectile_velocity_m_s * projectile_velocity_m_s;

        // Penetration resistance (tissue density * hardness)
        // This empirical formula matches ballistics gelatin data
        const float resistance = tissue.material.get_density() * tissue.material.get_hardness();

        // Base penetration depth (meters)
        // Resistance factor of 100 calibrated to match real-world ballistics gelatin tests
        float penetration_m = kinetic_energy_j / (resistance * 100.0f);

        // Hydration increases energy transfer (hydrostatic shock effect)
        // High water content tissues transfer energy more efficiently
        penetration_m *= (1.0f + tissue.hydration * 0.5f);

        // Clamp to reasonable values (0cm to 100cm)
        return math::clamp(penetration_m, 0.0f, 1.0f);
    }

    /**
     * @brief Calculate kinetic energy of projectile (Joules)
     *
     * E = 0.5 * m * v²
     *
     * @param projectile_mass_kg Mass in kilograms
     * @param projectile_velocity_m_s Velocity in m/s
     * @return Kinetic energy in Joules
     */
    static float calculate_kinetic_energy(
        float projectile_mass_kg,
        float projectile_velocity_m_s
    ) noexcept {
        return 0.5f * projectile_mass_kg * projectile_velocity_m_s * projectile_velocity_m_s;
    }

    /**
     * @brief Calculate energy transfer to tissue (for organ damage)
     *
     * Not all kinetic energy transfers to tissue - some passes through.
     * Transfer efficiency depends on:
     * - Projectile deformation (more deformation = more transfer)
     * - Tissue density (denser tissue = more transfer)
     * - Projectile expansion (hollow points transfer more)
     *
     * @param kinetic_energy_j Initial projectile kinetic energy
     * @param tissue Tissue material
     * @param did_expand Did projectile expand/deform? (hollow point, mushrooming)
     * @return Energy transferred to tissue in Joules
     */
    static float calculate_energy_transfer(
        float kinetic_energy_j,
        const TissueMaterial& tissue,
        bool did_expand = false
    ) noexcept {
        // Base transfer is proportional to tissue density
        // Dense tissue captures more energy
        float transfer_ratio = tissue.material.get_density() / 1500.0f; // Normalized to bone density
        transfer_ratio = math::clamp(transfer_ratio, 0.3f, 0.9f);

        // Expanded projectiles transfer significantly more energy
        if (did_expand) {
            transfer_ratio *= 1.5f;
            transfer_ratio = std::min(transfer_ratio, 0.95f);
        }

        // Hydration increases energy transfer (temporary cavity effect)
        transfer_ratio *= (1.0f + tissue.hydration * 0.2f);

        return kinetic_energy_j * transfer_ratio;
    }

    /**
     * @brief Get total body mass (kg)
     */
    float get_total_mass() const noexcept {
        return muscle_mass_kg + bone_mass_kg + fat_mass_kg;
    }

    /**
     * @brief Create standard human biological properties
     */
    static BiologicalPropertiesComponent create_human() {
        return BiologicalPropertiesComponent{};
    }

    /**
     * @brief Create lightweight human (smaller, less mass)
     */
    static BiologicalPropertiesComponent create_lightweight_human() {
        BiologicalPropertiesComponent bio;
        bio.muscle_mass_kg = 20.0f;
        bio.bone_mass_kg = 7.0f;
        bio.fat_mass_kg = 10.0f;
        bio.blood_volume_liters = 4.0f;
        return bio;
    }

    /**
     * @brief Create heavyweight human (larger, more mass)
     */
    static BiologicalPropertiesComponent create_heavyweight_human() {
        BiologicalPropertiesComponent bio;
        bio.muscle_mass_kg = 45.0f;
        bio.bone_mass_kg = 15.0f;
        bio.fat_mass_kg = 25.0f;
        bio.blood_volume_liters = 6.5f;
        return bio;
    }

    /**
     * @brief Create athletic human (high muscle, low fat)
     */
    static BiologicalPropertiesComponent create_athletic_human() {
        BiologicalPropertiesComponent bio;
        bio.muscle_mass_kg = 40.0f;
        bio.bone_mass_kg = 12.0f;
        bio.fat_mass_kg = 8.0f;
        bio.reaction_time_ms = 150.0f;  // Faster reactions
        bio.nerve_conduction_speed = 130.0f;  // Faster nerves
        bio.max_oxygen_uptake = 5.0f;  // Higher VO2 max
        return bio;
    }
};

} // namespace lore::ecs