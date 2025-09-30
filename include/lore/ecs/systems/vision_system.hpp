#pragma once

#include <lore/vision/shadow_casting_fov.hpp>
#include <lore/vision/vision_profile.hpp>
#include <lore/world/tilemap_world_system.hpp>
#include <lore/ecs/ecs.hpp>
#include <memory>

namespace lore::ecs {

/**
 * @brief System that updates entity visibility using vision library
 *
 * Each frame, calculates field of view for entities with VisionComponent
 * and updates their VisibilityComponent with visible tiles and entities.
 *
 * Clean API - you just call update() each frame.
 */
class VisionSystem : public System {
public:
    /**
     * @brief Create vision system
     * @param world Reference to tilemap world for vision queries
     */
    explicit VisionSystem(world::TilemapWorldSystem& world);

    /**
     * @brief Initialize the vision system
     * @param world ECS world instance
     */
    void init(World& world) override;

    /**
     * @brief Update all entity vision
     * @param world ECS world containing entities
     * @param delta_time Time since last frame (seconds)
     */
    void update(World& world, float delta_time) override;

    /**
     * @brief Shutdown the vision system
     * @param world ECS world instance
     */
    void shutdown(World& world) override;

    /**
     * @brief Set environmental conditions affecting vision
     * @param conditions New environmental state
     */
    void set_environment(const vision::EnvironmentalConditions& conditions);

    /**
     * @brief Get current environmental conditions
     */
    const vision::EnvironmentalConditions& get_environment() const { return env_conditions_; }

    /**
     * @brief Enable/disable vision system (for performance)
     * @param enabled Whether system is active
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if vision system is enabled
     */
    bool is_enabled() const { return enabled_; }

private:
    world::TilemapWorldSystem& world_;
    std::unique_ptr<world::TilemapVisionAdapter> vision_adapter_;
    vision::EnvironmentalConditions env_conditions_;
    bool enabled_ = true;

    /// Update single entity's visibility
    void update_entity_vision(
        World& world,
        Entity entity_id,
        const math::Vec3& position,
        struct VisionComponent& vision,
        struct VisibilityComponent& visibility
    );

    /// Find entities visible to this entity
    void update_visible_entities(
        World& world,
        Entity observer_entity_id,
        const math::Vec3& observer_position,
        const vision::VisionProfile& observer_profile,
        struct VisibilityComponent& visibility
    );
};

} // namespace lore::ecs