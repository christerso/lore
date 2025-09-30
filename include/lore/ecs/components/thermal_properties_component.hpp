#pragma once

#include "lore/math/vec3.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief Phase states of matter
 */
enum class PhaseState : uint8_t {
    Solid,
    Liquid,
    Gas,
    Plasma
};

/**
 * @brief Thermal properties of a material
 *
 * Defines how a material responds to heat transfer, temperature changes,
 * and phase transitions. Based on real thermodynamic properties.
 *
 * Clean API design:
 * - Real SI units (Joules, Kelvin, Watts)
 * - Physically accurate default values
 * - Easy integration with ballistics (kinetic → thermal energy)
 * - Phase transition support for melting/boiling/burning
 *
 * Example usage:
 * @code
 * // Create steel material
 * auto steel = ThermalPropertiesComponent::create_steel();
 *
 * // Apply heat from ballistic impact
 * float kinetic_energy_j = 500.0f; // 500 Joules from bullet
 * float mass_kg = 0.1f; // 100 grams of steel
 * steel.add_thermal_energy(kinetic_energy_j, mass_kg);
 *
 * // Check if melted
 * if (steel.current_temperature_k > steel.melting_point_k) {
 *     // Material has melted
 * }
 * @endcode
 */
struct ThermalPropertiesComponent {
    // Current thermodynamic state
    float current_temperature_k = 293.15f;      ///< Current temperature (Kelvin), default 20°C
    PhaseState current_phase = PhaseState::Solid; ///< Current phase state
    float accumulated_energy_j = 0.0f;          ///< Total thermal energy absorbed (Joules)

    // Material thermal properties (SI units)
    float specific_heat_capacity_j_kg_k = 500.0f;  ///< Energy per kg per Kelvin (J/(kg·K))
    float thermal_conductivity_w_m_k = 50.0f;      ///< Heat conduction rate (W/(m·K))
    float thermal_diffusivity_m2_s = 1.0e-5f;      ///< Heat diffusion rate (m²/s)

    // Phase transition temperatures (Kelvin)
    float melting_point_k = 1811.0f;               ///< Solid → Liquid transition (default: steel)
    float boiling_point_k = 3134.0f;               ///< Liquid → Gas transition
    float ignition_temperature_k = 0.0f;           ///< Auto-ignition temp (0 = not flammable)

    // Phase transition energies (Joules per kilogram)
    float latent_heat_fusion_j_kg = 270000.0f;    ///< Energy to melt (J/kg)
    float latent_heat_vaporization_j_kg = 6100000.0f; ///< Energy to boil (J/kg)

    // Heat transfer coefficients
    float emissivity = 0.8f;                       ///< Thermal radiation efficiency (0-1)
    float absorptivity = 0.8f;                     ///< Heat absorption efficiency (0-1)
    float surface_area_m2 = 1.0f;                  ///< Surface area for heat transfer

    // State tracking
    bool is_burning = false;                       ///< Is material currently on fire?
    float time_since_ignition_s = 0.0f;           ///< Duration of combustion
    float cooldown_rate_k_s = 1.0f;               ///< Passive cooling rate (K/s)

    /**
     * @brief Add thermal energy to material
     *
     * Converts energy (e.g., from kinetic impact) to temperature increase.
     * Automatically handles phase transitions.
     *
     * Formula: ΔT = Q / (m * c)
     * Where Q = energy, m = mass, c = specific heat capacity
     *
     * @param energy_joules Energy to add (Joules)
     * @param mass_kg Material mass (kilograms)
     * @return New temperature after energy addition (Kelvin)
     */
    float add_thermal_energy(float energy_joules, float mass_kg) noexcept {
        accumulated_energy_j += energy_joules;

        // Calculate temperature increase: ΔT = Q / (m * c)
        float delta_temperature_k = energy_joules / (mass_kg * specific_heat_capacity_j_kg_k);
        current_temperature_k += delta_temperature_k;

        // Check for phase transitions
        update_phase_state(mass_kg);

        return current_temperature_k;
    }

    /**
     * @brief Convert kinetic energy to heat
     *
     * Convenience method for ballistic impacts.
     * Assumes energy conversion efficiency (default 80% → heat, 20% → deformation).
     *
     * @param projectile_mass_kg Projectile mass (kg)
     * @param projectile_velocity_m_s Projectile velocity (m/s)
     * @param target_mass_kg Target material mass (kg)
     * @param conversion_efficiency Heat conversion ratio (0-1), default 0.8
     * @return Temperature increase (Kelvin)
     */
    float apply_kinetic_heating(
        float projectile_mass_kg,
        float projectile_velocity_m_s,
        float target_mass_kg,
        float conversion_efficiency = 0.8f
    ) noexcept {
        // Kinetic energy: E = 0.5 * m * v²
        float kinetic_energy_j = 0.5f * projectile_mass_kg *
                                projectile_velocity_m_s * projectile_velocity_m_s;

        // Convert to thermal energy (80% default, rest goes to deformation)
        float thermal_energy_j = kinetic_energy_j * conversion_efficiency;

        return add_thermal_energy(thermal_energy_j, target_mass_kg);
    }

