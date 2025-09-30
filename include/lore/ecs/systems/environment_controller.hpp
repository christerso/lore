#pragma once

#include "lore/ecs/components/atmospheric_component.hpp"
#include "lore/core/math_types.hpp"
#include <string>
#include <vector>

namespace lore::ecs {

/**
 * @brief Real-time environment and post-processing controller
 *
 * Allows artists/designers to tweak lighting, time of day, contrast, and colors
 * at runtime without code changes. Perfect for prototyping and iteration.
 *
 * Features:
 * - Time of day control (0-24 hours, automatic sun position)
 * - Quick time presets (dawn, noon, dusk, night, golden hour)
 * - Contrast and brightness adjustment
 * - Color grading (temperature, tint, saturation)
 * - Exposure control (EV compensation)
 * - Fog density and color
 * - Shadow strength
 * - Ambient light intensity
 * - Real-time preview (no restart required)
 * - Preset save/load system
 * - Smooth transitions between settings
 *
 * Usage:
 * @code
 * // Initialize
 * EnvironmentController env_controller;
 * env_controller.initialize(world);
 *
 * // Change time of day
 * env_controller.set_time_of_day(18.5f);  // 6:30 PM (sunset)
 *
 * // Increase contrast (Mirror's Edge style)
 * env_controller.set_contrast(1.3f);  // 30% more contrast
 *
 * // Warm color temperature
 * env_controller.set_color_temperature(0.2f);  // Warmer (orange tint)
 *
 * // Apply preset
 * env_controller.apply_preset("mirrors_edge_bright");
 *
 * // Smooth transition to night
 * env_controller.transition_to_time(22.0f, 5.0f);  // 10 PM over 5 seconds
 * @endcode
 */
class EnvironmentController {
public:
    /**
     * @brief Post-processing settings
     */
    struct PostProcessing {
        // Tone mapping
        float exposure_ev = 0.0f;           // EV compensation (-5 to +5)
        float exposure_min = 0.03f;         // Minimum luminance
        float exposure_max = 8.0f;          // Maximum luminance
        bool auto_exposure = false;         // Automatic exposure adaptation

        // Contrast and brightness
        float contrast = 1.0f;              // Contrast (0.5 to 2.0, 1.0 = neutral)
        float brightness = 0.0f;            // Brightness offset (-1 to +1)
        float gamma = 2.2f;                 // Gamma correction (1.0 to 3.0)

        // Color grading
        float temperature = 0.0f;           // Color temperature (-1 to +1, 0 = neutral)
                                            // Negative = cooler (blue), Positive = warmer (orange)
        float tint = 0.0f;                  // Green/Magenta tint (-1 to +1)
        float saturation = 1.0f;            // Saturation (0 to 2, 1 = neutral)
        float vibrance = 0.0f;              // Vibrance (-1 to +1, affects less-saturated colors more)

        // Color balance (shadows, midtones, highlights)
        math::Vec3 lift{0.0f, 0.0f, 0.0f};      // Shadows color shift
        math::Vec3 gamma_color{1.0f, 1.0f, 1.0f}; // Midtones color shift
        math::Vec3 gain{1.0f, 1.0f, 1.0f};      // Highlights color shift

        // Vignette
        float vignette_intensity = 0.0f;    // Vignette strength (0 to 1)
        float vignette_smoothness = 0.5f;   // Vignette falloff

        // Ambient occlusion
        float ao_intensity = 0.8f;          // AO strength (0 to 2)
        float ao_radius = 0.5f;             // AO sample radius
    };

    /**
     * @brief Lighting settings
     */
    struct LightingSettings {
        // Sun/Sky
        float sun_intensity_multiplier = 1.0f;   // Sun brightness (0.5 to 2.0)
        float ambient_intensity_multiplier = 1.0f; // Ambient light (0.0 to 2.0)
        float sky_intensity_multiplier = 1.0f;   // Sky light contribution

        // Shadows
        float shadow_strength = 1.0f;       // Shadow darkness (0 to 1)
        float shadow_bias = 0.002f;         // Shadow acne prevention

        // Fog
        float fog_density_multiplier = 1.0f; // Fog thickness (0.1 to 5.0)
        math::Vec3 fog_color_override{0.0f, 0.0f, 0.0f}; // Override fog color (0 = use atmospheric)

        // Volumetrics
        float volumetric_intensity_multiplier = 1.0f; // God rays strength
    };

    /**
     * @brief Complete environment preset
     */
    struct Preset {
        std::string name;
        float time_of_day_hours = 12.0f;
        uint32_t day_of_year = 172;         // June 21
        float latitude_degrees = 0.0f;
        PostProcessing post_processing;
        LightingSettings lighting;
        std::string atmospheric_preset = "earth_clear_day";
    };

    EnvironmentController() = default;
    ~EnvironmentController() = default;

    /**
     * @brief Initialize controller with world
     * @param world ECS world
     * @return true if atmospheric component found
     */
    bool initialize(World& world);

    /**
     * @brief Update controller (handles transitions)
     * @param world ECS world
     * @param delta_time_s Time since last update
     */
    void update(World& world, float delta_time_s);

    // ========================================================================
    // TIME OF DAY CONTROL
    // ========================================================================

    /**
     * @brief Set time of day (0-24 hours)
     * Instantly updates sun position
     */
    void set_time_of_day(float hours);

    /**
     * @brief Smoothly transition to new time of day
     * @param target_hours Target time (0-24)
     * @param duration_s Transition duration in seconds
     */
    void transition_to_time(float target_hours, float duration_s);

    /**
     * @brief Advance time (for day/night cycle)
     * @param hours_per_second Real-time to game-time ratio (e.g., 0.01 = 1 hour per 100 seconds)
     */
    void advance_time(float hours_per_second, float delta_time_s);

