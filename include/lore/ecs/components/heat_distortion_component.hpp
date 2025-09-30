#pragma once

#include "lore/math/vec3.hpp"
#include <string>

namespace lore::ecs {

/**
 * @brief Heat distortion effect component for realistic heat shimmer and refraction
 *
 * Physical basis:
 * - Hot air has lower refractive index than cool air (Gladstone-Dale relation)
 * - Temperature gradient causes light bending (Snell's law at interfaces)
 * - Creates visible shimmer effect above heat sources (fires, explosions)
 * - Rising hot air creates vertical distortion patterns
 *
 * Real-world measurements:
 * - Air refractive index: ~1.000293 at 0°C, ~1.000277 at 100°C
 * - Δn/ΔT ≈ -1.0×10⁻⁶ per °C (refractive index decrease with temperature)
 * - Typical distortion: 1-5 pixels for campfire, 10-30 pixels for large fire
 *
 * Implementation:
 * - Screen-space post-process effect
 * - UV offset based on temperature field
 * - Perlin noise for realistic shimmer
 * - Vertical bias for rising hot air
 * - Integration with VolumetricFireSystem and ExplosionComponent
 *
 * INI Configuration: data/config/heat_distortion.ini
 * Example:
 * ```ini
 * [General]
 * enabled = true
 * quality_preset = high
 *
 * [DistortionStrength]
 * base_strength = 0.02          # Base UV offset multiplier (0.0 - 0.1)
 * temperature_scale = 0.00005   # Strength per °C above ambient
 * max_strength = 0.08           # Maximum UV offset
 *
 * [SpatialFalloff]
 * inner_radius_m = 0.5          # No falloff within this radius
 * outer_radius_m = 3.0          # Zero distortion beyond this radius
 * vertical_bias = 1.5           # Stronger distortion above source
 * height_falloff_m = 5.0        # Distortion height extent
 *
 * [ShimmerAnimation]
 * noise_frequency = 2.0         # Noise frequency (Hz)
 * noise_octaves = 3             # Detail levels (1-6)
 * noise_amplitude = 1.0         # Noise strength multiplier
 * vertical_speed_m_s = 0.5      # Upward shimmer motion
 * turbulence_scale = 0.3        # Chaotic motion amount
 *
 * [ExplosionShockwave]
 * shockwave_enabled = true
 * shockwave_strength = 0.15     # Max distortion during shockwave
 * shockwave_speed_m_s = 500.0   # Expansion speed
 * shockwave_duration_s = 0.3    # Total duration
 * shockwave_thickness_m = 1.0   # Visible band thickness
 * ```
 */
struct HeatDistortionComponent {
    // ========================================================================
    // DISTORTION STRENGTH
    // ========================================================================

    /// Base distortion strength (UV offset multiplier: 0.0 - 0.1)
    /// 0.02 = subtle shimmer, 0.05 = visible heat waves, 0.08 = strong distortion
    float base_strength = 0.02f;

    /// Strength scaling with temperature (per °C above ambient)
    /// Formula: total_strength = base_strength + (T - T_ambient) * temperature_scale
    /// Default: 0.00005 (1000°C fire → +0.05 strength)
    float temperature_scale = 0.00005f;

    /// Maximum distortion strength (prevents excessive warping)
    float max_strength = 0.08f;

    /// Ambient temperature reference (Kelvin)
    float ambient_temperature_k = 293.15f; // 20°C

    // ========================================================================
    // SPATIAL FALLOFF
    // ========================================================================

    /// Inner radius - full distortion (meters)
    /// No falloff within this distance from heat source
    float inner_radius_m = 0.5f;

    /// Outer radius - zero distortion (meters)
    /// Smooth falloff from inner to outer radius
    float outer_radius_m = 3.0f;

    /// Vertical bias - stronger distortion above heat source
    /// 1.0 = symmetric, 1.5 = 50% stronger above, 2.0 = twice as strong above
    float vertical_bias = 1.5f;

    /// Height falloff - distortion vertical extent (meters)
    /// Heat shimmer visible up to this height above source
    float height_falloff_m = 5.0f;

    // ========================================================================
    // SHIMMER ANIMATION
    // ========================================================================

