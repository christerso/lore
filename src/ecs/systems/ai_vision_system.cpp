#include <lore/ecs/systems/ai_vision_system.hpp>
#include <lore/ecs/components/vision_component.hpp>
#include <lore/input/input_ecs.hpp> // For TransformComponent
#include <lore/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace lore::ecs {

void AIVisionSystem::init(World& world) {
    (void)world;
    LOG_INFO(Game, "AIVisionSystem initialized");
}

void AIVisionSystem::shutdown(World& world) {
    (void)world;
    LOG_INFO(Game, "AIVisionSystem shutdown");
}

void AIVisionSystem::update(World& world, float delta_time) {
    if (!enabled_) {
        return;
    }

    current_time_ += delta_time;

    // Get component arrays
    auto& behavior_array = world.get_component_array<AIVisionBehaviorComponent>();
    auto& visibility_array = world.get_component_array<VisibilityComponent>();
    auto& transform_array = world.get_component_array<input::TransformComponent>();

    const Entity* behavior_entities = behavior_array.entities();
    const std::size_t behavior_count = behavior_array.size();

    // Process each entity with AI vision behavior
    for (std::size_t i = 0; i < behavior_count; ++i) {
        Entity entity_id = behavior_entities[i];

        // Check if entity has required components
        if (!visibility_array.has_component(entity_id) || !transform_array.has_component(entity_id)) {
            continue;
        }

        auto& behavior = behavior_array.get_component(entity_id);
        const auto& visibility = visibility_array.get_component(entity_id);
        const auto& transform = transform_array.get_component(entity_id);

        behavior.last_update_time = current_time_;

        update_entity_behavior(world, entity_id, behavior, visibility, transform.position, delta_time);
    }
}

void AIVisionSystem::update_entity_behavior(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    // Detect targets in current visibility
    detect_targets(world, entity_id, behavior, visibility, position);

    // Update state machine based on detected targets
    update_state_machine(world, entity_id, behavior, visibility, position, delta_time);
}

void AIVisionSystem::detect_targets(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position)
{
    // Clear old detected entities
    std::unordered_set<Entity> previous_detected = behavior.detected_entities;
    behavior.detected_entities.clear();

    // Check each visible entity
    for (Entity visible_entity : visibility.visible_entities) {
        // Don't detect self
        if (visible_entity == entity_id) {
            continue;
        }

        math::Vec3 target_pos = get_entity_position(world, visible_entity);
        float distance = glm::length(target_pos - position);

        // Check if within detection range
        if (distance <= behavior.config.detection_distance * behavior.config.perception_multiplier) {
            behavior.detected_entities.insert(visible_entity);

            // Check if this is a newly detected entity
            if (previous_detected.find(visible_entity) == previous_detected.end()) {
                // New detection - notify callback
                if (behavior.on_target_detected) {
                    behavior.on_target_detected(visible_entity);
                }

                LOG_DEBUG(Game, "Entity {} detected entity {} at distance {:.2f}",
                         entity_id, visible_entity, distance);
            }
        }
    }

    // Check for lost targets
    for (Entity prev_entity : previous_detected) {
        if (behavior.detected_entities.find(prev_entity) == behavior.detected_entities.end()) {
            // Entity was detected but is no longer visible
            AIVisionBehaviorComponent::TargetInfo lost_target;
            lost_target.entity_id = prev_entity;
            lost_target.type = classify_target(world, entity_id, prev_entity, behavior);
            lost_target.last_known_position = get_entity_position(world, prev_entity);
            lost_target.last_seen_time = current_time_;

            behavior.lost_targets.push_back(lost_target);

            // Notify callback
            if (behavior.on_target_lost) {
                behavior.on_target_lost(prev_entity);
            }

            LOG_DEBUG(Game, "Entity {} lost sight of entity {}", entity_id, prev_entity);
        }
    }

    // Select best target from detected entities
    select_target(world, entity_id, behavior, position);
}

void AIVisionSystem::select_target(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const math::Vec3& position)
{
    Entity best_target = INVALID_ENTITY;
    float highest_threat = -1.0f;
    float closest_distance = std::numeric_limits<float>::max();

    for (Entity detected_entity : behavior.detected_entities) {
        math::Vec3 target_pos = get_entity_position(world, detected_entity);
        float distance = glm::length(target_pos - position);

        // Calculate threat level
        float threat = calculate_threat_level(world, entity_id, detected_entity, position, target_pos);

        // Classify target
        auto target_type = classify_target(world, entity_id, detected_entity, behavior);

        // Select target based on threat and distance
        bool is_better_target = false;

        if (target_type == AIVisionBehaviorComponent::TargetType::Predator) {
            // Predators are highest priority
            if (threat > highest_threat) {
                is_better_target = true;
            }
        } else if (target_type == AIVisionBehaviorComponent::TargetType::Prey) {
            // Prey is pursued if no predators present
            if (highest_threat < 0.5f && distance < closest_distance) {
                is_better_target = true;
            }
        }

        if (is_better_target) {
            best_target = detected_entity;
            highest_threat = threat;
            closest_distance = distance;
        }
    }

    // Update current target if changed
    if (best_target != behavior.current_target.entity_id) {
        if (best_target != INVALID_ENTITY) {
            behavior.current_target.entity_id = best_target;
            behavior.current_target.type = classify_target(world, entity_id, best_target, behavior);
            behavior.current_target.last_known_position = get_entity_position(world, best_target);
            behavior.current_target.last_seen_time = current_time_;
            behavior.current_target.threat_level = highest_threat;

            LOG_DEBUG(Game, "Entity {} selected target {} (threat: {:.2f})",
                     entity_id, best_target, highest_threat);
        } else {
            behavior.clear_target();
        }
    } else if (best_target != INVALID_ENTITY) {
        // Update existing target info
        behavior.current_target.last_known_position = get_entity_position(world, best_target);
        behavior.current_target.last_seen_time = current_time_;
        behavior.current_target.threat_level = highest_threat;
    }
}