    /**
     * @brief Set day of year (1-365)
     * Affects sun angle and seasonal lighting
     */
    void set_day_of_year(uint32_t day);

    /**
     * @brief Set latitude (-90 to +90 degrees)
     * Affects sun path and angle
     */
    void set_latitude(float degrees);

    // ========================================================================
    // QUICK TIME PRESETS
    // ========================================================================

    void apply_dawn();          // 6:00 AM - Soft pink/orange
    void apply_morning();       // 9:00 AM - Bright, clear
    void apply_noon();          // 12:00 PM - Harsh, high contrast
    void apply_afternoon();     // 15:00 PM - Warm
    void apply_golden_hour();   // 18:30 PM - Golden sunset
    void apply_dusk();          // 20:00 PM - Purple/blue
    void apply_night();         // 0:00 AM - Moonlight
    void apply_midnight();      // 2:00 AM - Very dark

    // ========================================================================
    // POST-PROCESSING CONTROL
    // ========================================================================

    /**
     * @brief Set exposure (EV compensation)
     * @param ev_offset -5 to +5 (0 = neutral, +1 = 2× brighter, -1 = 2× darker)
     */
    void set_exposure(float ev_offset);

    /**
     * @brief Set contrast
     * @param contrast 0.5 to 2.0 (1.0 = neutral, >1.0 = more contrast, <1.0 = less)
     */
    void set_contrast(float contrast);

    /**
     * @brief Set brightness offset
     * @param brightness -1 to +1 (0 = neutral)
     */
    void set_brightness(float brightness);

    /**
     * @brief Set color temperature
     * @param temperature -1 to +1 (0 = neutral, <0 = cooler/blue, >0 = warmer/orange)
     */
    void set_color_temperature(float temperature);

    /**
     * @brief Set saturation
     * @param saturation 0 to 2 (0 = grayscale, 1 = neutral, 2 = oversaturated)
     */
    void set_saturation(float saturation);

    /**
     * @brief Set vignette intensity
     * @param intensity 0 to 1 (0 = none, 1 = strong)
     */
    void set_vignette(float intensity);

    // ========================================================================
    // LIGHTING CONTROL
    // ========================================================================

    /**
     * @brief Set sun intensity multiplier
     * @param multiplier 0.5 to 2.0 (1.0 = default)
     */
    void set_sun_intensity(float multiplier);

    /**
     * @brief Set ambient light intensity
     * @param multiplier 0.0 to 2.0 (1.0 = default)
     */
    void set_ambient_intensity(float multiplier);

    /**
     * @brief Set shadow strength
     * @param strength 0 to 1 (0 = no shadows, 1 = full strength)
     */
    void set_shadow_strength(float strength);

    /**
     * @brief Set fog density multiplier
     * @param multiplier 0.1 to 5.0 (1.0 = default)
     */
    void set_fog_density(float multiplier);

    /**
     * @brief Override fog color
     * @param color RGB color (if length = 0, uses atmospheric color)
     */
    void set_fog_color(const math::Vec3& color);

    // ========================================================================
    // PRESET MANAGEMENT
    // ========================================================================

    /**
     * @brief Apply complete preset
     */
    void apply_preset(const std::string& preset_name);

    /**
     * @brief Save current settings as preset
     */
    void save_preset(const std::string& preset_name);

    /**
     * @brief Load preset from file
     */
    bool load_preset_file(const std::string& filepath);

    /**
     * @brief Get list of available presets
     */
    std::vector<std::string> get_preset_names() const;

    /**
     * @brief Get current settings
     */
    const PostProcessing& get_post_processing() const { return post_processing_; }
    const LightingSettings& get_lighting() const { return lighting_settings_; }

    /**
     * @brief Direct access to settings (for UI)
     */
    PostProcessing& get_post_processing_mut() { return post_processing_; }
    LightingSettings& get_lighting_mut() { return lighting_settings_; }

    // ========================================================================
    // BUILT-IN PRESETS
    // ========================================================================

    static Preset create_mirrors_edge_bright();  // Mirror's Edge bright prototype
    static Preset create_mirrors_edge_indoor();  // Indoor with ambient
    static Preset create_high_contrast();        // Dramatic shadows
    static Preset create_low_contrast();         // Soft, diffuse
    static Preset create_warm_sunset();          // Warm orange/red
    static Preset create_cool_morning();         // Cool blue/cyan
    static Preset create_neutral_noon();         // Neutral midday
    static Preset create_moody_overcast();       // Low saturation, gray
    static Preset create_cinematic_night();      // Dark, blue tint
    static Preset create_vibrant_day();          // High saturation

private:
    /**
     * @brief Apply post-processing settings to render pipeline
     */
    void apply_post_processing(World& world);

    /**
     * @brief Apply lighting settings to atmospheric component
     */
    void apply_lighting(World& world);

    /**
     * @brief Handle smooth transitions
     */
    void update_transition(float delta_time_s);

    // Current settings
    PostProcessing post_processing_;
    LightingSettings lighting_settings_;

    // Current time
    float current_time_hours_ = 12.0f;
    uint32_t current_day_of_year_ = 172;
    float current_latitude_ = 0.0f;

    // Transition state
    bool transitioning_ = false;
    float target_time_hours_ = 12.0f;
    float transition_duration_ = 0.0f;
    float transition_elapsed_ = 0.0f;
    PostProcessing transition_start_post_;
    PostProcessing transition_target_post_;

    // Presets
    std::unordered_map<std::string, Preset> presets_;

    // Entity references
    Entity atmospheric_entity_ = INVALID_ENTITY;

    bool initialized_ = false;
};

} // namespace lore::ecs