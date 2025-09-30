#pragma once

#include "lore/core/math_types.hpp"
#include <array>
#include <string>
#include <vector>

namespace lore::ecs {

/**
 * @brief Atmospheric scattering and lighting component
 *
 * Simulates realistic atmospheric scattering (Rayleigh + Mie) to produce
 * beautiful colored sunlight, pollution effects, and volumetric lighting.
 *
 * Physics:
 * - Rayleigh scattering: β_R(λ) = (8π³(n²-1)²)/(3N λ⁴)
 *   Wavelength-dependent scattering (blue sky, red sunset)
 *
 * - Mie scattering: β_M = pollution_density * scattering_coefficient
 *   Less wavelength-dependent (white haze, fog, pollution)
 *
 * - Ozone absorption: Absorbs UV, affects sky color
 *
 * - Sun color: Based on optical depth through atmosphere
 *   τ(λ) = integral(β_R(λ) + β_M + β_O(λ)) along view path
 *   I(λ) = I₀(λ) * exp(-τ(λ))
 *
 * Features:
 * - Dynamic sun/moon colors based on angle and pollution
 * - Volumetric god rays through atmospheric particles
 * - Distance fog with scattering
 * - Weather integration (rain, snow affects visibility)
 * - Alien planet atmospheres (Mars, Titan, ice worlds)
 * - 60+ INI configuration parameters
 *
 * Usage:
 * @code
 * // Earth clear day
 * auto atmosphere = AtmosphericComponent::create_earth_clear_day();
 *
 * // Polluted city
 * auto polluted = AtmosphericComponent::create_earth_polluted_city();
 * polluted.pollution_density_kg_m3 = 0.0001f; // Heavy smog
 *
 * // Mars
 * auto mars = AtmosphericComponent::create_mars();
 *
 * // Magic purple atmosphere
 * auto magic = AtmosphericComponent::create_custom();
 * magic.rayleigh_scattering_rgb = {0.5f, 0.0f, 1.0f}; // Purple tint
 * @endcode
 *
 * @note All units are SI unless specified
 * @note GPU compute shaders handle expensive scattering calculations
 */
struct AtmosphericComponent {
    // ========================================================================
    // CELESTIAL BODIES (Suns, Moons, Planets, Rings)
    // ========================================================================

    /**
     * @brief Types of celestial bodies
     */
    enum class CelestialBodyType {
        Sun,       // Primary light source (star)
        Moon,      // Reflected light, orbits planet
        Planet,    // Visible planet/gas giant in sky
        Ring,      // Planetary ring system
        Nebula,    // Background nebula/galaxy
    };

    /**
     * @brief Celestial body (sun, moon, planet, etc.)
     */
    struct CelestialBody {
        CelestialBodyType type = CelestialBodyType::Sun;

        /**
         * @brief Direction in world space (normalized)
         */
        math::Vec3 direction{0.0f, 0.707f, 0.707f};

        /**
         * @brief Intensity (W/m² for suns/moons, albedo 0-1 for planets)
         * Sun: 1361 W/m²
         * Full moon: 0.0032 W/m²
         * Planet: 0.1-0.6 (reflectivity)
         */
        float intensity = 1361.0f;

        /**
         * @brief Base color (RGB, linear)
         * Sun: (1.0, 1.0, 1.0) for white star
         * Moon: (0.95, 0.93, 0.88) for Earth's moon
         * Mars: (1.0, 0.5, 0.3) for red planet
         */
        math::Vec3 base_color_rgb{1.0f, 1.0f, 1.0f};

        /**
         * @brief Angular diameter (radians)
         * Sun: 0.0093 rad (0.53°)
         * Moon: 0.0089 rad (0.51°)
         * Larger = bigger disk, softer shadows
         */
        float angular_diameter_rad = 0.0093f;

        /**
         * @brief For moons/planets: phase (0-1)
         * 0.0 = new (dark)
         * 0.5 = half
         * 1.0 = full
         */
        float phase = 1.0f;

        /**
         * @brief For rings: inner radius (planet radii)
         */
        float ring_inner_radius = 1.5f;

