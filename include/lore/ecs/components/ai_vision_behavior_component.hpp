#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <functional>

namespace lore::ecs {

/**
 * @brief AI behavior driven by vision system
 *
 * Entities with this component react to what they can see:
 * - Detect threats/targets when they enter FOV
 * - Pursue visible targets
 * - Flee from visible predators
 * - Investigate last known positions when targets disappear
 */
struct AIVisionBehaviorComponent {
    /// Current behavioral state
    enum class State {
        Idle,           // No targets visible, patrolling or idle
        Alert,          // Target detected but not yet pursuing
        Pursuing,       // Actively chasing visible target
        Fleeing,        // Running from visible threat
        Investigating,  // Searching for target at last known position
        Hiding          // Taking cover from threat
    };

    /// Target classification for decision making
    enum class TargetType {
        None,
        Prey,           // Something to hunt/chase
        Predator,       // Something to flee from
        Neutral,        // Something to observe but not react to
        Ally            // Friendly entity
    };

    /// Behavior configuration
    struct Config {
        float detection_distance = 30.0f;       // Maximum distance to detect targets
        float alert_distance = 20.0f;           // Distance to become alert
        float pursuit_distance = 50.0f;         // Maximum pursuit distance
        float flee_distance = 40.0f;            // How far to flee from threats

        float investigation_time = 5.0f;        // How long to investigate last known position
        float alert_timeout = 3.0f;             // How long to stay alert after losing sight

        bool can_pursue = true;                 // Can this entity pursue targets?
        bool can_flee = true;                   // Can this entity flee from threats?
        bool can_investigate = true;            // Can this entity investigate?

        float perception_multiplier = 1.0f;     // Perception skill modifier
    };

    /// Target tracking information
    struct TargetInfo {
        Entity entity_id = INVALID_ENTITY;
        TargetType type = TargetType::None;
        math::Vec3 last_known_position;
        float last_seen_time = 0.0f;
        float threat_level = 0.0f;              // 0.0 = no threat, 1.0 = extreme threat
    };

    // Current state
    State state = State::Idle;
    State previous_state = State::Idle;

    // Configuration
    Config config;

    // Current target (entity we're pursuing/fleeing from)
    TargetInfo current_target;

    // All detected entities (for threat assessment)
    std::unordered_set<Entity> detected_entities;

    // Recently lost targets (for investigation)
    std::vector<TargetInfo> lost_targets;

    // Investigation point
    math::Vec3 investigation_point;
    float investigation_start_time = 0.0f;

    // Timing
    float state_enter_time = 0.0f;
    float last_update_time = 0.0f;

    // Faction/allegiance for target classification
    int32_t faction_id = 0;

    // Callbacks for custom behavior
    std::function<void(State, State)> on_state_changed;         // (old_state, new_state)
    std::function<void(Entity)> on_target_detected;             // (target_entity)
    std::function<void(Entity)> on_target_lost;                 // (target_entity)
    std::function<TargetType(Entity)> classify_target;          // Custom target classification

    /**
     * @brief Create default AI vision behavior for neutral NPC
     */
    static AIVisionBehaviorComponent create_default();

    /**
     * @brief Create AI vision behavior for guard/patrol NPC
     */
    static AIVisionBehaviorComponent create_guard();

    /**
     * @brief Create AI vision behavior for prey animal
     */
    static AIVisionBehaviorComponent create_prey();

    /**
     * @brief Create AI vision behavior for predator
     */
    static AIVisionBehaviorComponent create_predator();

    /**
     * @brief Check if entity is in current behavior state
     */
    bool is_in_state(State check_state) const { return state == check_state; }

    /**
     * @brief Get time in current state (seconds)
     */
    float time_in_state(float current_time) const {
        return current_time - state_enter_time;
    }

    /**
     * @brief Check if entity has a valid target
     */
    bool has_target() const {
        return current_target.entity_id != INVALID_ENTITY;
    }

    /**
     * @brief Clear current target
     */
    void clear_target() {
        current_target.entity_id = INVALID_ENTITY;
        current_target.type = TargetType::None;
    }
};

} // namespace lore::ecs