    /**
     * @brief Calculate heat conduction to/from neighbor
     *
     * Fourier's law of heat conduction:
     * Q = k * A * ΔT / d
     * Where k = conductivity, A = area, ΔT = temp difference, d = distance
     *
     * @param neighbor_temp_k Neighbor's temperature (K)
     * @param contact_area_m2 Contact surface area (m²)
     * @param distance_m Distance between centers (m)
     * @param delta_time_s Time step (seconds)
     * @return Heat transferred (Joules, positive = gained heat)
     */
    float calculate_conduction(
        float neighbor_temp_k,
        float contact_area_m2,
        float distance_m,
        float delta_time_s
    ) const noexcept {
        if (distance_m <= 0.0f) return 0.0f;

        float temp_difference_k = neighbor_temp_k - current_temperature_k;

        // Fourier's law: Q = k * A * ΔT * t / d
        float heat_transfer_j = thermal_conductivity_w_m_k * contact_area_m2 *
                               temp_difference_k * delta_time_s / distance_m;

        return heat_transfer_j;
    }

    /**
     * @brief Calculate radiative heat loss (Stefan-Boltzmann law)
     *
     * P = ε * σ * A * (T⁴ - T_ambient⁴)
     * Where σ = Stefan-Boltzmann constant = 5.67e-8 W/(m²·K⁴)
     *
     * @param ambient_temp_k Ambient temperature (K)
     * @param delta_time_s Time step (seconds)
     * @return Heat lost via radiation (Joules, negative value)
     */
    float calculate_radiation(float ambient_temp_k, float delta_time_s) const noexcept {
        constexpr float STEFAN_BOLTZMANN = 5.67e-8f; // W/(m²·K⁴)

        float t4 = current_temperature_k * current_temperature_k *
                   current_temperature_k * current_temperature_k;
        float t_amb4 = ambient_temp_k * ambient_temp_k *
                       ambient_temp_k * ambient_temp_k;

        // Power radiated: P = ε * σ * A * (T⁴ - T_ambient⁴)
        float power_w = emissivity * STEFAN_BOLTZMANN * surface_area_m2 * (t4 - t_amb4);

        // Energy over time: E = P * t
        return power_w * delta_time_s;
    }

    /**
     * @brief Apply passive cooling
     *
     * Materials naturally cool toward ambient temperature.
     *
     * @param ambient_temp_k Ambient temperature (K)
     * @param delta_time_s Time step (seconds)
     */
    void apply_cooling(float ambient_temp_k, float delta_time_s) noexcept {
        if (current_temperature_k > ambient_temp_k) {
            float cooling_k = cooldown_rate_k_s * delta_time_s;
            current_temperature_k -= cooling_k;

            if (current_temperature_k < ambient_temp_k) {
                current_temperature_k = ambient_temp_k;
            }
        }
    }

    /**
     * @brief Update phase state based on temperature
     *
     * Handles solid ↔ liquid ↔ gas transitions with latent heat.
     *
     * @param mass_kg Material mass for latent heat calculations
     */
    void update_phase_state(float mass_kg) noexcept {
        // Check for melting (solid → liquid)
        if (current_phase == PhaseState::Solid &&
            current_temperature_k >= melting_point_k) {

            // Absorb latent heat of fusion
            float energy_required_j = latent_heat_fusion_j_kg * mass_kg;
            if (accumulated_energy_j >= energy_required_j) {
                current_phase = PhaseState::Liquid;
                accumulated_energy_j -= energy_required_j;
            }
        }

        // Check for boiling (liquid → gas)
        else if (current_phase == PhaseState::Liquid &&
                 current_temperature_k >= boiling_point_k) {

            float energy_required_j = latent_heat_vaporization_j_kg * mass_kg;
            if (accumulated_energy_j >= energy_required_j) {
                current_phase = PhaseState::Gas;
                accumulated_energy_j -= energy_required_j;
            }
        }

        // Check for freezing (liquid → solid)
        else if (current_phase == PhaseState::Liquid &&
                 current_temperature_k < melting_point_k) {
            current_phase = PhaseState::Solid;
        }

        // Check for condensation (gas → liquid)
        else if (current_phase == PhaseState::Gas &&
                 current_temperature_k < boiling_point_k) {
            current_phase = PhaseState::Liquid;
        }
    }