        /**
         * @brief For rings: outer radius (planet radii)
         */
        float ring_outer_radius = 2.5f;

        /**
         * @brief For rings: tilt angle (radians)
         */
        float ring_tilt_rad = 0.1f;

        /**
         * @brief Whether this body casts light (suns and bright moons)
         */
        bool casts_light = true;

        /**
         * @brief Whether this body is visible in sky
         */
        bool visible = true;

        /**
         * @brief Current calculated color after atmospheric scattering
         * Updated by AtmosphericSystem
         */
        math::Vec3 current_color_rgb{1.0f, 1.0f, 1.0f};
    };

    /**
     * @brief Array of celestial bodies (up to 8)
     * Index 0 is primary sun by convention
     */
    std::array<CelestialBody, 8> celestial_bodies;

    /**
     * @brief Number of active celestial bodies (0-8)
     */
    uint32_t num_celestial_bodies = 1;

    /**
     * @brief Index of primary sun (default 0)
     * This sun is used for primary lighting calculations
     */
    uint32_t primary_sun_index = 0;

    // Legacy accessors for backward compatibility (map to celestial_bodies[0])
    math::Vec3& sun_direction = celestial_bodies[0].direction;
    float& sun_intensity_w_m2 = celestial_bodies[0].intensity;
    math::Vec3& sun_base_color_rgb = celestial_bodies[0].base_color_rgb;
    float& sun_angular_diameter_rad = celestial_bodies[0].angular_diameter_rad;

    /**
     * @brief Add a celestial body
     * @return Index of added body, or -1 if array full
     */
    int32_t add_celestial_body(const CelestialBody& body) {
        if (num_celestial_bodies >= 8) {
            return -1;
        }
        celestial_bodies[num_celestial_bodies] = body;
        return num_celestial_bodies++;
    }

    /**
     * @brief Remove celestial body at index
     */
    void remove_celestial_body(uint32_t index) {
        if (index >= num_celestial_bodies) return;
        for (uint32_t i = index; i < num_celestial_bodies - 1; ++i) {
            celestial_bodies[i] = celestial_bodies[i + 1];
        }
        num_celestial_bodies--;
    }

    /**
     * @brief Get primary sun (convenience)
     */
    const CelestialBody& get_primary_sun() const {
        return celestial_bodies[primary_sun_index];
    }

    /**
     * @brief Get mutable primary sun
     */
    CelestialBody& get_primary_sun_mut() {
        return celestial_bodies[primary_sun_index];
    }

    // ========================================================================
    // PLANET PROPERTIES
    // ========================================================================

    /**
     * @brief Planet radius (m)
     * Earth: 6,371,000 m
     * Mars: 3,390,000 m
     * Affects atmosphere curvature and optical depth
     */
    float planet_radius_m = 6371000.0f;

    /**
     * @brief Atmosphere thickness (m)
     * Earth: 100,000 m (karman line)
     * Mars: 10,700 m (thin atmosphere)
     * Titan: 600,000 m (very thick)
     */
    float atmosphere_thickness_m = 100000.0f;

    /**
     * @brief Surface altitude (m above planet surface)
     * 0 = sea level
     * 8848 = Mt. Everest
     */
    float surface_altitude_m = 0.0f;

    /**
     * @brief Planet albedo (surface reflectivity, 0-1)
     * Earth average: 0.3
     * Mars: 0.25 (reddish)
     * Ice planet: 0.6 (high reflectivity)
     */
    float planet_albedo = 0.3f;

    // ========================================================================
    // RAYLEIGH SCATTERING (Blue sky)
    // ========================================================================

    /**
     * @brief Rayleigh scattering coefficients for RGB (1/m)
     *
     * Physics: β_R(λ) = (8π³(n²-1)²)/(3N λ⁴)
     * where n = refractive index, N = number density, λ = wavelength
     *
     * Earth sea level (λ = 680nm, 550nm, 440nm):
     * Red:   5.8e-6 (less scattering)
     * Green: 13.5e-6
     * Blue:  33.1e-6 (most scattering, why sky is blue)
     *
     * Higher values = more scattering = bluer sky
     */
    math::Vec3 rayleigh_scattering_rgb{5.8e-6f, 13.5e-6f, 33.1e-6f};

