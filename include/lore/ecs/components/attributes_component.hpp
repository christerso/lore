#pragma once

#include <lore/ecs/ecs.hpp>
#include <cstdint>

namespace lore::ecs {

/**
 * @brief Core attributes that affect ALL game mechanics
 *
 * Six core attributes on 1-20 scale (10 is average human):
 * - STR: Melee damage, carrying capacity
 * - DEX: Accuracy, dodge, stealth
 * - CON: Resilience, stamina, survival
 * - INT: Tech use, problem solving, memory
 * - WIS: Perception, awareness, judgment
 * - CHA: Social interactions, leadership
 *
 * NO over-engineering - just these 6 that matter.
 */
struct AttributesComponent {
    // Core Attributes (1-20 scale, 10 = average human)
    int8_t strength = 10;       ///< Melee damage, carrying capacity
    int8_t dexterity = 10;      ///< Accuracy, dodge, stealth
    int8_t constitution = 10;   ///< Resilience to damage, stamina, survival
    int8_t intelligence = 10;   ///< Tech use, problem solving, learning
    int8_t wisdom = 10;         ///< Perception, awareness, judgment, willpower
    int8_t charisma = 10;       ///< Social interactions, leadership, persuasion

    /**
     * @brief Get attribute modifier (D&D style: (attribute - 10) / 2)
     *
     * Used for skill checks, damage bonuses, etc.
     * - Attribute 10 = +0 modifier (average)
     * - Attribute 12 = +1 modifier
     * - Attribute 8 = -1 modifier
     */
    static int get_modifier(int8_t attribute) {
        return (attribute - 10) / 2;
    }

    /**
     * @brief Get carrying capacity in kilograms
     */
    float get_carrying_capacity() const {
        return strength * 5.0f; // STR 10 = 50kg, STR 20 = 100kg
    }

    /**
     * @brief Get stamina pool (for sprinting, combat actions)
     */
    float get_stamina_pool() const {
        return constitution * 10.0f; // CON 10 = 100 stamina
    }

    /**
     * @brief Get perception range modifier
     */
    float get_perception_multiplier() const {
        return 1.0f + (get_modifier(wisdom) * 0.1f); // WIS 10 = 1.0x, WIS 20 = 1.5x
    }

    /**
     * @brief Get social interaction modifier
     */
    int get_social_modifier() const {
        return get_modifier(charisma);
    }

    /**
     * @brief Create average human attributes (all 10s)
     */
    static AttributesComponent create_average_human() {
        return AttributesComponent{10, 10, 10, 10, 10, 10};
    }

    /**
     * @brief Create soldier/guard attributes
     */
    static AttributesComponent create_soldier() {
        return AttributesComponent{
            .strength = 14,      // Strong
            .dexterity = 12,     // Agile
            .constitution = 13,  // Tough
            .intelligence = 10,  // Average
            .wisdom = 11,        // Alert
            .charisma = 10       // Average
        };
    }

    /**
     * @brief Create scientist/engineer attributes
     */
    static AttributesComponent create_scientist() {
        return AttributesComponent{
            .strength = 8,       // Weak
            .dexterity = 10,     // Average
            .constitution = 9,   // Frail
            .intelligence = 16,  // Brilliant
            .wisdom = 14,        // Insightful
            .charisma = 11       // Decent
        };
    }

    /**
     * @brief Create rogue/thief attributes
     */
    static AttributesComponent create_rogue() {
        return AttributesComponent{
            .strength = 9,       // Light
            .dexterity = 16,     // Very agile
            .constitution = 11,  // Average
            .intelligence = 12,  // Clever
            .wisdom = 13,        // Observant
            .charisma = 14       // Charming
        };
    }

    /**
     * @brief Create robot attributes (high STR/CON, low CHA)
     */
    static AttributesComponent create_robot() {
        return AttributesComponent{
            .strength = 16,      // Very strong
            .dexterity = 8,      // Clumsy
            .constitution = 16,  // Very durable
            .intelligence = 12,  // Programmed
            .wisdom = 10,        // Sensor-based
            .charisma = 3        // Not social
        };
    }

    /**
     * @brief Create animal/creature attributes
     */
    static AttributesComponent create_animal(int8_t str = 12, int8_t dex = 14, int8_t con = 12) {
        return AttributesComponent{
            .strength = str,
            .dexterity = dex,
            .constitution = con,
            .intelligence = 2,   // Animal intelligence
            .wisdom = 12,        // Good instincts
            .charisma = 6        // Limited social
        };
    }
};

} // namespace lore::ecs