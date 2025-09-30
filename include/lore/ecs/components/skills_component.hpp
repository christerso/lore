#pragma once

#include <lore/ecs/ecs.hpp>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <random>

namespace lore::ecs {

/**
 * @brief Learned proficiencies and skill checks
 *
 * Skills are learned abilities (0-20 scale):
 * - 0: Untrained
 * - 5: Novice
 * - 10: Professional
 * - 15: Expert
 * - 20: Master
 *
 * Flexible string-based system - add skills as needed.
 * NO over-engineering with complex skill trees.
 */
struct SkillsComponent {
    /// Skill levels (0-20, 0=untrained, 10=professional, 20=master)
    std::unordered_map<std::string, int8_t> skills;

    // Common skill names (for consistency)
    static constexpr const char* MELEE_COMBAT = "melee_combat";
    static constexpr const char* RANGED_COMBAT = "ranged_combat";
    static constexpr const char* UNARMED_COMBAT = "unarmed_combat";
    static constexpr const char* STEALTH = "stealth";
    static constexpr const char* PERCEPTION = "perception";
    static constexpr const char* ATHLETICS = "athletics";
    static constexpr const char* ENGINEERING = "engineering";
    static constexpr const char* MEDICINE = "medicine";
    static constexpr const char* HACKING = "hacking";
    static constexpr const char* PILOTING = "piloting";
    static constexpr const char* PERSUASION = "persuasion";
    static constexpr const char* INTIMIDATION = "intimidation";
    static constexpr const char* DECEPTION = "deception";
    static constexpr const char* SURVIVAL = "survival";
    static constexpr const char* CRAFTING = "crafting";

    /**
     * @brief Get skill level (returns 0 if untrained)
     */
    int8_t get_skill(const char* skill_name) const {
        auto it = skills.find(skill_name);
        return (it != skills.end()) ? it->second : 0;
    }

    /**
     * @brief Set skill level (automatically clamped to 0-20)
     */
    void set_skill(const char* skill_name, int8_t level) {
        level = std::clamp(level, int8_t(0), int8_t(20));
        skills[skill_name] = level;
    }

    /**
     * @brief Increase skill level (with cap at 20)
     */
    void increase_skill(const char* skill_name, int8_t amount = 1) {
        int8_t current = get_skill(skill_name);
        set_skill(skill_name, current + amount);
    }

    /**
     * @brief Check if entity is trained in skill (level > 0)
     */
    bool is_trained(const char* skill_name) const {
        return get_skill(skill_name) > 0;
    }

    /**
     * @brief Skill check: d20 + skill + attribute modifier vs difficulty
     *
     * @param skill_name The skill to check
     * @param difficulty Target number to beat (10=easy, 15=medium, 20=hard, 25=very hard)
     * @param attribute_modifier Relevant attribute modifier (from AttributesComponent)
     * @return true if check succeeded
     */
    bool skill_check(const char* skill_name, int difficulty, int attribute_modifier) const {
        // Roll d20
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> d20(1, 20);
        int roll = d20(gen);

        // Natural 1 always fails, natural 20 always succeeds
        if (roll == 1) return false;
        if (roll == 20) return true;

        // Check: d20 + skill + attribute modifier
        int total = roll + get_skill(skill_name) + attribute_modifier;
        return total >= difficulty;
    }

    /**
     * @brief Get total skill bonus (skill + attribute modifier)
     */
    int get_skill_bonus(const char* skill_name, int attribute_modifier) const {
        return get_skill(skill_name) + attribute_modifier;
    }

    /**
     * @brief Create untrained character (no skills)
     */
    static SkillsComponent create_untrained() {
        return SkillsComponent{};
    }

    /**
     * @brief Create soldier skill set
     */
    static SkillsComponent create_soldier() {
        return SkillsComponent{
            .skills = {
                {MELEE_COMBAT, 8},
                {RANGED_COMBAT, 10},
                {UNARMED_COMBAT, 6},
                {ATHLETICS, 7},
                {PERCEPTION, 6},
                {INTIMIDATION, 5}
            }
        };
    }

    /**
     * @brief Create scientist/engineer skill set
     */
    static SkillsComponent create_scientist() {
        return SkillsComponent{
            .skills = {
                {ENGINEERING, 12},
                {HACKING, 10},
                {MEDICINE, 8},
                {PERCEPTION, 7},
                {CRAFTING, 9}
            }
        };
    }

    /**
     * @brief Create rogue/thief skill set
     */
    static SkillsComponent create_rogue() {
        return SkillsComponent{
            .skills = {
                {STEALTH, 12},
                {PERCEPTION, 10},
                {HACKING, 8},
                {RANGED_COMBAT, 7},
                {DECEPTION, 9},
                {ATHLETICS, 6}
            }
        };
    }

    /**
     * @brief Create medic skill set
     */
    static SkillsComponent create_medic() {
        return SkillsComponent{
            .skills = {
                {MEDICINE, 14},
                {PERCEPTION, 8},
                {RANGED_COMBAT, 5},
                {PERSUASION, 7},
                {SURVIVAL, 6}
            }
        };
    }

    /**
     * @brief Create pilot skill set
     */
    static SkillsComponent create_pilot() {
        return SkillsComponent{
            .skills = {
                {PILOTING, 12},
                {ENGINEERING, 8},
                {PERCEPTION, 9},
                {RANGED_COMBAT, 6},
                {ATHLETICS, 5}
            }
        };
    }
};

} // namespace lore::ecs