    /**
     * @brief Rayleigh scale height (m)
     * Exponential falloff: density(h) = density(0) * exp(-h / H)
     * Earth: 8500 m
     * Mars: 11,100 m (thinner atmosphere)
     */
    float rayleigh_scale_height_m = 8500.0f;

    /**
     * @brief Rayleigh density multiplier
     * 1.0 = Earth standard
     * 0.5 = half density (thinner air, less blue)
     * 2.0 = double density (very blue sky)
     */
    float rayleigh_density_multiplier = 1.0f;

    // ========================================================================
    // MIE SCATTERING (Haze, pollution, fog)
    // ========================================================================

    /**
     * @brief Mie scattering coefficient (1/m)
     *
     * Earth clear day: 2.0e-6
     * Light haze: 1.0e-5
     * Heavy pollution: 5.0e-5
     * Fog: 1.0e-4
     *
     * Less wavelength-dependent than Rayleigh (white haze)
     */
    float mie_scattering_coeff = 2.0e-6f;

    /**
     * @brief Mie extinction coefficient (1/m)
     * Extinction = scattering + absorption
     * Typically mie_extinction = mie_scattering / 0.9
     */
    float mie_extinction_coeff = 2.22e-6f;

    /**
     * @brief Mie scale height (m)
     * Pollution/haze concentrated near surface
     * Earth: 1200 m (much lower than Rayleigh)
     */
    float mie_scale_height_m = 1200.0f;

    /**
     * @brief Mie phase function asymmetry factor g
     *
     * g = 0: Isotropic scattering (equal in all directions)
     * g = 0.76: Earth aerosols (forward scattering)
     * g = 0.9: Strong forward scattering (bright sun glow)
     * g = -0.5: Backscattering
     *
     * Henyey-Greenstein phase function:
     * P(θ) = (1 - g²) / (4π (1 + g² - 2g cos θ)^(3/2))
     */
    float mie_phase_g = 0.76f;

    /**
     * @brief Pollution density (kg/m³)
     *
     * Clean air: 0.0 (only natural aerosols)
     * Light smog: 0.00001 (10 µg/m³)
     * Moderate pollution: 0.00005 (50 µg/m³)
     * Heavy smog: 0.0001 (100 µg/m³)
     * Extreme: 0.0003 (300 µg/m³, hazardous)
     *
     * Affects visibility and sun color (more haze = whiter sun)
     */
    float pollution_density_kg_m3 = 0.0f;

    /**
     * @brief Pollution color tint
     * RGB multiplier for pollution haze
     *
     * Brown smog: (0.7, 0.6, 0.5)
     * Yellow industrial: (1.0, 0.9, 0.6)
     * Green toxic gas: (0.6, 1.0, 0.6)
     * Purple alien: (0.8, 0.6, 1.0)
     */
    math::Vec3 pollution_color_tint{0.7f, 0.6f, 0.5f};

    // ========================================================================
    // OZONE ABSORPTION
    // ========================================================================

    /**
     * @brief Ozone absorption coefficients for RGB (1/m)
     *
     * Ozone (O₃) absorbs UV and blue light, affects sky color
     * Peak absorption at ~600nm (orange)
     *
     * Earth standard (at ozone layer altitude ~25km):
     * Red:   0.0 (no absorption)
     * Green: 1.8e-6
     * Blue:  4.0e-6
     */
    math::Vec3 ozone_absorption_rgb{0.0f, 1.8e-6f, 4.0e-6f};

    /**
     * @brief Ozone layer peak altitude (m)
     * Earth: 25,000 m
     */
    float ozone_peak_altitude_m = 25000.0f;

    /**
     * @brief Ozone layer thickness (m)
     * Earth: 15,000 m (approximately)
     */
    float ozone_layer_thickness_m = 15000.0f;

    /**
     * @brief Ozone concentration multiplier
     * 1.0 = Earth standard
     * 0.0 = No ozone (harsher UV, different sky color)
     * 2.0 = Double ozone (deeper blue)
     */
    float ozone_concentration_multiplier = 1.0f;

