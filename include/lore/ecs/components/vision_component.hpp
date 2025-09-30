#pragma once

#include <lore/vision/vision_profile.hpp>
#include <lore/math/math.hpp>

namespace lore::ecs {

/**
 * @brief Component that defines an entity's vision capabilities
 *
 * Attach this to entities that can see (players, NPCs, creatures).
 * Configure vision parameters like range, FOV, and perception abilities.
 */
struct VisionComponent {
    vision::VisionProfile profile;

    /// Eye height offset from entity position (meters)
    float eye_height = 1.7f;

    /// Direction the entity is looking (normalized forward vector)
    math::Vec3 look_direction = {0.0f, 0.0f, -1.0f};

    /// Whether vision is currently active (can be disabled for optimization)
    bool enabled = true;

    /// Whether entity is currently focused (narrows FOV, increases range)
    bool is_focused = false;

    /**
     * @brief Create VisionComponent with default humanoid profile
     */
    static VisionComponent create_default();

    /**
     * @brief Create VisionComponent for player character
     */
    static VisionComponent create_player();

    /**
     * @brief Create VisionComponent for guard/patrol NPC
     */
    static VisionComponent create_guard();

    /**
     * @brief Create VisionComponent for creature with enhanced senses
     */
    static VisionComponent create_creature(float range_meters, float fov_degrees);
};

} // namespace lore::ecs