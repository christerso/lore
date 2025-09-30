#include "lore/ecs/systems/environment_controller.hpp"
#include <algorithm>
#include <cmath>

namespace lore::ecs {

bool EnvironmentController::initialize(World& world) {
    // Find atmospheric component
    // In real implementation, would query ECS for AtmosphericComponent
    // For now, assume it exists
    initialized_ = true;

    // Initialize built-in presets
    presets_["mirrors_edge_bright"] = create_mirrors_edge_bright();
    presets_["mirrors_edge_indoor"] = create_mirrors_edge_indoor();
    presets_["high_contrast"] = create_high_contrast();
    presets_["low_contrast"] = create_low_contrast();
    presets_["warm_sunset"] = create_warm_sunset();
    presets_["cool_morning"] = create_cool_morning();
    presets_["neutral_noon"] = create_neutral_noon();
    presets_["moody_overcast"] = create_moody_overcast();
    presets_["cinematic_night"] = create_cinematic_night();
    presets_["vibrant_day"] = create_vibrant_day();

    return true;
}

void EnvironmentController::update(World& world, float delta_time_s) {
    if (!initialized_) return;

    // Handle transitions
    if (transitioning_) {
        update_transition(delta_time_s);
    }

    // Apply current settings
    apply_lighting(world);
    apply_post_processing(world);
}

// ============================================================================
// Time of Day Control
// ============================================================================

void EnvironmentController::set_time_of_day(float hours) {
    current_time_hours_ = std::fmod(hours, 24.0f);
    if (current_time_hours_ < 0.0f) {
        current_time_hours_ += 24.0f;
    }
}

void EnvironmentController::transition_to_time(float target_hours, float duration_s) {
    transitioning_ = true;
    target_time_hours_ = std::fmod(target_hours, 24.0f);
    transition_duration_ = duration_s;
    transition_elapsed_ = 0.0f;

    // Lerp takes shortest path (handle wraparound)
    float diff = target_time_hours_ - current_time_hours_;
    if (diff > 12.0f) {
        target_time_hours_ -= 24.0f;
    } else if (diff < -12.0f) {
        target_time_hours_ += 24.0f;
    }
}

void EnvironmentController::advance_time(float hours_per_second, float delta_time_s) {
    current_time_hours_ += hours_per_second * delta_time_s;
    current_time_hours_ = std::fmod(current_time_hours_, 24.0f);
}

void EnvironmentController::set_day_of_year(uint32_t day) {
    current_day_of_year_ = std::clamp(day, 1u, 365u);
}

void EnvironmentController::set_latitude(float degrees) {
    current_latitude_ = std::clamp(degrees, -90.0f, 90.0f);
}

// ============================================================================
// Quick Time Presets
// ============================================================================

void EnvironmentController::apply_dawn() {
    set_time_of_day(6.0f);
    post_processing_.temperature = 0.15f;  // Warm
    post_processing_.tint = 0.05f;         // Slightly magenta
    post_processing_.saturation = 0.9f;    // Desaturated
    lighting_settings_.sun_intensity_multiplier = 0.7f;
}

void EnvironmentController::apply_morning() {
    set_time_of_day(9.0f);
    post_processing_.temperature = 0.05f;  // Neutral-warm
    post_processing_.tint = 0.0f;
    post_processing_.saturation = 1.0f;
    lighting_settings_.sun_intensity_multiplier = 1.0f;
}

void EnvironmentController::apply_noon() {
    set_time_of_day(12.0f);
    post_processing_.temperature = 0.0f;   // Neutral
    post_processing_.tint = 0.0f;
    post_processing_.saturation = 1.0f;
    post_processing_.contrast = 1.2f;      // High contrast
    lighting_settings_.sun_intensity_multiplier = 1.1f;
    lighting_settings_.shadow_strength = 1.0f;
}

void EnvironmentController::apply_afternoon() {
    set_time_of_day(15.0f);
    post_processing_.temperature = 0.1f;   // Slightly warm
    post_processing_.saturation = 1.05f;
    lighting_settings_.sun_intensity_multiplier = 1.0f;
}

void EnvironmentController::apply_golden_hour() {
    set_time_of_day(18.5f);
    post_processing_.temperature = 0.3f;   // Very warm
    post_processing_.tint = -0.05f;        // Slightly green
    post_processing_.saturation = 1.15f;   // Boosted
    post_processing_.contrast = 1.1f;
    lighting_settings_.sun_intensity_multiplier = 0.9f;
}

void EnvironmentController::apply_dusk() {
    set_time_of_day(20.0f);
    post_processing_.temperature = -0.1f;  // Cool
    post_processing_.tint = 0.1f;          // Magenta
    post_processing_.saturation = 0.85f;
    lighting_settings_.sun_intensity_multiplier = 0.3f;
}

void EnvironmentController::apply_night() {
    set_time_of_day(0.0f);
    post_processing_.temperature = -0.2f;  // Cool blue
    post_processing_.saturation = 0.7f;
    post_processing_.brightness = -0.3f;   // Darker
    lighting_settings_.sun_intensity_multiplier = 0.0f;
    lighting_settings_.ambient_intensity_multiplier = 0.3f;
}

void EnvironmentController::apply_midnight() {
    set_time_of_day(2.0f);
    post_processing_.temperature = -0.25f;
    post_processing_.saturation = 0.6f;
    post_processing_.brightness = -0.5f;
    post_processing_.contrast = 0.9f;
    lighting_settings_.sun_intensity_multiplier = 0.0f;
    lighting_settings_.ambient_intensity_multiplier = 0.2f;
}

// ============================================================================
// Post-Processing Control
// ============================================================================

void EnvironmentController::set_exposure(float ev_offset) {
    post_processing_.exposure_ev = std::clamp(ev_offset, -5.0f, 5.0f);
}

void EnvironmentController::set_contrast(float contrast) {
    post_processing_.contrast = std::clamp(contrast, 0.5f, 2.0f);
}

void EnvironmentController::set_brightness(float brightness) {
    post_processing_.brightness = std::clamp(brightness, -1.0f, 1.0f);
}

void EnvironmentController::set_color_temperature(float temperature) {
    post_processing_.temperature = std::clamp(temperature, -1.0f, 1.0f);
}

void EnvironmentController::set_saturation(float saturation) {
    post_processing_.saturation = std::clamp(saturation, 0.0f, 2.0f);
}

void EnvironmentController::set_vignette(float intensity) {
    post_processing_.vignette_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

// ============================================================================
// Lighting Control
// ============================================================================

void EnvironmentController::set_sun_intensity(float multiplier) {
    lighting_settings_.sun_intensity_multiplier = std::clamp(multiplier, 0.5f, 2.0f);
}

void EnvironmentController::set_ambient_intensity(float multiplier) {
    lighting_settings_.ambient_intensity_multiplier = std::clamp(multiplier, 0.0f, 2.0f);
}

void EnvironmentController::set_shadow_strength(float strength) {
    lighting_settings_.shadow_strength = std::clamp(strength, 0.0f, 1.0f);
}

void EnvironmentController::set_fog_density(float multiplier) {
    lighting_settings_.fog_density_multiplier = std::clamp(multiplier, 0.1f, 5.0f);
}

void EnvironmentController::set_fog_color(const math::Vec3& color) {
    lighting_settings_.fog_color_override = color;
}

// ============================================================================
// Preset Management
// ============================================================================

void EnvironmentController::apply_preset(const std::string& preset_name) {
    auto it = presets_.find(preset_name);
    if (it == presets_.end()) {
        return;  // Preset not found
    }

    const Preset& preset = it->second;
    current_time_hours_ = preset.time_of_day_hours;
    current_day_of_year_ = preset.day_of_year;
    current_latitude_ = preset.latitude_degrees;
    post_processing_ = preset.post_processing;
    lighting_settings_ = preset.lighting;
}

void EnvironmentController::save_preset(const std::string& preset_name) {
    Preset preset;
    preset.name = preset_name;
    preset.time_of_day_hours = current_time_hours_;
    preset.day_of_year = current_day_of_year_;
    preset.latitude_degrees = current_latitude_;
    preset.post_processing = post_processing_;
    preset.lighting = lighting_settings_;

    presets_[preset_name] = preset;
}

std::vector<std::string> EnvironmentController::get_preset_names() const {
    std::vector<std::string> names;
    names.reserve(presets_.size());
    for (const auto& [name, preset] : presets_) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Built-In Presets
// ============================================================================

EnvironmentController::Preset EnvironmentController::create_mirrors_edge_bright() {
    Preset preset;
    preset.name = "Mirror's Edge Bright";
    preset.time_of_day_hours = 12.0f;  // Noon
    preset.day_of_year = 172;          // Summer
    preset.latitude_degrees = 0.0f;    // Equator (high sun)

    // Post-processing: High contrast, clean
    preset.post_processing.exposure_ev = 0.3f;      // Slightly brighter
    preset.post_processing.contrast = 1.3f;         // High contrast!
    preset.post_processing.brightness = 0.1f;       // Bright
    preset.post_processing.gamma = 2.0f;            // Slightly less gamma (punchier)
    preset.post_processing.temperature = 0.05f;     // Slightly warm
    preset.post_processing.tint = 0.0f;
    preset.post_processing.saturation = 0.9f;       // Slightly desaturated
    preset.post_processing.vibrance = 0.2f;         // Vibrant accents
    preset.post_processing.vignette_intensity = 0.0f; // No vignette
    preset.post_processing.ao_intensity = 0.8f;     // Strong AO for depth

    // Lighting: Harsh, clear
    preset.lighting.sun_intensity_multiplier = 1.2f;   // Bright sun
    preset.lighting.ambient_intensity_multiplier = 0.4f; // Low ambient (high contrast)
    preset.lighting.sky_intensity_multiplier = 0.3f;
    preset.lighting.shadow_strength = 1.0f;            // Full strength shadows
    preset.lighting.fog_density_multiplier = 0.3f;     // Minimal fog
    preset.lighting.volumetric_intensity_multiplier = 0.2f; // Subtle god rays

    preset.atmospheric_preset = "mirrors_edge_style";

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_mirrors_edge_indoor() {
    Preset preset = create_mirrors_edge_bright();
    preset.name = "Mirror's Edge Indoor";

    // Indoor: More ambient, softer shadows
    preset.post_processing.contrast = 1.15f;  // Lower contrast
    preset.lighting.sun_intensity_multiplier = 0.7f; // Indirect sun
    preset.lighting.ambient_intensity_multiplier = 0.8f; // More ambient
    preset.lighting.shadow_strength = 0.7f;    // Softer shadows
    preset.lighting.fog_density_multiplier = 0.0f; // No fog

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_high_contrast() {
    Preset preset;
    preset.name = "High Contrast";
    preset.time_of_day_hours = 13.0f;  // Early afternoon

    preset.post_processing.contrast = 1.5f;     // Very high!
    preset.post_processing.brightness = 0.0f;
    preset.post_processing.saturation = 1.0f;
    preset.post_processing.gamma = 2.0f;

    preset.lighting.sun_intensity_multiplier = 1.3f;
    preset.lighting.ambient_intensity_multiplier = 0.3f; // Very low ambient
    preset.lighting.shadow_strength = 1.0f;

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_low_contrast() {
    Preset preset;
    preset.name = "Low Contrast Soft";
    preset.time_of_day_hours = 10.0f;

    preset.post_processing.contrast = 0.75f;    // Low contrast
    preset.post_processing.saturation = 0.85f;
    preset.post_processing.gamma = 2.4f;        // Softer

    preset.lighting.sun_intensity_multiplier = 0.8f;
    preset.lighting.ambient_intensity_multiplier = 0.9f; // High ambient
    preset.lighting.shadow_strength = 0.6f;

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_warm_sunset() {
    Preset preset;
    preset.name = "Warm Sunset";
    preset.time_of_day_hours = 18.5f;

    preset.post_processing.temperature = 0.35f;  // Very warm
    preset.post_processing.tint = -0.05f;
    preset.post_processing.saturation = 1.2f;
    preset.post_processing.contrast = 1.15f;

    preset.lighting.sun_intensity_multiplier = 0.9f;
    preset.lighting.ambient_intensity_multiplier = 0.6f;

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_cool_morning() {
    Preset preset;
    preset.name = "Cool Morning";
    preset.time_of_day_hours = 7.0f;

    preset.post_processing.temperature = -0.15f; // Cool blue
    preset.post_processing.tint = 0.05f;
    preset.post_processing.saturation = 0.9f;
    preset.post_processing.contrast = 1.05f;

    preset.lighting.sun_intensity_multiplier = 0.8f;
    preset.lighting.ambient_intensity_multiplier = 0.7f;

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_neutral_noon() {
    Preset preset;
    preset.name = "Neutral Noon";
    preset.time_of_day_hours = 12.0f;

    // Everything neutral
    preset.post_processing.temperature = 0.0f;
    preset.post_processing.tint = 0.0f;
    preset.post_processing.saturation = 1.0f;
    preset.post_processing.contrast = 1.0f;
    preset.post_processing.brightness = 0.0f;

    preset.lighting.sun_intensity_multiplier = 1.0f;
    preset.lighting.ambient_intensity_multiplier = 1.0f;
    preset.lighting.shadow_strength = 1.0f;

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_moody_overcast() {
    Preset preset;
    preset.name = "Moody Overcast";
    preset.time_of_day_hours = 14.0f;

    preset.post_processing.saturation = 0.7f;   // Low saturation
    preset.post_processing.contrast = 0.9f;     // Low contrast
    preset.post_processing.brightness = -0.1f;  // Darker
    preset.post_processing.temperature = -0.05f; // Slightly cool

    preset.lighting.sun_intensity_multiplier = 0.5f; // Overcast sun
    preset.lighting.ambient_intensity_multiplier = 0.9f;
    preset.lighting.shadow_strength = 0.4f;     // Soft shadows
    preset.lighting.fog_density_multiplier = 2.0f; // Fog

    preset.atmospheric_preset = "earth_overcast";

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_cinematic_night() {
    Preset preset;
    preset.name = "Cinematic Night";
    preset.time_of_day_hours = 22.0f;

    preset.post_processing.temperature = -0.25f; // Blue tint
    preset.post_processing.saturation = 0.75f;
    preset.post_processing.contrast = 1.2f;     // High contrast
    preset.post_processing.brightness = -0.4f;  // Dark
    preset.post_processing.vignette_intensity = 0.3f;

    preset.lighting.sun_intensity_multiplier = 0.0f;
    preset.lighting.ambient_intensity_multiplier = 0.25f;
    preset.lighting.shadow_strength = 0.8f;

    preset.atmospheric_preset = "earth_night";

    return preset;
}

EnvironmentController::Preset EnvironmentController::create_vibrant_day() {
    Preset preset;
    preset.name = "Vibrant Day";
    preset.time_of_day_hours = 14.0f;

    preset.post_processing.saturation = 1.4f;   // Very saturated!
    preset.post_processing.vibrance = 0.3f;
    preset.post_processing.contrast = 1.15f;
    preset.post_processing.temperature = 0.05f;

    preset.lighting.sun_intensity_multiplier = 1.1f;
    preset.lighting.ambient_intensity_multiplier = 0.6f;

    return preset;
}

// ============================================================================
// Internal Methods
// ============================================================================

void EnvironmentController::apply_post_processing(World& world) {
    // In real implementation, would update post-processing pipeline
    // For now, just store settings
}

void EnvironmentController::apply_lighting(World& world) {
    // In real implementation, would update AtmosphericComponent and lighting system
    // For now, just store settings
}

void EnvironmentController::update_transition(float delta_time_s) {
    transition_elapsed_ += delta_time_s;

    if (transition_elapsed_ >= transition_duration_) {
        // Transition complete
        transitioning_ = false;
        current_time_hours_ = target_time_hours_;
        return;
    }

    // Lerp time
    float t = transition_elapsed_ / transition_duration_;
    current_time_hours_ = current_time_hours_ * (1.0f - t) + target_time_hours_ * t;
}

} // namespace lore::ecs