    // ========================================================================
    // VOLUMETRIC LIGHTING (God rays)
    // ========================================================================

    /**
     * @brief Enable volumetric lighting calculations
     * God rays through atmospheric particles
     * Performance: ~1-2ms for full screen at 1080p
     */
    bool enable_volumetric_lighting = true;

    /**
     * @brief Number of raymarching samples for god rays
     * 32 = Low quality, fast
     * 64 = Medium quality (recommended)
     * 128 = High quality, slower
     */
    uint32_t volumetric_samples = 64;

    /**
     * @brief Volumetric scattering intensity
     * How bright are god rays
     * 0.1 = Subtle
     * 0.5 = Noticeable
     * 1.0 = Strong
     */
    float volumetric_intensity = 0.5f;

    /**
     * @brief Volumetric noise scale
     * Adds detail to god rays (clouds, dust variation)
     * 0.0 = Smooth god rays
     * 1.0 = Detailed atmospheric variation
     */
    float volumetric_noise_scale = 0.3f;

    // ========================================================================
    // DISTANCE FOG
    // ========================================================================

    /**
     * @brief Enable distance fog
     * Atmospheric scattering reduces visibility with distance
     */
    bool enable_distance_fog = true;

    /**
     * @brief Fog density (1/m)
     * Exponential fog: visibility = exp(-density * distance)
     *
     * Clear day: 0.00001 (100km visibility)
     * Light haze: 0.0001 (10km visibility)
     * Fog: 0.001 (1km visibility)
     * Dense fog: 0.01 (100m visibility)
     */
    float fog_density = 0.00001f;

    /**
     * @brief Fog color (RGB, linear)
     * Usually calculated from atmospheric scattering, but can override
     * Empty (0,0,0) = use calculated atmospheric color
     * Non-zero = override fog color
     */
    math::Vec3 fog_color_override_rgb{0.0f, 0.0f, 0.0f};

    /**
     * @brief Fog start distance (m)
     * 0 = fog starts immediately
     * 1000 = fog starts at 1km
     */
    float fog_start_distance_m = 0.0f;

    /**
     * @brief Height fog density multiplier
     * Fog thickens closer to ground
     * exp(-height / height_falloff)
     */
    float fog_height_falloff_m = 1000.0f;

    // ========================================================================
    // WEATHER EFFECTS
    // ========================================================================

    /**
     * @brief Cloud coverage (0-1)
     * 0.0 = Clear sky
     * 0.3 = Partly cloudy
     * 0.7 = Mostly cloudy
     * 1.0 = Overcast
     */
    float cloud_coverage = 0.0f;

    /**
     * @brief Cloud opacity (0-1)
     * How much clouds block sunlight
     * 0.5 = Thin clouds
     * 0.9 = Thick clouds
     */
    float cloud_opacity = 0.7f;

    /**
     * @brief Cloud color tint
     * White clouds: (1.0, 1.0, 1.0)
     * Storm clouds: (0.3, 0.3, 0.35)
     * Sunset clouds: (1.0, 0.5, 0.3)
     */
    math::Vec3 cloud_color_tint{1.0f, 1.0f, 1.0f};

    /**
     * @brief Rain intensity (0-1)
     * Affects visibility and atmospheric scattering
     * 0.0 = No rain
     * 0.5 = Moderate rain
     * 1.0 = Heavy downpour
     */
    float rain_intensity = 0.0f;

    /**
     * @brief Snow intensity (0-1)
     * Similar to rain but affects color differently
     */
    float snow_intensity = 0.0f;

    // ========================================================================
    // ADVANCED FEATURES
    // ========================================================================

    /**
     * @brief Enable multi-scattering approximation
     * More realistic but ~20% slower
     * Second-order scattering for accurate sky color
     */
    bool enable_multi_scattering = false;

    /**
     * @brief Enable aerial perspective
     * Objects in distance take on atmospheric color
     */
    bool enable_aerial_perspective = true;

