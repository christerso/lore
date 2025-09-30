#pragma once

#include "lore/chemistry/periodic_table.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief Chemical composition of a material
 *
 * Defines what elements make up a material and their proportions.
 * Enables realistic chemical reactions, combustion, and material interactions.
 *
 * Clean API with INI-configurable parameters:
 * - Element proportions for custom materials
 * - Reaction rates and activation energies
 * - Oxidation behavior
 * - Thermal decomposition products
 *
 * Example usage:
 * @code
 * // Create wood (cellulose: C6H10O5)
 * auto wood = ChemicalCompositionComponent::create_wood();
 *
 * // Check if can combust with oxygen
 * if (wood.can_oxidize()) {
 *     // Wood + O2 → CO2 + H2O + heat
 * }
 *
 * // Get combustion products
 * auto products = wood.get_combustion_products();
 * @endcode
 */
struct ChemicalCompositionComponent {
    /**
     * @brief Element proportion in composition
     */
    struct ElementProportion {
        std::string element_symbol;      ///< Chemical symbol (e.g., "C", "H", "O")
        float molar_ratio = 1.0f;       ///< Moles of element per formula unit
        float mass_fraction = 0.0f;     ///< Mass percentage (0-1)
    };

    /**
     * @brief Chemical reaction definition
     */
    struct Reaction {
        std::string name;                           ///< Reaction name
        std::vector<std::string> reactants;         ///< Required reactants
        std::vector<std::string> products;          ///< Produced compounds
        float activation_energy_kj_mol = 0.0f;     ///< Energy to start reaction
        float heat_release_kj_mol = 0.0f;          ///< Exothermic (+) or endothermic (-)
        float reaction_rate_coeff = 1.0f;          ///< Speed multiplier
    };

    // Composition data
    std::vector<ElementProportion> elements;
    std::string chemical_formula;               ///< E.g., "C6H10O5" for cellulose

    // Oxidation properties (for combustion)
    bool is_combustible = false;
    float oxidation_rate = 1.0f;                ///< How fast it oxidizes (0-10 scale)
    float oxygen_required_mol = 0.0f;           ///< Moles of O2 per mole of compound
    float heat_of_combustion_kj_mol = 0.0f;    ///< Energy released when burned

    // Decomposition properties (thermal breakdown)
    bool can_decompose = false;
    float decomposition_temp_k = 0.0f;          ///< Temperature where breakdown occurs
    std::vector<std::string> decomposition_products; ///< What it breaks down into

    // Reaction definitions
    std::vector<Reaction> possible_reactions;

    // Configuration parameters (INI-configurable)
    struct Config {
        float combustion_efficiency = 0.95f;     ///< How complete the burn is (0-1)
        float soot_production_rate = 0.1f;       ///< Carbon particles per mole (0-1)
        float smoke_density_factor = 1.0f;       ///< Smoke opacity multiplier
        float co2_production_ratio = 1.0f;       ///< CO2 vs CO production (1=complete, 0=incomplete)
        float water_vapor_ratio = 1.0f;          ///< H2O vapor production
        float ash_residue_fraction = 0.05f;      ///< Solid residue remaining (0-1)
        float reaction_temp_modifier = 1.0f;     ///< Temperature effect multiplier
        bool enable_side_reactions = true;       ///< Allow complex chemistry
    } config;

    /**
     * @brief Calculate total molecular weight
     *
     * @return Molecular weight in g/mol
     */
    float get_molecular_weight() const noexcept {
        const auto& periodic_table = chemistry::PeriodicTable::get_instance();
        float total_weight = 0.0f;

        for (const auto& elem : elements) {
            const auto* element = periodic_table.get_element(elem.element_symbol);
            if (element) {
                total_weight += element->atomic_mass_amu * elem.molar_ratio;
            }
        }

        return total_weight;
    }

