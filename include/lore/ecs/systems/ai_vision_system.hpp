#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/components/ai_vision_behavior_component.hpp>
#include <lore/ecs/components/visibility_component.hpp>
#include <memory>

namespace lore::ecs {

/**
 * @brief System that processes AI vision-based behaviors
 *
 * Updates entities with AIVisionBehaviorComponent based on what they can see.
 * Handles state transitions for detection, pursuit, fleeing, and investigation.
 *
 * Requires entities to have:
 * - AIVisionBehaviorComponent (behavior configuration)
 * - VisibilityComponent (what entity can see)
 * - TransformComponent (entity position)
 */
class AIVisionSystem : public System {
public:
    AIVisionSystem() = default;

    /**
     * @brief Initialize the AI vision system
     * @param world ECS world instance
     */
    void init(World& world) override;

    /**
     * @brief Update all AI vision behaviors
     * @param world ECS world containing entities
     * @param delta_time Time since last frame (seconds)
     */
    void update(World& world, float delta_time) override;

    /**
     * @brief Shutdown the AI vision system
     * @param world ECS world instance
     */
    void shutdown(World& world) override;

    /**
     * @brief Enable/disable AI vision system (for performance)
     * @param enabled Whether system is active
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if AI vision system is enabled
     */
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Set debug visualization mode
     */
    void set_debug_mode(bool debug) { debug_mode_ = debug; }

    /**
     * @brief Check if debug mode is enabled
     */
    bool is_debug_mode() const { return debug_mode_; }

private:
    bool enabled_ = true;
    bool debug_mode_ = false;
    float current_time_ = 0.0f;

    /// Process single entity's AI vision behavior
    void update_entity_behavior(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Detect and classify targets in visibility set
    void detect_targets(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position
    );

    /// Select best target based on threat level and priority
    void select_target(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const math::Vec3& position
    );

    /// Update behavior state machine
    void update_state_machine(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Transition to new state
    void change_state(
        AIVisionBehaviorComponent& behavior,
        AIVisionBehaviorComponent::State new_state
    );

    /// Handle idle state behavior
    void handle_idle_state(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position
    );

    /// Handle alert state behavior
    void handle_alert_state(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Handle pursuit state behavior
    void handle_pursuit_state(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Handle fleeing state behavior
    void handle_fleeing_state(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Handle investigation state behavior
    void handle_investigation_state(
        World& world,
        Entity entity_id,
        AIVisionBehaviorComponent& behavior,
        const VisibilityComponent& visibility,
        const math::Vec3& position,
        float delta_time
    );

    /// Calculate threat level for a target
    float calculate_threat_level(
        World& world,
        Entity observer_id,
        Entity target_id,
        const math::Vec3& observer_pos,
        const math::Vec3& target_pos
    );

    /// Classify target type (prey, predator, neutral, ally)
    AIVisionBehaviorComponent::TargetType classify_target(
        World& world,
        Entity observer_id,
        Entity target_id,
        const AIVisionBehaviorComponent& behavior
    );

    /// Check if target should trigger pursuit
    bool should_pursue(
        const AIVisionBehaviorComponent& behavior,
        const AIVisionBehaviorComponent::TargetInfo& target,
        float distance
    );

    /// Check if target should trigger fleeing
    bool should_flee(
        const AIVisionBehaviorComponent& behavior,
        const AIVisionBehaviorComponent::TargetInfo& target,
        float distance
    );

    /// Get target position from entity
    math::Vec3 get_entity_position(World& world, Entity entity_id);
};

} // namespace lore::ecs