void AIVisionSystem::update_state_machine(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    switch (behavior.state) {
        case AIVisionBehaviorComponent::State::Idle:
            handle_idle_state(world, entity_id, behavior, visibility, position);
            break;

        case AIVisionBehaviorComponent::State::Alert:
            handle_alert_state(world, entity_id, behavior, visibility, position, delta_time);
            break;

        case AIVisionBehaviorComponent::State::Pursuing:
            handle_pursuit_state(world, entity_id, behavior, visibility, position, delta_time);
            break;

        case AIVisionBehaviorComponent::State::Fleeing:
            handle_fleeing_state(world, entity_id, behavior, visibility, position, delta_time);
            break;

        case AIVisionBehaviorComponent::State::Investigating:
            handle_investigation_state(world, entity_id, behavior, visibility, position, delta_time);
            break;

        case AIVisionBehaviorComponent::State::Hiding:
            // Hiding state - check if threat is still present
            if (!behavior.has_target()) {
                change_state(behavior, AIVisionBehaviorComponent::State::Alert);
            }
            break;
    }
}

void AIVisionSystem::change_state(
    AIVisionBehaviorComponent& behavior,
    AIVisionBehaviorComponent::State new_state)
{
    if (behavior.state == new_state) {
        return;
    }

    AIVisionBehaviorComponent::State old_state = behavior.state;
    behavior.previous_state = old_state;
    behavior.state = new_state;
    behavior.state_enter_time = current_time_;

    // Notify callback
    if (behavior.on_state_changed) {
        behavior.on_state_changed(old_state, new_state);
    }

    LOG_DEBUG(Game, "AI state changed: {} -> {}",
             static_cast<int>(old_state), static_cast<int>(new_state));
}

void AIVisionSystem::handle_idle_state(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position)
{
    (void)world;
    (void)entity_id;
    (void)visibility;

    // Check if target detected
    if (behavior.has_target()) {
        math::Vec3 target_pos = behavior.current_target.last_known_position;
        float distance = glm::length(target_pos - position);

        // Decide next state based on target type
        if (behavior.current_target.type == AIVisionBehaviorComponent::TargetType::Predator) {
            if (behavior.config.can_flee) {
                change_state(behavior, AIVisionBehaviorComponent::State::Fleeing);
            } else {
                change_state(behavior, AIVisionBehaviorComponent::State::Alert);
            }
        } else if (behavior.current_target.type == AIVisionBehaviorComponent::TargetType::Prey) {
            if (behavior.config.can_pursue && distance <= behavior.config.pursuit_distance) {
                change_state(behavior, AIVisionBehaviorComponent::State::Alert);
            }
        }
    }
}

void AIVisionSystem::handle_alert_state(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    (void)world;
    (void)entity_id;
    (void)visibility;
    (void)delta_time;

    if (!behavior.has_target()) {
        // No target - return to idle after timeout
        if (behavior.time_in_state(current_time_) >= behavior.config.alert_timeout) {
            change_state(behavior, AIVisionBehaviorComponent::State::Idle);
        }
        return;
    }

    math::Vec3 target_pos = behavior.current_target.last_known_position;
    float distance = glm::length(target_pos - position);

    // Check if should pursue or flee
    if (should_flee(behavior, behavior.current_target, distance)) {
        change_state(behavior, AIVisionBehaviorComponent::State::Fleeing);
    } else if (should_pursue(behavior, behavior.current_target, distance)) {
        change_state(behavior, AIVisionBehaviorComponent::State::Pursuing);
    }
}

void AIVisionSystem::handle_pursuit_state(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    (void)world;
    (void)entity_id;
    (void)delta_time;

    if (!behavior.has_target()) {
        // Lost target - investigate last known position
        if (behavior.config.can_investigate && !behavior.lost_targets.empty()) {
            behavior.investigation_point = behavior.lost_targets.back().last_known_position;
            behavior.investigation_start_time = current_time_;
            change_state(behavior, AIVisionBehaviorComponent::State::Investigating);
        } else {
            change_state(behavior, AIVisionBehaviorComponent::State::Alert);
        }
        return;
    }

    // Check if target is still visible
    if (visibility.can_see_entity(behavior.current_target.entity_id)) {
        math::Vec3 target_pos = behavior.current_target.last_known_position;
        float distance = glm::length(target_pos - position);

        // Check if out of pursuit range
        if (distance > behavior.config.pursuit_distance) {
            change_state(behavior, AIVisionBehaviorComponent::State::Alert);
        }
    } else {
        // Target no longer visible - start investigating
        if (behavior.config.can_investigate) {
            behavior.investigation_point = behavior.current_target.last_known_position;
            behavior.investigation_start_time = current_time_;
            change_state(behavior, AIVisionBehaviorComponent::State::Investigating);
        } else {
            change_state(behavior, AIVisionBehaviorComponent::State::Alert);
        }
    }
}

