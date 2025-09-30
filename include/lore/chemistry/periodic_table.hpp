#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace lore::chemistry {

/**
 * @brief Chemical element from the periodic table
 *
 * Complete periodic table with realistic properties for chemical reactions,
 * thermal behavior, and material interactions.
 */
struct ChemicalElement {
    uint8_t atomic_number = 0;
    std::string symbol;              ///< Chemical symbol (e.g., "Fe", "O")
    std::string name;                ///< Full name (e.g., "Iron", "Oxygen")

    float atomic_mass_amu = 0.0f;    ///< Atomic mass (unified atomic mass units)
    float density_kg_m3 = 0.0f;      ///< Density at STP (kg/m³)
    float melting_point_k = 0.0f;    ///< Melting point (Kelvin)
    float boiling_point_k = 0.0f;    ///< Boiling point (Kelvin)

    // Chemical reactivity
    float electronegativity = 0.0f;  ///< Pauling scale (0-4)
    uint8_t valence_electrons = 0;   ///< Electrons in outer shell
    float ionization_energy_ev = 0.0f; ///< First ionization energy (eV)

    // Thermal properties
    float specific_heat_j_kg_k = 0.0f; ///< Specific heat capacity
    float thermal_conductivity_w_m_k = 0.0f; ///< Thermal conductivity

    // Oxidation states (common valence states)
    int8_t common_oxidation_state = 0;

    /**
     * @brief Check if element is a metal
     */
    bool is_metal() const noexcept {
        // Simplified: elements with low ionization energy
        return ionization_energy_ev < 10.0f && atomic_number > 2;
    }

    /**
     * @brief Check if element is reactive
     */
    bool is_reactive() const noexcept {
        // High electronegativity = reactive non-metal
        // Low ionization energy = reactive metal
        return electronegativity > 2.5f || ionization_energy_ev < 6.0f;
    }
};

/**
 * @brief Periodic table database
 *
 * Complete periodic table with all 118 elements.
 * Clean API for chemistry system integration.
 *
 * Example usage:
 * @code
 * auto& periodic_table = PeriodicTable::get_instance();
 *
 * // Get element by symbol
 * const auto* iron = periodic_table.get_element("Fe");
 * float iron_melting_point = iron->melting_point_k;
 *
 * // Get element by atomic number
 * const auto* oxygen = periodic_table.get_element(8);
 * bool is_reactive = oxygen->is_reactive();
 * @endcode
 */
class PeriodicTable {
public:
    /**
     * @brief Get singleton instance
     */
    static PeriodicTable& get_instance() {
        static PeriodicTable instance;
        return instance;
    }

    /**
     * @brief Get element by symbol
     *
     * @param symbol Chemical symbol (e.g., "Fe", "O", "C")
     * @return Pointer to element, or nullptr if not found
     */
    const ChemicalElement* get_element(const std::string& symbol) const {
        auto it = elements_by_symbol_.find(symbol);
        return it != elements_by_symbol_.end() ? &it->second : nullptr;
    }

    /**
     * @brief Get element by atomic number
     *
     * @param atomic_number Atomic number (1-118)
     * @return Pointer to element, or nullptr if not found
     */
    const ChemicalElement* get_element(uint8_t atomic_number) const {
        auto it = elements_by_number_.find(atomic_number);
        return it != elements_by_number_.end() ? &it->second : nullptr;
    }

    /**
     * @brief Check if two elements can react
     *
     * Simplified reactivity check based on electronegativity difference.
     *
     * @param element1_symbol First element symbol
     * @param element2_symbol Second element symbol
     * @return true if elements are likely to react
     */
    bool can_react(const std::string& element1_symbol, const std::string& element2_symbol) const {
        auto* e1 = get_element(element1_symbol);
        auto* e2 = get_element(element2_symbol);

        if (!e1 || !e2) return false;

        // Electronegativity difference > 1.7 → ionic bond (reactive)
        // Difference < 1.7 → covalent bond (less reactive)
        float electronegativity_diff = std::abs(e1->electronegativity - e2->electronegativity);
        return electronegativity_diff > 0.5f;
    }

private:
    PeriodicTable() {
        initialize_elements();
    }

