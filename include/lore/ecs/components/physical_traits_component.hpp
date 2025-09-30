#pragma once

#include <lore/ecs/ecs.hpp>

namespace lore::ecs {

/**
 * @brief Physical properties that affect gameplay mechanics
 *
 * Simple but realistic physical traits:
 * - Weight and height affect equipment fit, carrying capacity
 * - Size category determines equipment compatibility
 * - Movement speed base (modified by leg function, stamina, etc.)
 */
struct PhysicalTraitsComponent {
    /// Body Size Category (affects equipment compatibility, stealth, etc.)
    enum class SizeCategory : uint8_t {
        Tiny,       ///< < 0.5m (rats, small drones)
        Small,      ///< 0.5-1.2m (children, small creatures)
        Medium,     ///< 1.2-2.4m (humans, most humanoids)
        Large,      ///< 2.4-4.8m (ogres, vehicles)
        Huge        ///< > 4.8m (giants, mechs)
    };

    // Basic Physical Properties
    float weight_kg = 70.0f;        ///< Mass in kilograms (affects fall damage, carrying capacity)
    float height_m = 1.75f;          ///< Height in meters (affects vision height, stealth, equipment fit)
    SizeCategory size = SizeCategory::Medium;  ///< Size category for equipment compatibility

    // Movement
    float base_move_speed = 1.0f;   ///< Base movement multiplier (modified by leg damage, stamina, etc.)

    /**
     * @brief Create default human physical traits
     */
    static PhysicalTraitsComponent create_human(float weight = 70.0f, float height = 1.75f) {
        return PhysicalTraitsComponent{
            .weight_kg = weight,
            .height_m = height,
            .size = SizeCategory::Medium,
            .base_move_speed = 1.0f
        };
    }

    /**
     * @brief Create small creature traits (dog, child, etc.)
     */
    static PhysicalTraitsComponent create_small(float weight = 20.0f, float height = 0.8f) {
        return PhysicalTraitsComponent{
            .weight_kg = weight,
            .height_m = height,
            .size = SizeCategory::Small,
            .base_move_speed = 0.9f
        };
    }

    /**
     * @brief Create large creature traits (ogre, bear, etc.)
     */
    static PhysicalTraitsComponent create_large(float weight = 300.0f, float height = 3.0f) {
        return PhysicalTraitsComponent{
            .weight_kg = weight,
            .height_m = height,
            .size = SizeCategory::Large,
            .base_move_speed = 0.8f
        };
    }

    /**
     * @brief Create robot/drone traits
     */
    static PhysicalTraitsComponent create_robot(float weight = 150.0f, float height = 2.0f) {
        return PhysicalTraitsComponent{
            .weight_kg = weight,
            .height_m = height,
            .size = SizeCategory::Large,
            .base_move_speed = 1.0f
        };
    }

    /**
     * @brief Check if entity can fit through an opening
     */
    bool can_fit_through(float opening_size_m) const {
        return height_m <= opening_size_m * 1.2f; // 20% clearance needed
    }

    /**
     * @brief Check if two entities are compatible sizes (for equipment transfer, etc.)
     */
    bool is_size_compatible(SizeCategory other_size) const {
        // Same size or one category difference
        int diff = std::abs(static_cast<int>(size) - static_cast<int>(other_size));
        return diff <= 1;
    }

    /**
     * @brief Get effective weight for physics calculations
     */
    float get_effective_weight() const {
        return weight_kg;
    }

    /**
     * @brief Get vision height (eye level, roughly)
     */
    float get_eye_height() const {
        return height_m * 0.9f; // Eyes are ~90% of height
    }
};

} // namespace lore::ecs