    /**
     * @brief Aerial perspective distance scale (m)
     * How quickly distant objects fade to sky color
     * 10000 = 10km for full transition
     */
    float aerial_perspective_distance_m = 10000.0f;

    /**
     * @brief Enable aurora/northern lights simulation
     * Requires additional particle system
     */
    bool enable_aurora = false;

    /**
     * @brief Aurora altitude (m)
     * Earth: 100,000-300,000 m
     */
    float aurora_altitude_m = 100000.0f;

    /**
     * @brief Aurora color (RGB)
     * Green (oxygen): (0.0, 1.0, 0.2)
     * Red (oxygen high altitude): (1.0, 0.0, 0.0)
     * Blue (nitrogen): (0.2, 0.3, 1.0)
     */
    math::Vec3 aurora_color_rgb{0.0f, 1.0f, 0.2f};

    /**
     * @brief Aurora intensity (0-1)
     */
    float aurora_intensity = 0.0f;

    // ========================================================================
    // TEMPORAL EFFECTS
    // ========================================================================

    /**
     * @brief Time of day (0-24 hours)
     * Used to automatically update sun position if auto_update_sun enabled
     */
    float time_of_day_hours = 12.0f;

    /**
     * @brief Day of year (1-365)
     * Affects sun angle (seasons)
     */
    uint32_t day_of_year = 180;  // Summer

    /**
     * @brief Latitude (degrees, -90 to +90)
     * Affects sun path and angle
     * 0 = Equator
     * 51.5 = London
     * -33.9 = Sydney
     */
    float latitude_degrees = 0.0f;

    /**
     * @brief Auto-update sun position based on time/date/location
     */
    bool auto_update_sun_position = false;

    // ========================================================================
    // GPU RESOURCES
    // ========================================================================

    /**
     * @brief GPU texture for precomputed atmospheric scattering LUT
     * 4D LUT: (view_zenith, sun_zenith, altitude, distance)
     * Precomputed for real-time lookups
     */
    uint32_t scattering_lut_texture = 0;

    /**
     * @brief GPU texture for transmittance LUT
     * 2D LUT: (view_zenith, altitude)
     */
    uint32_t transmittance_lut_texture = 0;

    /**
     * @brief Dirty flag to trigger LUT regeneration
     */
    bool needs_lut_update = true;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Configuration preset name
     */
    std::string preset_name = "custom";

    /**
     * @brief LOD level (0 = highest, 2 = lowest)
     * Affects LUT resolution and raymarch samples
     */
    uint32_t lod_level = 1;

    // ========================================================================
    // CALCULATED VALUES (Updated by AtmosphericSystem)
    // ========================================================================

    /**
     * @brief Current sun color after atmospheric scattering (RGB, linear)
     * Calculated based on sun angle, altitude, pollution
     */
    math::Vec3 current_sun_color_rgb{1.0f, 1.0f, 1.0f};

    /**
     * @brief Current moon color after atmospheric scattering
     */
    math::Vec3 current_moon_color_rgb{0.95f, 0.93f, 0.88f};

    /**
     * @brief Current ambient sky color (RGB, linear)
     */
    math::Vec3 current_ambient_sky_rgb{0.3f, 0.4f, 0.6f};

    /**
     * @brief Current zenith color (top of sky)
     */
    math::Vec3 current_zenith_color_rgb{0.2f, 0.3f, 0.8f};

    /**
     * @brief Current horizon color
     */
    math::Vec3 current_horizon_color_rgb{0.6f, 0.7f, 0.9f};

    /**
     * @brief Optical depth at current sun angle
     * How much atmosphere light travels through
     */
    float current_optical_depth = 1.0f;

    /**
     * @brief Visibility distance (m)
     * Based on fog density and pollution
     */
    float current_visibility_distance_m = 100000.0f;

    // ========================================================================
    // STATIC FACTORY METHODS - Earth Presets
    // ========================================================================

    /**
     * @brief Earth clear day (noon, no pollution)
     */
    static AtmosphericComponent create_earth_clear_day();

    /**
     * @brief Earth sunrise/sunset (golden hour)
     */
    static AtmosphericComponent create_earth_golden_hour();

