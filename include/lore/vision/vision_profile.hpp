#pragma once

#include <lore/math/math.hpp>
#include <cstdint>

namespace lore::vision {

// Entity's vision capabilities
struct VisionProfile {
    // Core vision attributes
    float base_range_meters = 50.0f;        // Maximum vision distance in ideal conditions
    float fov_angle_degrees = 210.0f;       // Field of view (humans: ~210°, focused: ~60°)
    float eye_height_meters = 1.7f;         // Height of eyes above ground (affects over-obstacle viewing)

    // Vision quality modifiers
    float perception = 0.5f;                // Ability to notice details (0.0-1.0)
    float night_vision = 0.0f;              // Low-light vision ability (0.0-1.0, 1.0 = full cat-like vision)
    float visual_acuity = 1.0f;             // Clarity at distance (1.0 = 20/20 vision)

    // Special vision types
    bool has_thermal_vision = false;        // See heat signatures (ignores smoke/darkness)
    bool has_xray_vision = false;           // See through walls (debug/special ability)

    // Status effects
    bool is_blind = false;                  // Cannot see at all
    bool is_dazed = false;                  // Reduced vision range and clarity

    // Focused/aiming mode
    float focused_fov_angle = 60.0f;        // Narrow FOV when aiming
    float focus_range_bonus = 1.5f;         // Range multiplier when focused (ADS, binoculars)
};

// Environmental conditions that affect vision
struct EnvironmentalConditions {
    // Time and lighting
    float time_of_day = 0.5f;               // 0.0 = midnight, 0.5 = noon, 1.0 = midnight
    float ambient_light_level = 1.0f;       // 0.0 = pitch black, 1.0 = bright daylight

    // Weather conditions
    float fog_density = 0.0f;               // 0.0 = clear, 1.0 = opaque fog
    float rain_intensity = 0.0f;            // 0.0 = no rain, 1.0 = heavy downpour
    float snow_intensity = 0.0f;            // 0.0 = clear, 1.0 = blizzard
    float dust_density = 0.0f;              // Airborne particles (sandstorm, etc.)

    // Dynamic occlusion (local effects)
    float smoke_density = 0.0f;             // Smoke from fires, grenades, etc.
    float gas_density = 0.0f;               // Tear gas, chemical weapons

    // Calculate effective ambient light (considers time of day)
    float get_effective_light_level() const {
        // Simple day/night cycle
        // 0.0-0.2 = night (0.1), 0.2-0.3 = dawn (0.5), 0.3-0.7 = day (1.0), 0.7-0.8 = dusk (0.5), 0.8-1.0 = night (0.1)
        float light_from_time;
        if (time_of_day < 0.2f || time_of_day > 0.8f) {
            light_from_time = 0.1f; // Night
        } else if (time_of_day < 0.3f || time_of_day > 0.7f) {
            light_from_time = 0.5f; // Dawn/dusk
        } else {
            light_from_time = 1.0f; // Day
        }

        return std::min(ambient_light_level, light_from_time);
    }

    // Calculate visibility reduction from weather
    float get_weather_visibility_modifier() const {
        float modifier = 1.0f;

        // Fog: exponential falloff with distance
        modifier *= (1.0f - fog_density * 0.8f);

        // Rain: linear reduction
        modifier *= (1.0f - rain_intensity * 0.3f);

        // Snow: moderate reduction
        modifier *= (1.0f - snow_intensity * 0.5f);

        // Dust/smoke: heavy reduction
        modifier *= (1.0f - dust_density * 0.7f);
        modifier *= (1.0f - smoke_density * 0.9f);
        modifier *= (1.0f - gas_density * 0.6f);

        return std::max(0.0f, modifier);
    }
};

// Tile occlusion properties (part of TileDefinition)
struct TileOcclusion {
    bool blocks_sight = false;              // Full occlusion (solid wall)
    float transparency = 0.0f;              // 0.0 = opaque, 1.0 = fully transparent (window, water)
    float height_meters = 0.0f;             // Physical height of obstacle
    bool is_foliage = false;                // Vegetation (partial occlusion, can see through with perception)
};

// Calculate effective vision range for an entity
inline float calculate_effective_vision_range(
    const VisionProfile& profile,
    const EnvironmentalConditions& env,
    bool is_focused = false)
{
    // Blind entities cannot see
    if (profile.is_blind) {
        return 0.0f;
    }

    float base_range = profile.base_range_meters;

    // Apply focus bonus if aiming/using optics
    if (is_focused) {
        base_range *= profile.focus_range_bonus;
    }

    // Light level affects vision range
    float light_level = env.get_effective_light_level();
    // Night vision compensates for darkness
    light_level = std::max(light_level, profile.night_vision);

    // Thermal vision ignores weather/smoke
    float env_modifier = 1.0f;
    if (!profile.has_thermal_vision) {
        env_modifier = env.get_weather_visibility_modifier();
    }

    // Visual acuity affects effective range
    float acuity_modifier = profile.visual_acuity;

    // Dazed reduces vision
    if (profile.is_dazed) {
        acuity_modifier *= 0.5f;
    }

    return base_range * light_level * env_modifier * acuity_modifier;
}

} // namespace lore::vision