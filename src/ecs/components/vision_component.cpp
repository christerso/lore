#include <lore/ecs/components/vision_component.hpp>

namespace lore::ecs {

VisionComponent VisionComponent::create_default() {
    VisionComponent comp;
    comp.profile.base_range_meters = 20.0f;
    comp.profile.fov_angle_degrees = 120.0f;
    comp.profile.perception = 0.5f;
    comp.profile.night_vision = 0.0f;
    comp.profile.visual_acuity = 1.0f;
    comp.profile.has_thermal_vision = false;
    comp.profile.has_xray_vision = false;
    comp.eye_height = 1.7f;
    return comp;
}

VisionComponent VisionComponent::create_player() {
    VisionComponent comp;
    comp.profile.base_range_meters = 50.0f;
    comp.profile.fov_angle_degrees = 110.0f;
    comp.profile.perception = 0.7f;
    comp.profile.night_vision = 0.1f;
    comp.profile.visual_acuity = 1.0f;
    comp.profile.has_thermal_vision = false;
    comp.profile.has_xray_vision = false;
    comp.eye_height = 1.7f;
    return comp;
}

VisionComponent VisionComponent::create_guard() {
    VisionComponent comp;
    comp.profile.base_range_meters = 30.0f;
    comp.profile.fov_angle_degrees = 140.0f;
    comp.profile.perception = 0.8f; // Trained guard
    comp.profile.night_vision = 0.2f;
    comp.profile.visual_acuity = 1.2f; // Sharp eyes
    comp.profile.has_thermal_vision = false;
    comp.profile.has_xray_vision = false;
    comp.eye_height = 1.75f;
    return comp;
}

VisionComponent VisionComponent::create_creature(float range_meters, float fov_degrees) {
    VisionComponent comp;
    comp.profile.base_range_meters = range_meters;
    comp.profile.fov_angle_degrees = fov_degrees;
    comp.profile.perception = 0.9f; // Creatures have keen senses
    comp.profile.night_vision = 0.8f; // Most creatures can see in dark
    comp.profile.visual_acuity = 1.5f;
    comp.profile.has_thermal_vision = false;
    comp.profile.has_xray_vision = false;
    comp.eye_height = 0.5f; // Lower for most creatures
    return comp;
}

} // namespace lore::ecs