    /// Noise frequency (Hz) - shimmer speed
    /// 1.0 = slow shimmer, 2.0 = normal, 4.0 = rapid flickering
    float noise_frequency = 2.0f;

    /// Noise octaves - detail levels (1-6)
    /// 1 = smooth blobs, 3 = good detail, 6 = very fine detail
    int noise_octaves = 3;

    /// Noise amplitude multiplier
    /// Scales the noise contribution to distortion
    float noise_amplitude = 1.0f;

    /// Vertical motion speed (m/s)
    /// Heat shimmer rises with hot air
    float vertical_speed_m_s = 0.5f;

    /// Turbulence scale - chaotic horizontal motion
    /// 0.0 = pure vertical, 0.5 = some swirl, 1.0 = highly turbulent
    float turbulence_scale = 0.3f;

    // ========================================================================
    // EXPLOSION SHOCKWAVE
    // ========================================================================

    /// Enable shockwave distortion for explosions
    bool shockwave_enabled = true;

    /// Shockwave maximum strength (0.0 - 0.2)
    /// Much stronger than heat shimmer, creates visible circular wave
    float shockwave_strength = 0.15f;

    /// Shockwave expansion speed (m/s)
    /// Supersonic for explosions: 340-1000+ m/s
    float shockwave_speed_m_s = 500.0f;

    /// Shockwave total duration (seconds)
    /// How long the shockwave effect lasts
    float shockwave_duration_s = 0.3f;

    /// Shockwave visible thickness (meters)
    /// Width of the distortion band
    float shockwave_thickness_m = 1.0f;

    /// Current shockwave time (0.0 = start, duration_s = end)
    /// -1.0 = no active shockwave
    float shockwave_time_s = -1.0f;

    // ========================================================================
    // INTEGRATION
    // ========================================================================

    /// Source position (world space)
    /// Distortion centered at this point
    math::Vec3 source_position{0.0f, 0.0f, 0.0f};

    /// Current temperature at source (Kelvin)
    /// Auto-updated from VolumetricFireComponent or ExplosionComponent
    float source_temperature_k = 293.15f;

    /// Enabled flag
    bool enabled = true;

    // ========================================================================
    // QUALITY PRESETS
    // ========================================================================

    enum class QualityPreset {
        Low,    // 1 octave, reduced resolution
        Medium, // 2 octaves, normal resolution
        High,   // 3 octaves, full resolution
        Ultra   // 4 octaves, high resolution, expensive
    };

    QualityPreset quality = QualityPreset::High;

    // ========================================================================
    // FACTORY METHODS
    // ========================================================================

    /**
     * @brief Small fire/campfire distortion
     * Gentle shimmer, 2m radius
     */
    static HeatDistortionComponent create_small_fire();

    /**
     * @brief Large fire/bonfire distortion
     * Strong shimmer, 5m radius, tall column
     */
    static HeatDistortionComponent create_large_fire();

    /**
     * @brief Explosion shockwave distortion
     * Intense spherical wave, 10m radius, brief duration
     */
    static HeatDistortionComponent create_explosion_shockwave();

    /**
     * @brief Torch/small flame distortion
     * Very localized, 1m radius
     */
    static HeatDistortionComponent create_torch();

    /**
     * @brief Jet engine/rocket exhaust distortion
     * Directional, high speed, extreme temperature
     */
    static HeatDistortionComponent create_exhaust();

    /**
     * @brief Load from INI file
     * @param filepath Path to heat_distortion.ini
     */
    static HeatDistortionComponent load_from_ini(const std::string& filepath);

    /**
     * @brief Load quality preset
     */
    void apply_quality_preset(QualityPreset preset);

    /**
     * @brief Trigger explosion shockwave effect
     * @param explosion_radius Explosion radius (meters)
     * @param explosion_intensity Intensity multiplier (0.5 - 2.0)
     */
    void trigger_shockwave(float explosion_radius, float explosion_intensity = 1.0f);

    /**
     * @brief Update distortion from fire temperature
     * Called automatically by HeatDistortionSystem
     */
    void update_from_fire(float fire_temperature_k, const math::Vec3& fire_position);

    /**
     * @brief Calculate current distortion strength
     * Accounts for temperature, distance, and shockwave
     */
    float calculate_strength_at_position(const math::Vec3& world_pos, float time_s) const;
};

} // namespace lore::ecs