    /**
     * @brief Check if material can oxidize (burn)
     *
     * @return true if contains carbon/hydrogen and is combustible
     */
    bool can_oxidize() const noexcept {
        if (!is_combustible) return false;

        // Check for carbon or hydrogen (fuel elements)
        for (const auto& elem : elements) {
            if (elem.element_symbol == "C" || elem.element_symbol == "H") {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Calculate oxygen consumption rate
     *
     * @param combustion_rate_mol_s Moles of fuel burning per second
     * @return Moles of O2 consumed per second
     */
    float calculate_oxygen_consumption(float combustion_rate_mol_s) const noexcept {
        return oxygen_required_mol * combustion_rate_mol_s;
    }

    /**
     * @brief Get combustion products and their quantities
     *
     * For complete combustion:
     * CxHyOz + O2 → x*CO2 + (y/2)*H2O + heat
     *
     * @param incomplete If true, produces CO and soot instead of pure CO2
     * @return Map of product formulas to molar quantities
     */
    std::unordered_map<std::string, float> get_combustion_products(bool incomplete = false) const {
        std::unordered_map<std::string, float> products;

        if (!is_combustible) return products;

        // Count C, H, O atoms
        float carbon_moles = 0.0f;
        float hydrogen_moles = 0.0f;

        for (const auto& elem : elements) {
            if (elem.element_symbol == "C") carbon_moles = elem.molar_ratio;
            if (elem.element_symbol == "H") hydrogen_moles = elem.molar_ratio;
        }

        if (incomplete || config.combustion_efficiency < 0.9f) {
            // Incomplete combustion: produce CO and soot
            float co_ratio = 1.0f - config.co2_production_ratio;
            products["CO2"] = carbon_moles * config.co2_production_ratio;
            products["CO"] = carbon_moles * co_ratio;
            products["C"] = carbon_moles * config.soot_production_rate; // Soot
        } else {
            // Complete combustion: all C → CO2
            products["CO2"] = carbon_moles;
        }

        // Water vapor from hydrogen
        products["H2O"] = (hydrogen_moles / 2.0f) * config.water_vapor_ratio;

        // Ash residue (solid leftover)
        if (config.ash_residue_fraction > 0.0f) {
            products["Ash"] = config.ash_residue_fraction;
        }

        return products;
    }

    /**
     * @brief Calculate heat release from combustion
     *
     * @param moles_burned Moles of compound combusted
     * @return Energy released in Joules
     */
    float calculate_heat_release(float moles_burned) const noexcept {
        // Convert kJ/mol to J: multiply by 1000
        float energy_kj = heat_of_combustion_kj_mol * moles_burned * config.combustion_efficiency;
        return energy_kj * 1000.0f; // Convert to Joules
    }

    /**
     * @brief Check if can react with another composition
     *
     * @param other Other material
     * @return true if a reaction is possible
     */
    bool can_react_with(const ChemicalCompositionComponent& other) const {
        const auto& periodic_table = chemistry::PeriodicTable::get_instance();

        // Check each pair of elements for reactivity
        for (const auto& elem1 : elements) {
            for (const auto& elem2 : other.elements) {
                if (periodic_table.can_react(elem1.element_symbol, elem2.element_symbol)) {
                    return true;
                }
            }
        }

        return false;
    }

    // ============================================================================
    // Material Presets (Common Compounds)
    // ============================================================================

    /**
     * @brief Wood (cellulose): C6H10O5
     */
    static ChemicalCompositionComponent create_wood() noexcept {
        ChemicalCompositionComponent wood;
        wood.chemical_formula = "C6H10O5";
        wood.elements = {
            {"C", 6.0f, 0.444f},  // 44.4% carbon
            {"H", 10.0f, 0.062f}, // 6.2% hydrogen
            {"O", 5.0f, 0.494f}   // 49.4% oxygen
        };
        wood.is_combustible = true;
        wood.oxidation_rate = 3.0f;
        wood.oxygen_required_mol = 6.0f;              // C6H10O5 + 6O2 → 6CO2 + 5H2O
        wood.heat_of_combustion_kj_mol = 2800.0f;    // ~2800 kJ/mol
        wood.can_decompose = true;
        wood.decomposition_temp_k = 523.15f;          // 250°C pyrolysis
        wood.decomposition_products = {"CO", "CO2", "H2O", "CH4", "Char"};
        wood.config.soot_production_rate = 0.15f;
        wood.config.smoke_density_factor = 2.0f;
        wood.config.ash_residue_fraction = 0.05f;
        return wood;
    }

    /**
     * @brief Gasoline (octane): C8H18
     */
    static ChemicalCompositionComponent create_gasoline() noexcept {
        ChemicalCompositionComponent gasoline;
        gasoline.chemical_formula = "C8H18";
        gasoline.elements = {
            {"C", 8.0f, 0.842f},   // 84.2% carbon
            {"H", 18.0f, 0.158f}   // 15.8% hydrogen
        };
        gasoline.is_combustible = true;
        gasoline.oxidation_rate = 8.0f;               // Very fast
        gasoline.oxygen_required_mol = 12.5f;         // C8H18 + 12.5O2 → 8CO2 + 9H2O
        gasoline.heat_of_combustion_kj_mol = 5470.0f; // Very high energy
        gasoline.can_decompose = false;
        gasoline.config.soot_production_rate = 0.25f; // Sooty flame
        gasoline.config.smoke_density_factor = 1.5f;
        gasoline.config.combustion_efficiency = 0.90f;
        gasoline.config.co2_production_ratio = 0.85f; // Some incomplete combustion
        return gasoline;
    }

    /**
     * @brief Methane (natural gas): CH4
     */
    static ChemicalCompositionComponent create_methane() noexcept {
        ChemicalCompositionComponent methane;
        methane.chemical_formula = "CH4";
        methane.elements = {
            {"C", 1.0f, 0.749f},
            {"H", 4.0f, 0.251f}
        };
        methane.is_combustible = true;
        methane.oxidation_rate = 7.0f;
        methane.oxygen_required_mol = 2.0f;           // CH4 + 2O2 → CO2 + 2H2O
        methane.heat_of_combustion_kj_mol = 890.0f;
        methane.can_decompose = false;
        methane.config.soot_production_rate = 0.0f;   // Clean burn
        methane.config.smoke_density_factor = 0.1f;
        methane.config.combustion_efficiency = 0.99f;
        methane.config.co2_production_ratio = 1.0f;   // Complete combustion
        return methane;
    }

    /**
     * @brief Gunpowder (black powder): 75% KNO3, 15% C, 10% S
     */
    static ChemicalCompositionComponent create_gunpowder() noexcept {
        ChemicalCompositionComponent gunpowder;
        gunpowder.chemical_formula = "KNO3_C_S";
        gunpowder.elements = {
            {"K", 1.0f, 0.147f},
            {"N", 1.0f, 0.053f},
            {"O", 3.0f, 0.18f},
            {"C", 1.0f, 0.045f},
            {"S", 1.0f, 0.012f}
        };
        gunpowder.is_combustible = true;
        gunpowder.oxidation_rate = 10.0f;             // Extremely fast (explosive)
        gunpowder.oxygen_required_mol = 0.0f;         // Self-oxidizing
        gunpowder.heat_of_combustion_kj_mol = 3000.0f;
        gunpowder.can_decompose = true;
        gunpowder.decomposition_temp_k = 573.15f;     // 300°C ignition
        gunpowder.decomposition_products = {"CO2", "N2", "K2S", "SO2"};
        gunpowder.config.soot_production_rate = 0.5f; // Very smoky
        gunpowder.config.smoke_density_factor = 5.0f;
        gunpowder.config.ash_residue_fraction = 0.5f; // Lots of solid residue
        return gunpowder;
    }

    /**
     * @brief Steel (iron with carbon): Fe + C
     */
    static ChemicalCompositionComponent create_steel() noexcept {
        ChemicalCompositionComponent steel;
        steel.chemical_formula = "Fe_C";
        steel.elements = {
            {"Fe", 1.0f, 0.985f},  // 98.5% iron
            {"C", 0.01f, 0.015f}   // 1.5% carbon
        };
        steel.is_combustible = false;
        steel.can_decompose = false;
        return steel;
    }
};

} // namespace lore::ecs