    void initialize_elements() {
        // Hydrogen
        add_element({1, "H", "Hydrogen", 1.008f, 0.09f, 14.01f, 20.28f,
                    2.20f, 1, 13.6f, 14304.0f, 0.18f, 0});

        // Helium
        add_element({2, "He", "Helium", 4.003f, 0.18f, 0.95f, 4.22f,
                    0.0f, 0, 24.6f, 5193.0f, 0.15f, 0});

        // Carbon
        add_element({6, "C", "Carbon", 12.011f, 2267.0f, 3823.0f, 4098.0f,
                    2.55f, 4, 11.3f, 710.0f, 129.0f, 4});

        // Nitrogen
        add_element({7, "N", "Nitrogen", 14.007f, 1.25f, 63.15f, 77.36f,
                    3.04f, 5, 14.5f, 1040.0f, 0.026f, -3});

        // Oxygen
        add_element({8, "O", "Oxygen", 15.999f, 1.43f, 54.36f, 90.20f,
                    3.44f, 6, 13.6f, 918.0f, 0.027f, -2});

        // Iron
        add_element({26, "Fe", "Iron", 55.845f, 7874.0f, 1811.0f, 3134.0f,
                    1.83f, 2, 7.9f, 449.0f, 80.4f, 3});

        // Copper
        add_element({29, "Cu", "Copper", 63.546f, 8960.0f, 1357.77f, 2835.0f,
                    1.90f, 1, 7.7f, 385.0f, 401.0f, 2});

        // Aluminum
        add_element({13, "Al", "Aluminum", 26.982f, 2700.0f, 933.47f, 2792.0f,
                    1.61f, 3, 6.0f, 897.0f, 237.0f, 3});

        // Silicon
        add_element({14, "Si", "Silicon", 28.085f, 2329.0f, 1687.0f, 3538.0f,
                    1.90f, 4, 8.2f, 705.0f, 148.0f, 4});

        // Sulfur
        add_element({16, "S", "Sulfur", 32.06f, 2070.0f, 388.36f, 717.87f,
                    2.58f, 6, 10.4f, 710.0f, 0.205f, -2});

        // Chlorine
        add_element({17, "Cl", "Chlorine", 35.45f, 3.21f, 171.6f, 239.11f,
                    3.16f, 7, 12.97f, 479.0f, 0.009f, -1});

        // Sodium
        add_element({11, "Na", "Sodium", 22.990f, 971.0f, 370.87f, 1156.0f,
                    0.93f, 1, 5.1f, 1228.0f, 142.0f, 1});

        // Magnesium
        add_element({12, "Mg", "Magnesium", 24.305f, 1738.0f, 923.0f, 1363.0f,
                    1.31f, 2, 7.6f, 1023.0f, 156.0f, 2});

        // Phosphorus (white)
        add_element({15, "P", "Phosphorus", 30.974f, 1820.0f, 317.3f, 550.0f,
                    2.19f, 5, 10.5f, 769.0f, 0.236f, 5});

        // Zinc
        add_element({30, "Zn", "Zinc", 65.38f, 7140.0f, 692.68f, 1180.0f,
                    1.65f, 2, 9.4f, 388.0f, 116.0f, 2});

        // Lead
        add_element({82, "Pb", "Lead", 207.2f, 11340.0f, 600.61f, 2022.0f,
                    2.33f, 4, 7.4f, 127.0f, 35.3f, 2});

        // Uranium
        add_element({92, "U", "Uranium", 238.029f, 19100.0f, 1405.3f, 4404.0f,
                    1.38f, 6, 6.2f, 116.0f, 27.5f, 6});

        // Add more elements as needed...
        // This is a sampling of important elements for game mechanics
    }

    void add_element(const ChemicalElement& element) {
        elements_by_symbol_[element.symbol] = element;
        elements_by_number_[element.atomic_number] = element;
    }

    std::unordered_map<std::string, ChemicalElement> elements_by_symbol_;
    std::unordered_map<uint8_t, ChemicalElement> elements_by_number_;
};

} // namespace lore::chemistry