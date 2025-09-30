#include <lore/ecs/components/ai_vision_behavior_component.hpp>

namespace lore::ecs {

AIVisionBehaviorComponent AIVisionBehaviorComponent::create_default() {
    AIVisionBehaviorComponent comp;
    comp.config.detection_distance = 30.0f;
    comp.config.alert_distance = 20.0f;
    comp.config.pursuit_distance = 50.0f;
    comp.config.flee_distance = 40.0f;
    comp.config.investigation_time = 5.0f;
    comp.config.alert_timeout = 3.0f;
    comp.config.can_pursue = true;
    comp.config.can_flee = true;
    comp.config.can_investigate = true;
    comp.config.perception_multiplier = 1.0f;
    return comp;
}

AIVisionBehaviorComponent AIVisionBehaviorComponent::create_guard() {
    AIVisionBehaviorComponent comp = create_default();
    comp.config.detection_distance = 40.0f;
    comp.config.alert_distance = 30.0f;
    comp.config.pursuit_distance = 80.0f;
    comp.config.flee_distance = 0.0f;  // Guards don't flee
    comp.config.investigation_time = 10.0f;
    comp.config.alert_timeout = 5.0f;
    comp.config.can_pursue = true;
    comp.config.can_flee = false;
    comp.config.can_investigate = true;
    comp.config.perception_multiplier = 1.5f;  // Guards are trained observers
    return comp;
}

AIVisionBehaviorComponent AIVisionBehaviorComponent::create_prey() {
    AIVisionBehaviorComponent comp = create_default();
    comp.config.detection_distance = 25.0f;
    comp.config.alert_distance = 15.0f;
    comp.config.pursuit_distance = 0.0f;  // Prey doesn't pursue
    comp.config.flee_distance = 60.0f;
    comp.config.investigation_time = 2.0f;
    comp.config.alert_timeout = 8.0f;  // Prey stays alert longer
    comp.config.can_pursue = false;
    comp.config.can_flee = true;
    comp.config.can_investigate = false;
    comp.config.perception_multiplier = 1.2f;  // Prey are alert
    return comp;
}

AIVisionBehaviorComponent AIVisionBehaviorComponent::create_predator() {
    AIVisionBehaviorComponent comp = create_default();
    comp.config.detection_distance = 50.0f;
    comp.config.alert_distance = 40.0f;
    comp.config.pursuit_distance = 100.0f;
    comp.config.flee_distance = 30.0f;
    comp.config.investigation_time = 8.0f;
    comp.config.alert_timeout = 4.0f;
    comp.config.can_pursue = true;
    comp.config.can_flee = true;  // Even predators flee from stronger threats
    comp.config.can_investigate = true;
    comp.config.perception_multiplier = 1.8f;  // Predators are keen hunters
    return comp;
}

} // namespace lore::ecs