    /**
     * @brief Check if material should ignite
     *
     * @return true if temperature exceeds ignition point
     */
    bool should_ignite() const noexcept {
        return ignition_temperature_k > 0.0f &&
               current_temperature_k >= ignition_temperature_k &&
               !is_burning;
    }

    /**
     * @brief Convert Celsius to Kelvin
     */
    static constexpr float celsius_to_kelvin(float celsius) noexcept {
        return celsius + 273.15f;
    }

    /**
     * @brief Convert Kelvin to Celsius
     */
    static constexpr float kelvin_to_celsius(float kelvin) noexcept {
        return kelvin - 273.15f;
    }

    // ============================================================================
    // Material Presets (Real Values)
    // ============================================================================

    /**
     * @brief Steel thermal properties
     */
    static ThermalPropertiesComponent create_steel() noexcept {
        return {
            .current_temperature_k = 293.15f,  // 20°C
            .specific_heat_capacity_j_kg_k = 500.0f,
            .thermal_conductivity_w_m_k = 50.0f,
            .thermal_diffusivity_m2_s = 1.2e-5f,
            .melting_point_k = 1811.0f,        // 1538°C
            .boiling_point_k = 3134.0f,        // 2861°C
            .ignition_temperature_k = 0.0f,    // Not flammable
            .latent_heat_fusion_j_kg = 270000.0f,
            .latent_heat_vaporization_j_kg = 6100000.0f,
            .emissivity = 0.8f,
            .absorptivity = 0.8f
        };
    }

    /**
     * @brief Wood thermal properties (pine)
     */
    static ThermalPropertiesComponent create_wood() noexcept {
        return {
            .current_temperature_k = 293.15f,
            .specific_heat_capacity_j_kg_k = 1700.0f,
            .thermal_conductivity_w_m_k = 0.12f, // Very low conductivity
            .thermal_diffusivity_m2_s = 8.2e-8f,
            .melting_point_k = 0.0f,             // Decomposes before melting
            .boiling_point_k = 0.0f,
            .ignition_temperature_k = 573.15f,   // 300°C auto-ignition
            .latent_heat_fusion_j_kg = 0.0f,
            .latent_heat_vaporization_j_kg = 0.0f,
            .emissivity = 0.9f,
            .absorptivity = 0.9f,
            .cooldown_rate_k_s = 0.5f
        };
    }

    /**
     * @brief Concrete thermal properties
     */
    static ThermalPropertiesComponent create_concrete() noexcept {
        return {
            .current_temperature_k = 293.15f,
            .specific_heat_capacity_j_kg_k = 880.0f,
            .thermal_conductivity_w_m_k = 1.4f,
            .thermal_diffusivity_m2_s = 6.5e-7f,
            .melting_point_k = 1923.15f,         // 1650°C
            .boiling_point_k = 0.0f,
            .ignition_temperature_k = 0.0f,      // Not flammable
            .latent_heat_fusion_j_kg = 0.0f,
            .latent_heat_vaporization_j_kg = 0.0f,
            .emissivity = 0.9f,
            .absorptivity = 0.7f,
            .cooldown_rate_k_s = 2.0f
        };
    }

    /**
     * @brief Gasoline thermal properties
     */
    static ThermalPropertiesComponent create_gasoline() noexcept {
        return {
            .current_temperature_k = 293.15f,
            .current_phase = PhaseState::Liquid,
            .specific_heat_capacity_j_kg_k = 2220.0f,
            .thermal_conductivity_w_m_k = 0.14f,
            .thermal_diffusivity_m2_s = 8.0e-8f,
            .melting_point_k = 213.15f,          // -60°C
            .boiling_point_k = 423.15f,          // 150°C
            .ignition_temperature_k = 553.15f,   // 280°C auto-ignition
            .latent_heat_fusion_j_kg = 0.0f,
            .latent_heat_vaporization_j_kg = 350000.0f,
            .emissivity = 0.95f,
            .absorptivity = 0.95f,
            .cooldown_rate_k_s = 0.2f
        };
    }

    /**
     * @brief Aluminum thermal properties
     */
    static ThermalPropertiesComponent create_aluminum() noexcept {
        return {
            .current_temperature_k = 293.15f,
            .specific_heat_capacity_j_kg_k = 900.0f,
            .thermal_conductivity_w_m_k = 205.0f, // Very high conductivity
            .thermal_diffusivity_m2_s = 8.4e-5f,
            .melting_point_k = 933.15f,           // 660°C
            .boiling_point_k = 2743.15f,          // 2470°C
            .ignition_temperature_k = 0.0f,
            .latent_heat_fusion_j_kg = 397000.0f,
            .latent_heat_vaporization_j_kg = 10500000.0f,
            .emissivity = 0.05f,                  // Very reflective
            .absorptivity = 0.1f,
            .cooldown_rate_k_s = 5.0f
        };
    }
};

} // namespace lore::ecs