    /**
     * @brief Earth overcast (cloudy)
     */
    static AtmosphericComponent create_earth_overcast();

    /**
     * @brief Earth polluted city (heavy smog)
     */
    static AtmosphericComponent create_earth_polluted_city();

    /**
     * @brief Earth foggy morning
     */
    static AtmosphericComponent create_earth_foggy_morning();

    /**
     * @brief Earth night (moon lighting)
     */
    static AtmosphericComponent create_earth_night();

    /**
     * @brief Earth stormy weather
     */
    static AtmosphericComponent create_earth_stormy();

    // ========================================================================
    // STATIC FACTORY METHODS - Alien Planet Presets
    // ========================================================================

    /**
     * @brief Mars (thin CO₂ atmosphere, red tint)
     */
    static AtmosphericComponent create_mars();

    /**
     * @brief Titan (thick nitrogen atmosphere, orange haze)
     */
    static AtmosphericComponent create_titan();

    /**
     * @brief Venus (extremely thick CO₂, yellow-white)
     */
    static AtmosphericComponent create_venus();

    /**
     * @brief Ice planet (thin atmosphere, blue tint)
     */
    static AtmosphericComponent create_ice_planet();

    /**
     * @brief Desert planet (dusty, yellow-orange)
     */
    static AtmosphericComponent create_desert_planet();

    /**
     * @brief Toxic planet (green haze, alien atmosphere)
     */
    static AtmosphericComponent create_toxic_planet();

    /**
     * @brief Volcanic planet (red/orange, ash-filled)
     */
    static AtmosphericComponent create_volcanic_planet();

    /**
     * @brief Alien jungle world (thick humid atmosphere, greenish)
     */
    static AtmosphericComponent create_jungle_planet();

    // ========================================================================
    // STATIC FACTORY METHODS - Fantasy/Sci-Fi Presets
    // ========================================================================

    /**
     * @brief Blood moon atmosphere (red tint)
     */
    static AtmosphericComponent create_blood_moon();

    /**
     * @brief Aurora planet (strong northern lights)
     */
    static AtmosphericComponent create_aurora_world();

    /**
     * @brief Purple alien sky
     */
    static AtmosphericComponent create_purple_sky();

    /**
     * @brief Dual sun system (binary star)
     */
    static AtmosphericComponent create_dual_sun();

    /**
     * @brief Void/space station (no atmosphere)
     */
    static AtmosphericComponent create_no_atmosphere();

    // ========================================================================
    // METHODS
    // ========================================================================

    /**
     * @brief Calculate sun color at current angle and altitude
     * @param view_direction Direction from observer to sun (normalized)
     * @return Sun color after atmospheric scattering (RGB, linear)
     */
    math::Vec3 calculate_sun_color(const math::Vec3& view_direction) const;

    /**
     * @brief Calculate sky color at given direction
     * @param view_direction Direction to sample (normalized)
     * @return Sky color (RGB, linear)
     */
    math::Vec3 calculate_sky_color(const math::Vec3& view_direction) const;

    /**
     * @brief Calculate optical depth (atmosphere thickness) along ray
     * @param start_altitude_m Starting altitude above surface
     * @param view_zenith_angle_rad Angle from zenith (0 = straight up, π/2 = horizon)
     * @return Optical depth (dimensionless)
     */
    float calculate_optical_depth(float start_altitude_m, float view_zenith_angle_rad) const;

    /**
     * @brief Calculate visibility distance based on fog and pollution
     * @return Distance (m) at which visibility drops to 5% (human perception threshold)
     */
    float calculate_visibility_distance() const;

    /**
     * @brief Update sun position based on time, date, and latitude
     * Astronomical calculation for realistic day/night cycle
     */
    void update_sun_position_from_time();

    /**
     * @brief Get sun elevation angle (radians above horizon)
     * 0 = horizon, π/2 = zenith
     */
    float get_sun_elevation() const;

    /**
     * @brief Get sun azimuth angle (radians, 0 = north, π/2 = east)
     */
    float get_sun_azimuth() const;
};

} // namespace lore::ecs