#include "lore/ecs/components/heat_distortion_component.hpp"
#include <cmath>
#include <algorithm>

namespace lore::ecs {

// ============================================================================
// FACTORY METHODS - PRESETS
// ============================================================================

HeatDistortionComponent HeatDistortionComponent::create_small_fire() {
    HeatDistortionComponent comp;

    // Gentle shimmer for campfire/small flame
    comp.base_strength = 0.015f;
    comp.temperature_scale = 0.00003f;
    comp.max_strength = 0.04f;

    // 2 meter radius
    comp.inner_radius_m = 0.3f;
    comp.outer_radius_m = 2.0f;
    comp.vertical_bias = 1.4f;
    comp.height_falloff_m = 3.0f;

    // Slow, gentle shimmer
    comp.noise_frequency = 1.5f;
    comp.noise_octaves = 2;
    comp.noise_amplitude = 0.8f;
    comp.vertical_speed_m_s = 0.3f;
    comp.turbulence_scale = 0.2f;

    // No shockwave for continuous fire
    comp.shockwave_enabled = false;

    comp.quality = QualityPreset::Medium;

    return comp;
}

HeatDistortionComponent HeatDistortionComponent::create_large_fire() {
    HeatDistortionComponent comp;

    // Strong shimmer for bonfire/building fire
    comp.base_strength = 0.03f;
    comp.temperature_scale = 0.00006f;
    comp.max_strength = 0.08f;

    // 5 meter radius
    comp.inner_radius_m = 0.8f;
    comp.outer_radius_m = 5.0f;
    comp.vertical_bias = 1.6f;
    comp.height_falloff_m = 8.0f;

    // Faster, more chaotic shimmer
    comp.noise_frequency = 2.5f;
    comp.noise_octaves = 3;
    comp.noise_amplitude = 1.2f;
    comp.vertical_speed_m_s = 0.6f;
    comp.turbulence_scale = 0.4f;

    comp.shockwave_enabled = false;

    comp.quality = QualityPreset::High;

    return comp;
}

HeatDistortionComponent HeatDistortionComponent::create_explosion_shockwave() {
    HeatDistortionComponent comp;

    // Strong base for explosion heat
    comp.base_strength = 0.05f;
    comp.temperature_scale = 0.0001f;
    comp.max_strength = 0.12f;

    // Large radius for explosion
    comp.inner_radius_m = 2.0f;
    comp.outer_radius_m = 10.0f;
    comp.vertical_bias = 1.2f; // More spherical
    comp.height_falloff_m = 10.0f;

    // Rapid, intense shimmer
    comp.noise_frequency = 4.0f;
    comp.noise_octaves = 2; // Performance for many explosions
    comp.noise_amplitude = 1.5f;
    comp.vertical_speed_m_s = 2.0f;
    comp.turbulence_scale = 0.6f;

    // Shockwave enabled
    comp.shockwave_enabled = true;
    comp.shockwave_strength = 0.15f;
    comp.shockwave_speed_m_s = 500.0f;
    comp.shockwave_duration_s = 0.3f;
    comp.shockwave_thickness_m = 1.0f;
    comp.shockwave_time_s = 0.0f; // Start immediately

    comp.quality = QualityPreset::Medium; // Performance

    return comp;
}

HeatDistortionComponent HeatDistortionComponent::create_torch() {
    HeatDistortionComponent comp;

    // Very subtle for torch
    comp.base_strength = 0.01f;
    comp.temperature_scale = 0.00002f;
    comp.max_strength = 0.03f;

    // Small 1 meter radius
    comp.inner_radius_m = 0.1f;
    comp.outer_radius_m = 1.0f;
    comp.vertical_bias = 1.3f;
    comp.height_falloff_m = 1.5f;

    // Slow, steady shimmer
    comp.noise_frequency = 1.0f;
    comp.noise_octaves = 2;
    comp.noise_amplitude = 0.6f;
    comp.vertical_speed_m_s = 0.2f;
    comp.turbulence_scale = 0.15f;

    comp.shockwave_enabled = false;

    comp.quality = QualityPreset::Low; // Performance for many torches

    return comp;
}

HeatDistortionComponent HeatDistortionComponent::create_exhaust() {
    HeatDistortionComponent comp;

    // Extreme distortion for jet/rocket
    comp.base_strength = 0.06f;
    comp.temperature_scale = 0.00008f;
    comp.max_strength = 0.15f;

    // Directional cone
    comp.inner_radius_m = 0.5f;
    comp.outer_radius_m = 6.0f;
    comp.vertical_bias = 2.0f; // Highly directional
    comp.height_falloff_m = 12.0f;

    // Very fast, turbulent shimmer
    comp.noise_frequency = 5.0f;
    comp.noise_octaves = 3;
    comp.noise_amplitude = 2.0f;
    comp.vertical_speed_m_s = 5.0f; // Fast exhaust velocity
    comp.turbulence_scale = 0.8f;

    comp.shockwave_enabled = false;

    comp.quality = QualityPreset::High;

    return comp;
}

// ============================================================================
// QUALITY PRESETS
// ============================================================================

void HeatDistortionComponent::apply_quality_preset(QualityPreset preset) {
    quality = preset;

    switch (preset) {
        case QualityPreset::Low:
            noise_octaves = 1;
            noise_amplitude = 0.8f;
            break;

        case QualityPreset::Medium:
            noise_octaves = 2;
            noise_amplitude = 1.0f;
            break;

        case QualityPreset::High:
            noise_octaves = 3;
            noise_amplitude = 1.0f;
            break;

        case QualityPreset::Ultra:
            noise_octaves = 4;
            noise_amplitude = 1.2f;
            break;
    }
}

// ============================================================================
// SHOCKWAVE
// ============================================================================

void HeatDistortionComponent::trigger_shockwave(float explosion_radius, float explosion_intensity) {
    if (!shockwave_enabled) return;

    // Reset shockwave timer
    shockwave_time_s = 0.0f;

    // Scale shockwave parameters based on explosion size
    outer_radius_m = explosion_radius * 1.5f; // Shockwave travels beyond fireball
    shockwave_strength = 0.15f * explosion_intensity;

    // Larger explosions = slower perceived shockwave (but still supersonic)
    // This is for visual effect - shockwave visible for longer
    if (explosion_radius > 10.0f) {
        shockwave_duration_s = 0.5f;
    } else if (explosion_radius > 5.0f) {
        shockwave_duration_s = 0.4f;
    } else {
        shockwave_duration_s = 0.3f;
    }
}

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

void HeatDistortionComponent::update_from_fire(float fire_temperature_k, const math::Vec3& fire_position) {
    source_temperature_k = fire_temperature_k;
    source_position = fire_position;
}

float HeatDistortionComponent::calculate_strength_at_position(const math::Vec3& world_pos, float time_s) const {
    if (!enabled) return 0.0f;

    // Distance from heat source
    math::Vec3 offset = world_pos - source_position;
    float horizontal_dist = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    float vertical_dist = offset.y;

    // Base strength from temperature
    float temp_strength = base_strength + (source_temperature_k - ambient_temperature_k) * temperature_scale;
    temp_strength = std::min(temp_strength, max_strength);

    // Spatial falloff
    float radial_falloff = 1.0f;
    if (horizontal_dist > inner_radius_m) {
        if (horizontal_dist >= outer_radius_m) {
            radial_falloff = 0.0f;
        } else {
            // Smooth falloff (cubic hermite)
            float t = (horizontal_dist - inner_radius_m) / (outer_radius_m - inner_radius_m);
            radial_falloff = 1.0f - t * t * (3.0f - 2.0f * t);
        }
    }

    // Vertical falloff (heat rises, weakens with height)
    float vertical_falloff = 1.0f;
    if (vertical_dist < 0.0f) {
        // Below source - rapid falloff
        vertical_falloff = std::exp(vertical_dist * 2.0f / height_falloff_m);
    } else {
        // Above source - gradual falloff with vertical bias
        vertical_falloff = std::exp(-vertical_dist / (height_falloff_m * vertical_bias));
    }

    float base_result = temp_strength * radial_falloff * vertical_falloff;

    // Shockwave contribution
    if (shockwave_enabled && shockwave_time_s >= 0.0f && shockwave_time_s <= shockwave_duration_s) {
        float shockwave_radius = shockwave_speed_m_s * shockwave_time_s;
        float dist_to_wavefront = std::abs(horizontal_dist - shockwave_radius);

        // Shockwave visible within thickness band
        if (dist_to_wavefront < shockwave_thickness_m) {
            // Smooth band profile
            float band_t = dist_to_wavefront / shockwave_thickness_m;
            float band_profile = 1.0f - band_t * band_t;

            // Fade shockwave over duration
            float time_fade = 1.0f - (shockwave_time_s / shockwave_duration_s);

            float shockwave_contrib = shockwave_strength * band_profile * time_fade;
            base_result += shockwave_contrib;
        }
    }

    return std::min(base_result, max_strength);
}

// ============================================================================
// INI LOADING
// ============================================================================

HeatDistortionComponent HeatDistortionComponent::load_from_ini(const std::string& filepath) {
    HeatDistortionComponent comp;

    // TODO: Implement INI parser integration
    // For now, return default preset
    // This will be connected to the engine's INI system

    return comp;
}

} // namespace lore::ecs