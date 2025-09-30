#include "lore/ecs/components/atmospheric_component.hpp"
#include <cmath>

namespace lore::ecs {

// ============================================================================
// Earth Presets
// ============================================================================

AtmosphericComponent AtmosphericComponent::create_earth_clear_day() {
    AtmosphericComponent atmos;
    atmos.preset_name = "earth_clear_day";

    // Standard Earth atmosphere
    atmos.planet_radius_m = 6371000.0f;
    atmos.atmosphere_thickness_m = 100000.0f;
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.3f;

    // Clear day sun (noon)
    atmos.sun_direction = {0.0f, 1.0f, 0.0f};  // Zenith
    atmos.sun_intensity_w_m2 = 1361.0f;
    atmos.sun_base_color_rgb = {1.0f, 1.0f, 1.0f};
    atmos.sun_angular_diameter_rad = 0.0093f;

    // Rayleigh scattering (blue sky)
    atmos.rayleigh_scattering_rgb = {5.8e-6f, 13.5e-6f, 33.1e-6f};
    atmos.rayleigh_scale_height_m = 8500.0f;
    atmos.rayleigh_density_multiplier = 1.0f;

    // Mie scattering (minimal haze)
    atmos.mie_scattering_coeff = 2.0e-6f;
    atmos.mie_extinction_coeff = 2.22e-6f;
    atmos.mie_scale_height_m = 1200.0f;
    atmos.mie_phase_g = 0.76f;
    atmos.pollution_density_kg_m3 = 0.0f;

    // Ozone
    atmos.ozone_absorption_rgb = {0.0f, 1.8e-6f, 4.0e-6f};
    atmos.ozone_peak_altitude_m = 25000.0f;
    atmos.ozone_layer_thickness_m = 15000.0f;
    atmos.ozone_concentration_multiplier = 1.0f;

    // Volumetric lighting
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.3f;
    atmos.volumetric_noise_scale = 0.2f;

    // Distance fog (minimal on clear day)
    atmos.enable_distance_fog = true;
    atmos.fog_density = 0.00001f;  // 100km visibility
    atmos.fog_start_distance_m = 0.0f;
    atmos.fog_height_falloff_m = 1000.0f;

    // No weather
    atmos.cloud_coverage = 0.0f;
    atmos.rain_intensity = 0.0f;
    atmos.snow_intensity = 0.0f;

    atmos.time_of_day_hours = 12.0f;
    atmos.day_of_year = 172;  // June 21 (summer solstice)

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_golden_hour() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "earth_golden_hour";

    // Sun at low angle (sunrise/sunset)
    atmos.sun_direction = math::Vec3{0.866f, 0.2f, 0.5f}.normalized();  // ~11° elevation

    // Slightly warmer base color (sun appears more orange at horizon)
    atmos.sun_base_color_rgb = {1.0f, 0.95f, 0.9f};

    // More dramatic volumetric lighting
    atmos.volumetric_intensity = 0.8f;
    atmos.volumetric_noise_scale = 0.4f;

    // Slightly more haze for golden glow
    atmos.mie_scattering_coeff = 3.5e-6f;
    atmos.mie_extinction_coeff = 3.89e-6f;

    atmos.time_of_day_hours = 6.0f;  // Sunrise

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_overcast() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "earth_overcast";

    // Heavy cloud coverage
    atmos.cloud_coverage = 0.9f;
    atmos.cloud_opacity = 0.85f;
    atmos.cloud_color_tint = {0.7f, 0.7f, 0.75f};  // Gray clouds

    // Reduced sun intensity (blocked by clouds)
    atmos.sun_intensity_w_m2 = 300.0f;

    // More diffuse lighting
    atmos.volumetric_intensity = 0.15f;

    // Slightly more humid atmosphere
    atmos.mie_scattering_coeff = 4.0e-6f;
    atmos.mie_extinction_coeff = 4.44e-6f;

    // Reduced visibility
    atmos.fog_density = 0.0001f;  // 10km visibility

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_polluted_city() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "earth_polluted_city";

    // Heavy pollution
    atmos.pollution_density_kg_m3 = 0.0001f;  // 100 µg/m³ (hazardous)
    atmos.pollution_color_tint = {0.7f, 0.6f, 0.5f};  // Brown smog

    // Much more Mie scattering (haze)
    atmos.mie_scattering_coeff = 5.0e-5f;  // 25× normal
    atmos.mie_extinction_coeff = 5.56e-5f;
    atmos.mie_scale_height_m = 800.0f;  // Pollution near ground

    // Heavily reduced visibility
    atmos.fog_density = 0.0005f;  // 2km visibility
    atmos.fog_height_falloff_m = 500.0f;  // Smog hugs ground

    // Volumetric lighting through smog
    atmos.volumetric_intensity = 0.6f;
    atmos.volumetric_noise_scale = 0.5f;

    // Some cloud coverage
    atmos.cloud_coverage = 0.3f;

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_foggy_morning() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "earth_foggy_morning";

    // Dense fog
    atmos.fog_density = 0.001f;  // 1km visibility
    atmos.fog_start_distance_m = 10.0f;  // Fog starts close
    atmos.fog_height_falloff_m = 200.0f;  // Ground fog

    // Very high Mie scattering
    atmos.mie_scattering_coeff = 1.0e-4f;
    atmos.mie_extinction_coeff = 1.11e-4f;
    atmos.mie_scale_height_m = 100.0f;  // Fog on ground

    // Cool morning sun
    atmos.sun_direction = math::Vec3{0.707f, 0.3f, 0.707f}.normalized();  // Low angle
    atmos.sun_base_color_rgb = {1.0f, 0.98f, 0.95f};  // Slightly cool

    // Reduced sun intensity through fog
    atmos.sun_intensity_w_m2 = 400.0f;

    // Subtle volumetric lighting
    atmos.volumetric_intensity = 0.4f;
    atmos.volumetric_noise_scale = 0.6f;

    atmos.time_of_day_hours = 7.0f;  // Morning

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_night() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "earth_night";

    // Sun below horizon
    atmos.sun_direction = {0.0f, -0.5f, 0.866f};  // 30° below horizon
    atmos.sun_intensity_w_m2 = 0.0f;  // No sun

    // Moon primary light source
    atmos.moon_direction = {0.0f, 0.707f, 0.707f};  // 45° elevation
    atmos.moon_intensity_w_m2 = 0.0032f;  // Full moon
    atmos.moon_base_color_rgb = {0.95f, 0.93f, 0.88f};

    // Reduced scattering (less visible at night)
    atmos.rayleigh_density_multiplier = 0.3f;
    atmos.mie_scattering_coeff = 1.0e-6f;

    // Clear night
    atmos.cloud_coverage = 0.0f;
    atmos.fog_density = 0.00001f;

    // No volumetric lighting
    atmos.enable_volumetric_lighting = false;

    atmos.time_of_day_hours = 0.0f;  // Midnight

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_earth_stormy() {
    auto atmos = create_earth_overcast();
    atmos.preset_name = "earth_stormy";

    // Dark storm clouds
    atmos.cloud_coverage = 1.0f;
    atmos.cloud_opacity = 0.95f;
    atmos.cloud_color_tint = {0.3f, 0.3f, 0.35f};  // Dark gray

    // Heavy rain
    atmos.rain_intensity = 0.8f;

    // Very reduced sun intensity
    atmos.sun_intensity_w_m2 = 150.0f;

    // Poor visibility
    atmos.fog_density = 0.0003f;  // 3.3km visibility
    atmos.mie_scattering_coeff = 8.0e-6f;

    // Dramatic volumetric lighting (lightning)
    atmos.volumetric_intensity = 0.5f;
    atmos.volumetric_noise_scale = 0.7f;

    return atmos;
}

// ============================================================================
// Alien Planet Presets
// ============================================================================

AtmosphericComponent AtmosphericComponent::create_mars() {
    AtmosphericComponent atmos;
    atmos.preset_name = "mars";

    // Mars properties
    atmos.planet_radius_m = 3390000.0f;  // Smaller than Earth
    atmos.atmosphere_thickness_m = 10700.0f;  // Very thin
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.25f;  // Reddish surface

    // Sun (slightly dimmer, Mars is farther from Sun)
    atmos.sun_direction = {0.0f, 0.8f, 0.6f};
    atmos.sun_intensity_w_m2 = 590.0f;  // 43% of Earth (Mars distance)
    atmos.sun_base_color_rgb = {1.0f, 1.0f, 1.0f};
    atmos.sun_angular_diameter_rad = 0.0062f;  // Smaller from Mars

    // Thin CO₂ atmosphere (less scattering)
    atmos.rayleigh_scattering_rgb = {2.5e-6f, 5.0e-6f, 10.0e-6f};  // Less than Earth
    atmos.rayleigh_scale_height_m = 11100.0f;
    atmos.rayleigh_density_multiplier = 0.01f;  // 1% of Earth density

    // Red dust in atmosphere
    atmos.mie_scattering_coeff = 8.0e-6f;
    atmos.mie_extinction_coeff = 8.89e-6f;
    atmos.mie_scale_height_m = 2000.0f;
    atmos.mie_phase_g = 0.8f;  // Forward scattering dust
    atmos.pollution_density_kg_m3 = 0.00003f;  // Dust
    atmos.pollution_color_tint = {1.0f, 0.5f, 0.3f};  // Red/orange dust

    // No ozone
    atmos.ozone_concentration_multiplier = 0.0f;

    // Minimal volumetric lighting (thin atmosphere)
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 32;
    atmos.volumetric_intensity = 0.15f;

    // Good visibility (thin atmosphere)
    atmos.fog_density = 0.00005f;  // 20km visibility
    atmos.fog_color_override_rgb = {0.8f, 0.5f, 0.3f};  // Reddish fog

    atmos.time_of_day_hours = 12.0f;

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_titan() {
    AtmosphericComponent atmos;
    atmos.preset_name = "titan";

    // Titan properties (Saturn's moon)
    atmos.planet_radius_m = 2575000.0f;
    atmos.atmosphere_thickness_m = 600000.0f;  // Very thick!
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.22f;

    // Distant sun (Saturn distance)
    atmos.sun_direction = {0.0f, 0.6f, 0.8f};
    atmos.sun_intensity_w_m2 = 15.0f;  // 1% of Earth (very distant)
    atmos.sun_base_color_rgb = {1.0f, 1.0f, 1.0f};
    atmos.sun_angular_diameter_rad = 0.00098f;  // Tiny sun

    // Thick nitrogen atmosphere
    atmos.rayleigh_scattering_rgb = {3.0e-6f, 6.0e-6f, 12.0e-6f};
    atmos.rayleigh_scale_height_m = 40000.0f;  // Thick atmosphere
    atmos.rayleigh_density_multiplier = 1.5f;  // Dense

    // Orange haze (organic compounds)
    atmos.mie_scattering_coeff = 5.0e-5f;  // Heavy haze
    atmos.mie_extinction_coeff = 5.56e-5f;
    atmos.mie_scale_height_m = 8000.0f;
    atmos.mie_phase_g = 0.85f;
    atmos.pollution_density_kg_m3 = 0.0002f;  // Organic haze
    atmos.pollution_color_tint = {1.0f, 0.7f, 0.4f};  // Orange

    // No ozone
    atmos.ozone_concentration_multiplier = 0.0f;

    // Volumetric lighting through haze
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.7f;
    atmos.volumetric_noise_scale = 0.6f;

    // Poor visibility (thick haze)
    atmos.fog_density = 0.0003f;  // 3.3km visibility
    atmos.fog_color_override_rgb = {0.9f, 0.6f, 0.3f};  // Orange fog

    // Overcast (permanent haze)
    atmos.cloud_coverage = 1.0f;
    atmos.cloud_opacity = 0.7f;
    atmos.cloud_color_tint = {0.9f, 0.7f, 0.5f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_venus() {
    AtmosphericComponent atmos;
    atmos.preset_name = "venus";

    // Venus properties
    atmos.planet_radius_m = 6052000.0f;  // Similar to Earth
    atmos.atmosphere_thickness_m = 250000.0f;  // Very thick
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.76f;  // Very bright (clouds)

    // Bright sun (closer than Earth)
    atmos.sun_direction = {0.0f, 0.5f, 0.866f};
    atmos.sun_intensity_w_m2 = 2613.0f;  // 192% of Earth
    atmos.sun_base_color_rgb = {1.0f, 1.0f, 1.0f};
    atmos.sun_angular_diameter_rad = 0.012f;  // Larger from Venus

    // Thick CO₂ atmosphere
    atmos.rayleigh_scattering_rgb = {4.0e-6f, 8.0e-6f, 16.0e-6f};
    atmos.rayleigh_scale_height_m = 15800.0f;
    atmos.rayleigh_density_multiplier = 90.0f;  // 90× Earth density!

    // Sulfuric acid clouds
    atmos.mie_scattering_coeff = 2.0e-4f;  // Extreme haze
    atmos.mie_extinction_coeff = 2.22e-4f;
    atmos.mie_scale_height_m = 5000.0f;
    atmos.mie_phase_g = 0.9f;
    atmos.pollution_density_kg_m3 = 0.001f;  // Dense clouds
    atmos.pollution_color_tint = {1.0f, 0.95f, 0.8f};  // Yellow-white

    // No ozone
    atmos.ozone_concentration_multiplier = 0.0f;

    // Volumetric lighting (but little reaches surface)
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.3f;

    // Extremely poor visibility
    atmos.fog_density = 0.002f;  // 500m visibility
    atmos.fog_color_override_rgb = {0.95f, 0.9f, 0.7f};  // Yellow

    // Total cloud cover
    atmos.cloud_coverage = 1.0f;
    atmos.cloud_opacity = 1.0f;
    atmos.cloud_color_tint = {0.95f, 0.9f, 0.7f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_ice_planet() {
    AtmosphericComponent atmos;
    atmos.preset_name = "ice_planet";

    // Cold planet properties
    atmos.planet_radius_m = 6000000.0f;
    atmos.atmosphere_thickness_m = 80000.0f;  // Thin (cold)
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.6f;  // High reflectivity (ice)

    // Distant blue-white star
    atmos.sun_direction = {0.0f, 0.866f, 0.5f};
    atmos.sun_intensity_w_m2 = 800.0f;  // Dim
    atmos.sun_base_color_rgb = {0.9f, 0.95f, 1.0f};  // Blue-white star
    atmos.sun_angular_diameter_rad = 0.008f;

    // Blue-tinted atmosphere
    atmos.rayleigh_scattering_rgb = {4.0e-6f, 10.0e-6f, 40.0e-6f};  // Very blue
    atmos.rayleigh_scale_height_m = 7000.0f;
    atmos.rayleigh_density_multiplier = 0.7f;

    // Ice crystals in atmosphere
    atmos.mie_scattering_coeff = 3.0e-6f;
    atmos.mie_extinction_coeff = 3.33e-6f;
    atmos.mie_scale_height_m = 1500.0f;
    atmos.mie_phase_g = 0.7f;
    atmos.pollution_density_kg_m3 = 0.00001f;  // Ice crystals
    atmos.pollution_color_tint = {0.9f, 0.95f, 1.0f};  // Blue-white

    // Ozone (creates deeper blue)
    atmos.ozone_concentration_multiplier = 1.5f;

    // Subtle volumetric lighting
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 48;
    atmos.volumetric_intensity = 0.25f;

    // Good visibility (cold air)
    atmos.fog_density = 0.000008f;  // 125km visibility
    atmos.fog_color_override_rgb = {0.85f, 0.9f, 1.0f};  // Blue tint

    // Light snow
    atmos.snow_intensity = 0.2f;

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_desert_planet() {
    auto atmos = create_mars();  // Similar to Mars but different
    atmos.preset_name = "desert_planet";

    // Standard planet size
    atmos.planet_radius_m = 6200000.0f;
    atmos.atmosphere_thickness_m = 90000.0f;
    atmos.planet_albedo = 0.35f;  // Sandy

    // Bright sun
    atmos.sun_intensity_w_m2 = 1500.0f;  // Harsh sun
    atmos.sun_base_color_rgb = {1.0f, 0.98f, 0.95f};  // Slightly yellow

    // Thin atmosphere
    atmos.rayleigh_density_multiplier = 0.6f;

    // Lots of dust
    atmos.mie_scattering_coeff = 1.5e-5f;
    atmos.pollution_density_kg_m3 = 0.00005f;  // Dust
    atmos.pollution_color_tint = {1.0f, 0.8f, 0.5f};  // Yellow-orange dust

    // Reduced visibility (dust storms)
    atmos.fog_density = 0.0001f;  // 10km visibility
    atmos.fog_color_override_rgb = {0.9f, 0.75f, 0.5f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_toxic_planet() {
    AtmosphericComponent atmos;
    atmos.preset_name = "toxic_planet";

    // Toxic world
    atmos.planet_radius_m = 6400000.0f;
    atmos.atmosphere_thickness_m = 120000.0f;
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.25f;

    // Greenish star or filtered sun
    atmos.sun_direction = {0.0f, 0.707f, 0.707f};
    atmos.sun_intensity_w_m2 = 1100.0f;
    atmos.sun_base_color_rgb = {0.9f, 1.0f, 0.9f};  // Slightly green
    atmos.sun_angular_diameter_rad = 0.01f;

    // Green-tinted scattering
    atmos.rayleigh_scattering_rgb = {8.0e-6f, 25.0e-6f, 12.0e-6f};  // Green peak
    atmos.rayleigh_scale_height_m = 9000.0f;
    atmos.rayleigh_density_multiplier = 1.2f;

    // Toxic gas haze
    atmos.mie_scattering_coeff = 4.0e-5f;
    atmos.mie_extinction_coeff = 4.44e-5f;
    atmos.mie_scale_height_m = 2000.0f;
    atmos.mie_phase_g = 0.8f;
    atmos.pollution_density_kg_m3 = 0.0003f;  // Heavy toxic gas
    atmos.pollution_color_tint = {0.6f, 1.0f, 0.6f};  // Green

    // No ozone (toxic atmosphere)
    atmos.ozone_concentration_multiplier = 0.0f;

    // Eerie volumetric lighting
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.6f;
    atmos.volumetric_noise_scale = 0.5f;

    // Poor visibility
    atmos.fog_density = 0.0004f;  // 2.5km visibility
    atmos.fog_color_override_rgb = {0.6f, 0.9f, 0.6f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_volcanic_planet() {
    AtmosphericComponent atmos;
    atmos.preset_name = "volcanic_planet";

    // Hot volcanic world
    atmos.planet_radius_m = 6100000.0f;
    atmos.atmosphere_thickness_m = 70000.0f;  // Thin (volcanic gases escape)
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.15f;  // Dark rock

    // Bright orange sun (or filtered through ash)
    atmos.sun_direction = {0.0f, 0.6f, 0.8f};
    atmos.sun_intensity_w_m2 = 1200.0f;
    atmos.sun_base_color_rgb = {1.0f, 0.7f, 0.5f};  // Orange-red
    atmos.sun_angular_diameter_rad = 0.011f;

    // Red-shifted scattering (volcanic gases)
    atmos.rayleigh_scattering_rgb = {15.0e-6f, 10.0e-6f, 6.0e-6f};  // More red
    atmos.rayleigh_scale_height_m = 6000.0f;
    atmos.rayleigh_density_multiplier = 0.8f;

    // Heavy ash and sulfur
    atmos.mie_scattering_coeff = 6.0e-5f;
    atmos.mie_extinction_coeff = 6.67e-5f;
    atmos.mie_scale_height_m = 1500.0f;
    atmos.mie_phase_g = 0.85f;
    atmos.pollution_density_kg_m3 = 0.0002f;  // Ash
    atmos.pollution_color_tint = {1.0f, 0.5f, 0.3f};  // Red-orange

    // No ozone
    atmos.ozone_concentration_multiplier = 0.0f;

    // Dramatic volumetric lighting through ash
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.8f;
    atmos.volumetric_noise_scale = 0.7f;

    // Poor visibility (ash)
    atmos.fog_density = 0.0003f;  // 3.3km visibility
    atmos.fog_color_override_rgb = {0.9f, 0.5f, 0.3f};  // Orange-red

    // Ash clouds
    atmos.cloud_coverage = 0.6f;
    atmos.cloud_opacity = 0.8f;
    atmos.cloud_color_tint = {0.6f, 0.4f, 0.3f};  // Dark ash

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_jungle_planet() {
    AtmosphericComponent atmos;
    atmos.preset_name = "jungle_planet";

    // Humid jungle world
    atmos.planet_radius_m = 6500000.0f;
    atmos.atmosphere_thickness_m = 110000.0f;
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.2f;  // Dark vegetation

    // Greenish star or filtered through plants
    atmos.sun_direction = {0.0f, 0.8f, 0.6f};
    atmos.sun_intensity_w_m2 = 1400.0f;
    atmos.sun_base_color_rgb = {0.98f, 1.0f, 0.98f};  // Slightly greenish
    atmos.sun_angular_diameter_rad = 0.0095f;

    // Green-tinted atmosphere (plant oxygen)
    atmos.rayleigh_scattering_rgb = {6.0e-6f, 16.0e-6f, 28.0e-6f};  // Slightly green-blue
    atmos.rayleigh_scale_height_m = 8800.0f;
    atmos.rayleigh_density_multiplier = 1.3f;  // Dense oxygen atmosphere

    // Humid haze (water vapor)
    atmos.mie_scattering_coeff = 8.0e-6f;
    atmos.mie_extinction_coeff = 8.89e-6f;
    atmos.mie_scale_height_m = 1000.0f;  // Humid near surface
    atmos.mie_phase_g = 0.75f;
    atmos.pollution_density_kg_m3 = 0.00002f;  // Pollen/spores
    atmos.pollution_color_tint = {0.85f, 1.0f, 0.85f};  // Greenish

    // High ozone (plant oxygen)
    atmos.ozone_concentration_multiplier = 1.5f;

    // Lush volumetric lighting
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.7f;
    atmos.volumetric_noise_scale = 0.6f;

    // Moderate visibility (humidity)
    atmos.fog_density = 0.00005f;  // 20km visibility
    atmos.fog_color_override_rgb = {0.8f, 0.95f, 0.85f};  // Green tint

    // Partial cloud cover
    atmos.cloud_coverage = 0.4f;
    atmos.cloud_opacity = 0.6f;
    atmos.cloud_color_tint = {0.95f, 1.0f, 0.95f};

    // Occasional rain (jungle)
    atmos.rain_intensity = 0.3f;

    return atmos;
}

// ============================================================================
// Fantasy/Sci-Fi Presets
// ============================================================================

AtmosphericComponent AtmosphericComponent::create_blood_moon() {
    auto atmos = create_earth_night();
    atmos.preset_name = "blood_moon";

    // Red moon
    atmos.moon_base_color_rgb = {1.0f, 0.2f, 0.1f};  // Deep red
    atmos.moon_intensity_w_m2 = 0.008f;  // Brighter than normal

    // Red-tinted atmosphere
    atmos.rayleigh_scattering_rgb = {20.0e-6f, 8.0e-6f, 5.0e-6f};  // Favor red
    atmos.rayleigh_density_multiplier = 0.5f;

    // Red haze
    atmos.mie_scattering_coeff = 5.0e-6f;
    atmos.pollution_density_kg_m3 = 0.00003f;
    atmos.pollution_color_tint = {1.0f, 0.3f, 0.2f};  // Red

    // Eerie red fog
    atmos.fog_density = 0.00008f;
    atmos.fog_color_override_rgb = {0.8f, 0.2f, 0.1f};

    // Volumetric lighting (red moon rays)
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 48;
    atmos.volumetric_intensity = 0.4f;

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_aurora_world() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "aurora_world";

    // Strong aurora
    atmos.enable_aurora = true;
    atmos.aurora_altitude_m = 100000.0f;
    atmos.aurora_intensity = 0.8f;
    atmos.aurora_color_rgb = {0.2f, 1.0f, 0.4f};  // Green aurora

    // Enhanced volumetric lighting for aurora visibility
    atmos.volumetric_intensity = 0.6f;
    atmos.volumetric_samples = 80;

    // Slight green tint to atmosphere
    atmos.rayleigh_scattering_rgb = {5.0e-6f, 15.0e-6f, 28.0e-6f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_purple_sky() {
    AtmosphericComponent atmos;
    atmos.preset_name = "purple_sky";

    // Standard planet
    atmos.planet_radius_m = 6300000.0f;
    atmos.atmosphere_thickness_m = 95000.0f;

    // Purple-white star
    atmos.sun_direction = {0.0f, 0.866f, 0.5f};
    atmos.sun_intensity_w_m2 = 1300.0f;
    atmos.sun_base_color_rgb = {1.0f, 0.9f, 1.0f};  // Slightly purple

    // Purple-shifted Rayleigh scattering
    atmos.rayleigh_scattering_rgb = {15.0e-6f, 10.0e-6f, 35.0e-6f};  // Peak in violet
    atmos.rayleigh_scale_height_m = 8200.0f;
    atmos.rayleigh_density_multiplier = 1.2f;

    // Purple haze
    atmos.mie_scattering_coeff = 3.0e-6f;
    atmos.pollution_density_kg_m3 = 0.00001f;
    atmos.pollution_color_tint = {0.8f, 0.6f, 1.0f};  // Purple

    // Ozone creates deeper purple
    atmos.ozone_concentration_multiplier = 2.0f;

    // Volumetric lighting
    atmos.enable_volumetric_lighting = true;
    atmos.volumetric_samples = 64;
    atmos.volumetric_intensity = 0.5f;

    // Purple-tinted fog
    atmos.fog_density = 0.00003f;
    atmos.fog_color_override_rgb = {0.7f, 0.6f, 0.9f};

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_dual_sun() {
    auto atmos = create_earth_clear_day();
    atmos.preset_name = "dual_sun";

    // Primary sun (yellow, main light source)
    atmos.celestial_bodies[0].type = CelestialBodyType::Sun;
    atmos.celestial_bodies[0].direction = math::Vec3{-0.3f, 0.8f, 0.5f}.normalized();
    atmos.celestial_bodies[0].intensity = 1000.0f;
    atmos.celestial_bodies[0].base_color_rgb = {1.0f, 1.0f, 0.95f};  // Yellow-white
    atmos.celestial_bodies[0].angular_diameter_rad = 0.0093f;
    atmos.celestial_bodies[0].casts_light = true;

    // Secondary sun (orange-red, dimmer companion star)
    atmos.celestial_bodies[1].type = CelestialBodyType::Sun;
    atmos.celestial_bodies[1].direction = math::Vec3{0.5f, 0.7f, -0.3f}.normalized();  // Different position
    atmos.celestial_bodies[1].intensity = 400.0f;  // 40% of primary
    atmos.celestial_bodies[1].base_color_rgb = {1.0f, 0.6f, 0.4f};  // Orange-red
    atmos.celestial_bodies[1].angular_diameter_rad = 0.007f;  // Smaller (farther or smaller star)
    atmos.celestial_bodies[1].casts_light = true;

    atmos.num_celestial_bodies = 2;
    atmos.primary_sun_index = 0;

    // Brighter overall atmosphere (two suns)
    atmos.rayleigh_density_multiplier = 1.3f;

    // More complex scattering (two light sources)
    atmos.volumetric_intensity = 0.7f;
    atmos.volumetric_samples = 80;

    return atmos;
}

AtmosphericComponent AtmosphericComponent::create_no_atmosphere() {
    AtmosphericComponent atmos;
    atmos.preset_name = "no_atmosphere";

    // Space station / moon
    atmos.planet_radius_m = 1737000.0f;  // Moon size
    atmos.atmosphere_thickness_m = 0.0f;  // No atmosphere
    atmos.surface_altitude_m = 0.0f;
    atmos.planet_albedo = 0.12f;  // Dark lunar surface

    // Bright sun (no attenuation)
    atmos.sun_direction = {0.0f, 0.707f, 0.707f};
    atmos.sun_intensity_w_m2 = 1361.0f;
    atmos.sun_base_color_rgb = {1.0f, 1.0f, 1.0f};

    // No scattering
    atmos.rayleigh_scattering_rgb = {0.0f, 0.0f, 0.0f};
    atmos.rayleigh_density_multiplier = 0.0f;
    atmos.mie_scattering_coeff = 0.0f;
    atmos.mie_extinction_coeff = 0.0f;
    atmos.ozone_concentration_multiplier = 0.0f;

    // No atmospheric effects
    atmos.enable_volumetric_lighting = false;
    atmos.enable_distance_fog = false;
    atmos.enable_aerial_perspective = false;

    // Black sky
    atmos.current_ambient_sky_rgb = {0.0f, 0.0f, 0.0f};
    atmos.current_zenith_color_rgb = {0.0f, 0.0f, 0.0f};

    return atmos;
}

// ============================================================================
// Calculation Methods
// ============================================================================

math::Vec3 AtmosphericComponent::calculate_sun_color(const math::Vec3& view_direction) const {
    // Calculate optical depth based on view angle and altitude
    float cos_view_zenith = view_direction.y;  // Assuming y is up
    float view_zenith_rad = std::acos(std::clamp(cos_view_zenith, -1.0f, 1.0f));

    float optical_depth = calculate_optical_depth(surface_altitude_m, view_zenith_rad);

    // Beer's law attenuation for each wavelength
    math::Vec3 rayleigh_ext = rayleigh_scattering_rgb * rayleigh_density_multiplier;
    float mie_ext = (mie_extinction_coeff + pollution_density_kg_m3 * 1000.0f);
    math::Vec3 ozone_ext = ozone_absorption_rgb * ozone_concentration_multiplier;

    math::Vec3 total_extinction = rayleigh_ext + math::Vec3{mie_ext, mie_ext, mie_ext} + ozone_ext;

    // I(λ) = I₀(λ) * exp(-τ(λ) * optical_depth)
    math::Vec3 sun_color;
    sun_color.x = sun_base_color_rgb.x * std::exp(-total_extinction.x * optical_depth);
    sun_color.y = sun_base_color_rgb.y * std::exp(-total_extinction.y * optical_depth);
    sun_color.z = sun_base_color_rgb.z * std::exp(-total_extinction.z * optical_depth);

    // Add pollution tint
    if (pollution_density_kg_m3 > 0.0f) {
        float pollution_factor = std::min(pollution_density_kg_m3 * 5000.0f, 1.0f);
        sun_color = sun_color * (1.0f - pollution_factor) +
                    (sun_color * pollution_color_tint) * pollution_factor;
    }

    return sun_color;
}

math::Vec3 AtmosphericComponent::calculate_sky_color(const math::Vec3& view_direction) const {
    // Simplified Rayleigh scattering for sky color
    // In reality this would be a complex integral, but approximate for performance

    float cos_view_zenith = view_direction.y;
    float view_elevation = std::asin(std::clamp(cos_view_zenith, -1.0f, 1.0f));

    // Sky is brighter near horizon (more scattering)
    float horizon_factor = 1.0f - std::abs(view_elevation) / (3.14159f / 2.0f);

    // Base sky color from Rayleigh scattering
    math::Vec3 sky_color = rayleigh_scattering_rgb * rayleigh_density_multiplier * 50000.0f;

    // Interpolate between zenith (darker blue) and horizon (lighter, more white)
    math::Vec3 zenith_color = sky_color;
    math::Vec3 horizon_color = sky_color * 0.5f + math::Vec3{0.5f, 0.5f, 0.5f};

    sky_color = zenith_color * (1.0f - horizon_factor) + horizon_color * horizon_factor;

    // Add Mie scattering (haze whitens sky)
    float mie_factor = (mie_scattering_coeff + pollution_density_kg_m3 * 1000.0f) * 100000.0f;
    sky_color += math::Vec3{mie_factor, mie_factor, mie_factor};

    // Normalize to reasonable range
    float max_component = std::max({sky_color.x, sky_color.y, sky_color.z});
    if (max_component > 1.0f) {
        sky_color /= max_component;
    }

    return sky_color;
}

float AtmosphericComponent::calculate_optical_depth(float start_altitude_m, float view_zenith_angle_rad) const {
    // Optical depth integral: τ = integral(ρ(h) * ds)
    // where ρ(h) = ρ₀ * exp(-h/H) is density profile
    // This is complex for curved atmosphere, so use approximation

    float cos_zenith = std::cos(view_zenith_angle_rad);

    // Chapman function approximation for spherical atmosphere
    float X = (planet_radius_m + start_altitude_m) / rayleigh_scale_height_m;
    float chapman_factor;

    if (cos_zenith >= 0.0f) {
        // Looking up or horizontal
        chapman_factor = 1.0f / (cos_zenith + 0.15f * std::pow(93.885f - view_zenith_angle_rad * 57.296f, -1.253f));
    } else {
        // Looking down (through less atmosphere)
        chapman_factor = 1.0f / (0.15f * std::pow(93.885f + view_zenith_angle_rad * 57.296f, -1.253f));
    }

    // Base optical depth at zenith (straight up)
    float optical_depth_zenith = rayleigh_scale_height_m * rayleigh_density_multiplier;

    return optical_depth_zenith * chapman_factor;
}

float AtmosphericComponent::calculate_visibility_distance() const {
    // Visibility distance where transmittance drops to 5% (Koschmieder equation)
    // L_v = -ln(0.05) / β_ext
    // where β_ext is extinction coefficient

    float total_extinction =
        (rayleigh_scattering_rgb.x + rayleigh_scattering_rgb.y + rayleigh_scattering_rgb.z) / 3.0f *
        rayleigh_density_multiplier +
        mie_extinction_coeff +
        pollution_density_kg_m3 * 1000.0f;

    if (fog_density > total_extinction) {
        total_extinction = fog_density;
    }

    if (total_extinction <= 0.0f) {
        return 1000000.0f;  // Essentially infinite
    }

    // Koschmieder equation: L_v = 3.912 / β_ext
    return 3.912f / total_extinction;
}

void AtmosphericComponent::update_sun_position_from_time() {
    if (!auto_update_sun_position) {
        return;
    }

    // Solar position calculation (simplified)
    // Full astronomical calculation would be much more complex

    // Hour angle: 0 = noon, 6 = sunrise, 18 = sunset
    float hour_angle = (time_of_day_hours - 12.0f) * 15.0f * (3.14159f / 180.0f);  // degrees to radians

    // Solar declination (angle between sun and equator)
    // Varies with day of year: δ = 23.45° * sin(360° * (day + 284) / 365)
    float declination_deg = 23.45f * std::sin(2.0f * 3.14159f * (day_of_year + 284) / 365.0f);
    float declination_rad = declination_deg * (3.14159f / 180.0f);

    float latitude_rad = latitude_degrees * (3.14159f / 180.0f);

    // Solar elevation angle
    float sin_elevation = std::sin(latitude_rad) * std::sin(declination_rad) +
                          std::cos(latitude_rad) * std::cos(declination_rad) * std::cos(hour_angle);
    float elevation = std::asin(std::clamp(sin_elevation, -1.0f, 1.0f));

    // Solar azimuth angle
    float cos_azimuth = (std::sin(declination_rad) - std::sin(latitude_rad) * sin_elevation) /
                        (std::cos(latitude_rad) * std::cos(elevation));
    float azimuth = std::acos(std::clamp(cos_azimuth, -1.0f, 1.0f));

    // Afternoon: azimuth > 180°
    if (hour_angle > 0.0f) {
        azimuth = 2.0f * 3.14159f - azimuth;
    }

    // Convert to direction vector (y is up)
    sun_direction.x = std::cos(elevation) * std::sin(azimuth);
    sun_direction.y = std::sin(elevation);
    sun_direction.z = std::cos(elevation) * std::cos(azimuth);
    sun_direction = sun_direction.normalized();

    needs_lut_update = true;
}

float AtmosphericComponent::get_sun_elevation() const {
    return std::asin(std::clamp(sun_direction.y, -1.0f, 1.0f));
}

float AtmosphericComponent::get_sun_azimuth() const {
    return std::atan2(sun_direction.x, sun_direction.z);
}

} // namespace lore::ecs