void AIVisionSystem::handle_fleeing_state(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    (void)world;
    (void)entity_id;
    (void)visibility;
    (void)delta_time;

    if (!behavior.has_target()) {
        // Threat gone - return to alert
        change_state(behavior, AIVisionBehaviorComponent::State::Alert);
        return;
    }

    math::Vec3 threat_pos = behavior.current_target.last_known_position;
    float distance = glm::length(threat_pos - position);

    // Check if far enough away
    if (distance >= behavior.config.flee_distance) {
        change_state(behavior, AIVisionBehaviorComponent::State::Alert);
    }
}

void AIVisionSystem::handle_investigation_state(
    World& world,
    Entity entity_id,
    AIVisionBehaviorComponent& behavior,
    const VisibilityComponent& visibility,
    const math::Vec3& position,
    float delta_time)
{
    (void)world;
    (void)entity_id;
    (void)visibility;
    (void)delta_time;

    // Check if new target detected during investigation
    if (behavior.has_target() &&
        visibility.can_see_entity(behavior.current_target.entity_id)) {
        // Re-acquired target
        if (should_pursue(behavior, behavior.current_target,
                         glm::length(behavior.current_target.last_known_position - position))) {
            change_state(behavior, AIVisionBehaviorComponent::State::Pursuing);
        } else {
            change_state(behavior, AIVisionBehaviorComponent::State::Alert);
        }
        return;
    }

    // Check if investigation time expired
    float investigation_duration = current_time_ - behavior.investigation_start_time;
    if (investigation_duration >= behavior.config.investigation_time) {
        change_state(behavior, AIVisionBehaviorComponent::State::Idle);
        behavior.lost_targets.clear();
    }

    // Check if reached investigation point
    float distance_to_point = glm::length(behavior.investigation_point - position);
    if (distance_to_point < 2.0f) {
        // Reached investigation point - give up and return to idle
        change_state(behavior, AIVisionBehaviorComponent::State::Idle);
        behavior.lost_targets.clear();
    }
}

float AIVisionSystem::calculate_threat_level(
    World& world,
    Entity observer_id,
    Entity target_id,
    const math::Vec3& observer_pos,
    const math::Vec3& target_pos)
{
    (void)world;
    (void)observer_id;
    (void)target_id;

    // Basic threat calculation based on distance (closer = more threatening)
    float distance = glm::length(target_pos - observer_pos);
    float threat = 1.0f - std::min(distance / 50.0f, 1.0f);

    // TODO: Consider other factors:
    // - Target's combat capabilities
    // - Target's faction relationship
    // - Target's current behavior state

    return threat;
}

AIVisionBehaviorComponent::TargetType AIVisionSystem::classify_target(
    World& world,
    Entity observer_id,
    Entity target_id,
    const AIVisionBehaviorComponent& behavior)
{
    (void)world;
    (void)observer_id;
    (void)target_id;

    // Use custom classification if provided
    if (behavior.classify_target) {
        return behavior.classify_target(target_id);
    }

    // Default classification: neutral
    // TODO: Implement faction-based classification
    return AIVisionBehaviorComponent::TargetType::Neutral;
}

bool AIVisionSystem::should_pursue(
    const AIVisionBehaviorComponent& behavior,
    const AIVisionBehaviorComponent::TargetInfo& target,
    float distance)
{
    if (!behavior.config.can_pursue) {
        return false;
    }

    if (target.type != AIVisionBehaviorComponent::TargetType::Prey) {
        return false;
    }

    if (distance > behavior.config.pursuit_distance) {
        return false;
    }

    return true;
}

bool AIVisionSystem::should_flee(
    const AIVisionBehaviorComponent& behavior,
    const AIVisionBehaviorComponent::TargetInfo& target,
    float distance)
{
    if (!behavior.config.can_flee) {
        return false;
    }

    if (target.type != AIVisionBehaviorComponent::TargetType::Predator) {
        return false;
    }

    if (distance > behavior.config.flee_distance) {
        return false;
    }

    // Flee if threat level is high enough
    return target.threat_level > 0.3f;
}

math::Vec3 AIVisionSystem::get_entity_position(World& world, Entity entity_id) {
    try {
        auto& transform_array = world.get_component_array<input::TransformComponent>();
        if (transform_array.has_component(entity_id)) {
            return transform_array.get_component(entity_id).position;
        }
    } catch (const std::exception&) {
        // Entity doesn't have transform component
    }

    return math::Vec3(0.0f, 0.0f, 0.0f);
}

} // namespace lore::ecs