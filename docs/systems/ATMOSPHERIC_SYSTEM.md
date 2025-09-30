# Atmospheric Scattering System

## Overview

The Atmospheric System simulates realistic atmospheric scattering to create beautiful colored sunlight, sky colors, pollution effects, and volumetric god rays. It supports Earth-like atmospheres, alien planets, multiple suns, and fantasy settings.

**Key Features:**
- **Physically-Based Scattering**: Rayleigh (blue sky), Mie (haze/pollution), Ozone absorption
- **Dynamic Sun Colors**: Sun color changes with angle, altitude, and pollution
- **Multiple Celestial Bodies**: Up to 8 suns, moons, planets, rings simultaneously
- **20+ Presets**: Earth (clear day, sunset, polluted city), Alien planets (Mars, Titan, Venus), Fantasy (blood moon, aurora, dual-sun)
- **Volumetric God Rays**: Beautiful light shafts through atmospheric particles
- **Weather Integration**: Clouds, rain, snow affect visibility and atmospheric color
- **60+ Configuration Parameters**: Extensive INI customization
- **GPU-Accelerated**: Precomputed LUTs, real-time compute shaders

**Physics:**
```
Rayleigh Scattering: β_R(λ) = (8π³(n²-1)²)/(3N λ⁴)
Mie Scattering: β_M = pollution_density * scattering_coefficient
Sun Color: I(λ) = I₀(λ) * exp(-τ(λ))
where τ(λ) = integral(β_R(λ) + β_M + β_O(λ)) along ray
```

## Quick Start

```cpp
#include "lore/ecs/components/atmospheric_component.hpp"

// Create atmosphere entity
auto atmosphere_entity = world.create_entity();

// Earth clear day
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_earth_clear_day()
);

// System updates automatically
atmospheric_system.update(world, delta_time);

// Query current sun color
math::Vec3 sun_color = atmos.current_sun_color_rgb;
```

## Available Presets

### Earth Presets

| Preset | Description | Sun Intensity | Visibility | Use Case |
|--------|-------------|---------------|------------|----------|
| `create_earth_clear_day()` | Perfect clear day, noon | 1361 W/m² | 100km | Default Earth atmosphere |
| `create_earth_golden_hour()` | Sunrise/sunset, warm tones | 1361 W/m² | 100km | Beautiful lighting, photography |
| `create_earth_overcast()` | Heavy clouds, diffuse light | 300 W/m² | 10km | Gloomy day, soft shadows |
| `create_earth_polluted_city()` | Heavy smog, brown haze | 1361 W/m² | 2km | Cyberpunk, industrial cities |
| `create_earth_foggy_morning()` | Ground fog, limited visibility | 400 W/m² | 1km | Spooky, mysterious atmosphere |
| `create_earth_night()` | Moonlight, starry sky | 0 W/m² (moon: 0.0032) | 100km | Night scenes |
| `create_earth_stormy()` | Dark clouds, rain, poor visibility | 150 W/m² | 3.3km | Storms, dramatic weather |

### Alien Planet Presets

| Preset | Description | Atmosphere | Visibility | Unique Features |
|--------|-------------|------------|------------|-----------------|
| `create_mars()` | Thin CO₂, red dust | 1% Earth density | 20km | Red/orange sky, dust storms |
| `create_titan()` | Thick nitrogen, orange haze | 150% Earth density | 3.3km | Permanent orange haze, dim sun |
| `create_venus()` | Extreme CO₂, sulfuric acid clouds | 9000% Earth density | 500m | Yellow-white, crushing atmosphere |
| `create_ice_planet()` | Thin cold atmosphere, ice crystals | 70% Earth density | 125km | Blue-tinted, crystal sparkles |
| `create_desert_planet()` | Dusty, harsh sunlight | 60% Earth density | 10km | Yellow-orange dust, Dune-like |
| `create_toxic_planet()` | Green poisonous gas | 120% Earth density | 2.5km | Eerie green haze, toxic |
| `create_volcanic_planet()` | Ash-filled, sulfur | 80% Earth density | 3.3km | Red-orange sky, ash clouds |
| `create_jungle_planet()` | Humid oxygen-rich | 130% Earth density | 20km | Greenish tint, lush atmosphere |

### Fantasy/Sci-Fi Presets

| Preset | Description | Special Features |
|--------|-------------|------------------|
| `create_blood_moon()` | Red moon, eerie atmosphere | Red-tinted sky and fog |
| `create_aurora_world()` | Strong northern lights | Green aurora, enhanced volumetrics |
| `create_purple_sky()` | Purple atmosphere | Alien purple-shifted scattering |
| `create_dual_sun()` | **Binary star system** | Two suns, complex shadows |
| `create_no_atmosphere()` | Space/moon | Black sky, stars visible, no scattering |

## Multi-Sun / Multi-Celestial Body System

### Adding Multiple Suns

```cpp
// Create dual-sun system manually
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_earth_clear_day()
);

// Primary sun (yellow)
atmos.celestial_bodies[0].type = CelestialBodyType::Sun;
atmos.celestial_bodies[0].direction = {0.0f, 0.8f, 0.6f};
atmos.celestial_bodies[0].intensity = 1200.0f;  // W/m²
atmos.celestial_bodies[0].base_color_rgb = {1.0f, 1.0f, 0.95f};  // Yellow-white
atmos.celestial_bodies[0].angular_diameter_rad = 0.01f;

// Secondary sun (red dwarf)
atmos.celestial_bodies[1].type = CelestialBodyType::Sun;
atmos.celestial_bodies[1].direction = {0.5f, 0.7f, -0.3f};
atmos.celestial_bodies[1].intensity = 300.0f;  // Dimmer
atmos.celestial_bodies[1].base_color_rgb = {1.0f, 0.5f, 0.3f};  // Red-orange
atmos.celestial_bodies[1].angular_diameter_rad = 0.006f;  // Smaller

atmos.num_celestial_bodies = 2;
atmos.primary_sun_index = 0;  // Primary sun for main lighting
```

### Adding Moons

```cpp
// Add Earth's moon
CelestialBody moon;
moon.type = CelestialBodyType::Moon;
moon.direction = {0.3f, 0.5f, -0.8f};
moon.intensity = 0.0032f;  // W/m² (full moon)
moon.base_color_rgb = {0.95f, 0.93f, 0.88f};  // Slightly yellow
moon.angular_diameter_rad = 0.0089f;  // 0.51°
moon.phase = 1.0f;  // Full moon (0.0 = new, 0.5 = half, 1.0 = full)
moon.casts_light = true;  // Provides illumination at night

atmos.add_celestial_body(moon);
```

### Adding Visible Planets

```cpp
// Add Jupiter visible in sky
CelestialBody jupiter;
jupiter.type = CelestialBodyType::Planet;
jupiter.direction = {-0.5f, 0.3f, 0.8f};
jupiter.intensity = 0.4f;  // Albedo (reflectivity, not W/m²)
jupiter.base_color_rgb = {0.9f, 0.85f, 0.75f};  // Cream color
jupiter.angular_diameter_rad = 0.008f;  // Visible disk
jupiter.casts_light = false;  // Doesn't provide illumination
jupiter.visible = true;

atmos.add_celestial_body(jupiter);
```

### Adding Planetary Rings

```cpp
// Saturn-like ring system
CelestialBody rings;
rings.type = CelestialBodyType::Ring;
rings.direction = {0.0f, 1.0f, 0.0f};  // Ring plane normal
rings.base_color_rgb = {0.9f, 0.85f, 0.8f};  // Icy white-cream
rings.ring_inner_radius = 1.5f;  // In planet radii
rings.ring_outer_radius = 2.5f;
rings.ring_tilt_rad = 0.3f;  // Tilt angle
rings.intensity = 0.5f;  // Brightness/opacity
rings.visible = true;
rings.casts_light = false;

atmos.add_celestial_body(rings);
```

## Configuration Parameters

### Rayleigh Scattering (Blue Sky)

```cpp
// Standard Earth
atmos.rayleigh_scattering_rgb = {5.8e-6f, 13.5e-6f, 33.1e-6f};  // R, G, B
atmos.rayleigh_scale_height_m = 8500.0f;  // Exponential falloff
atmos.rayleigh_density_multiplier = 1.0f;  // Overall strength

// Alien variations
// Redder sky: increase R, decrease B
atmos.rayleigh_scattering_rgb = {15.0e-6f, 10.0e-6f, 6.0e-6f};

// Deeper blue: increase B
atmos.rayleigh_scattering_rgb = {4.0e-6f, 10.0e-6f, 40.0e-6f};
```

### Mie Scattering (Haze/Pollution)

```cpp
// Clear air
atmos.mie_scattering_coeff = 2.0e-6f;
atmos.pollution_density_kg_m3 = 0.0f;

// Light haze
atmos.mie_scattering_coeff = 1.0e-5f;
atmos.pollution_density_kg_m3 = 0.00001f;

// Heavy pollution (brown smog)
atmos.mie_scattering_coeff = 5.0e-5f;
atmos.pollution_density_kg_m3 = 0.0001f;
atmos.pollution_color_tint = {0.7f, 0.6f, 0.5f};  // Brown

// Toxic green gas
atmos.pollution_density_kg_m3 = 0.0003f;
atmos.pollution_color_tint = {0.6f, 1.0f, 0.6f};  // Green
```

### Distance Fog

```cpp
// Clear day (100km visibility)
atmos.fog_density = 0.00001f;

// Light fog (10km visibility)
atmos.fog_density = 0.0001f;

// Heavy fog (1km visibility)
atmos.fog_density = 0.001f;

// Dense fog (100m visibility)
atmos.fog_density = 0.01f;

// Height-based fog (ground fog)
atmos.fog_height_falloff_m = 200.0f;  // Fog concentrated near ground
atmos.fog_start_distance_m = 10.0f;   // Fog starts at 10m

// Custom fog color (override atmospheric color)
atmos.fog_color_override_rgb = {0.8f, 0.9f, 1.0f};  // Blue-white fog
```

### Volumetric God Rays

```cpp
// Enable volumetric lighting
atmos.enable_volumetric_lighting = true;
atmos.volumetric_samples = 64;  // Quality (32=low, 64=medium, 128=high)
atmos.volumetric_intensity = 0.5f;  // Brightness (0.1-1.0)
atmos.volumetric_noise_scale = 0.3f;  // Detail variation (0.0-1.0)

// Dramatic god rays
atmos.volumetric_intensity = 0.8f;
atmos.volumetric_samples = 96;
atmos.volumetric_noise_scale = 0.6f;
```

### Weather Effects

```cpp
// Cloudy
atmos.cloud_coverage = 0.6f;  // 0.0 = clear, 1.0 = overcast
atmos.cloud_opacity = 0.7f;
atmos.cloud_color_tint = {0.9f, 0.9f, 0.95f};  // White clouds

// Storm clouds
atmos.cloud_coverage = 1.0f;
atmos.cloud_opacity = 0.95f;
atmos.cloud_color_tint = {0.3f, 0.3f, 0.35f};  // Dark gray

// Rain
atmos.rain_intensity = 0.8f;  // 0.0-1.0
atmos.fog_density = 0.0003f;  // Rain reduces visibility

// Snow
atmos.snow_intensity = 0.5f;
atmos.fog_density = 0.0002f;
```

### Time of Day

```cpp
// Manual sun position
atmos.sun_direction = {0.866f, 0.5f, 0.0f};  // Normalized vector

// Automatic sun position (astronomical calculation)
atmos.auto_update_sun_position = true;
atmos.time_of_day_hours = 6.0f;  // 6 AM (sunrise)
atmos.day_of_year = 172;  // June 21 (summer solstice)
atmos.latitude_degrees = 51.5f;  // London latitude

// Animate time of day
atmos.time_of_day_hours += delta_time * 0.01f;  // 1 hour per 100 seconds
atmos.update_sun_position_from_time();
```

## Integration Examples

### Example 1: Day/Night Cycle with Dual Suns

```cpp
// Tatooine-inspired dual sun system
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_desert_planet()
);

// Primary sun (Tatoo 1)
atmos.celestial_bodies[0].direction = {0.6f, 0.7f, 0.4f};
atmos.celestial_bodies[0].intensity = 1200.0f;
atmos.celestial_bodies[0].base_color_rgb = {1.0f, 0.95f, 0.9f};

// Secondary sun (Tatoo 2, slightly dimmer and redder)
atmos.celestial_bodies[1].type = CelestialBodyType::Sun;
atmos.celestial_bodies[1].direction = {0.5f, 0.65f, -0.5f};
atmos.celestial_bodies[1].intensity = 900.0f;
atmos.celestial_bodies[1].base_color_rgb = {1.0f, 0.8f, 0.6f};
atmos.num_celestial_bodies = 2;

// Update loop: Both suns orbit
float orbit_speed = 0.1f;
atmos.celestial_bodies[0].direction = rotate_vector(
    atmos.celestial_bodies[0].direction,
    {0, 1, 0},
    orbit_speed * delta_time
);
atmos.celestial_bodies[1].direction = rotate_vector(
    atmos.celestial_bodies[1].direction,
    {0, 1, 0},
    orbit_speed * delta_time * 0.9f  // Slightly different orbit
);
```

### Example 2: Gas Giant with Visible Moons and Rings

```cpp
// Standing on moon of gas giant
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_ice_planet()
);

// Primary sun (distant)
atmos.celestial_bodies[0].intensity = 400.0f;  // Farther from star

// Massive gas giant in sky
CelestialBody gas_giant;
gas_giant.type = CelestialBodyType::Planet;
gas_giant.direction = {0.2f, 0.4f, 0.9f};  // Dominates sky
gas_giant.intensity = 0.5f;  // Albedo
gas_giant.base_color_rgb = {0.9f, 0.7f, 0.6f};  // Jupiter-like
gas_giant.angular_diameter_rad = 0.3f;  // HUGE in sky (17°)
gas_giant.visible = true;
atmos.add_celestial_body(gas_giant);

// Gas giant's ring system
CelestialBody rings;
rings.type = CelestialBodyType::Ring;
rings.direction = {0.2f, 0.4f, 0.9f};  // Same as planet
rings.base_color_rgb = {0.85f, 0.8f, 0.75f};
rings.ring_inner_radius = 1.2f;
rings.ring_outer_radius = 2.0f;
rings.ring_tilt_rad = 0.4f;
rings.intensity = 0.6f;
rings.visible = true;
atmos.add_celestial_body(rings);

// Sister moon (fellow satellite)
CelestialBody sister_moon;
sister_moon.type = CelestialBodyType::Moon;
sister_moon.direction = {-0.6f, 0.3f, 0.7f};
sister_moon.intensity = 0.001f;  // Dim
sister_moon.base_color_rgb = {0.6f, 0.6f, 0.65f};  // Gray
sister_moon.angular_diameter_rad = 0.005f;  // Small
sister_moon.phase = 0.7f;  // Gibbous
atmos.add_celestial_body(sister_moon);

atmos.num_celestial_bodies = 4;  // Sun, gas giant, rings, sister moon
```

### Example 3: Polluted Cyberpunk City

```cpp
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_earth_polluted_city()
);

// Heavy industrial pollution
atmos.pollution_density_kg_m3 = 0.0002f;  // Hazardous levels
atmos.pollution_color_tint = {0.8f, 0.6f, 0.4f};  // Brown-orange smog

// Poor visibility
atmos.fog_density = 0.0008f;  // 1.25km visibility
atmos.fog_height_falloff_m = 300.0f;  // Smog concentrated near ground

// Overcast (pollution blocks sun)
atmos.cloud_coverage = 0.8f;
atmos.cloud_opacity = 0.9f;
atmos.cloud_color_tint = {0.5f, 0.45f, 0.4f};  // Dirty clouds

// Volumetric god rays through smog
atmos.volumetric_intensity = 0.7f;
atmos.volumetric_noise_scale = 0.6f;

// Sun barely visible through pollution
atmos.sun_intensity_w_m2 = 400.0f;  // Blocked by smog
```

### Example 4: Aurora Borealis Planet

```cpp
auto& atmos = world.add_component<AtmosphericComponent>(
    atmosphere_entity,
    AtmosphericComponent::create_aurora_world()
);

// Strong magnetic field creates permanent aurora
atmos.enable_aurora = true;
atmos.aurora_altitude_m = 100000.0f;
atmos.aurora_intensity = 0.9f;  // Very bright
atmos.aurora_color_rgb = {0.2f, 1.0f, 0.4f};  // Green (oxygen)

// Additional aurora layers (red high-altitude oxygen)
// Would require custom aurora system extension

// Enhanced volumetric lighting for aurora visibility
atmos.volumetric_intensity = 0.8f;
atmos.volumetric_samples = 96;  // High quality for aurora

// Slight green tint to atmosphere
atmos.rayleigh_scattering_rgb = {5.0e-6f, 15.0e-6f, 28.0e-6f};
```

### Example 5: Dynamic Weather System

```cpp
// Weather state machine
enum class Weather { Clear, Cloudy, Foggy, Rainy, Stormy };
Weather current_weather = Weather::Clear;
float weather_transition_timer = 0.0f;

void update_weather(AtmosphericComponent& atmos, float delta_time) {
    weather_transition_timer -= delta_time;

    if (weather_transition_timer <= 0.0f) {
        // Change weather randomly
        current_weather = static_cast<Weather>(rand() % 5);
        weather_transition_timer = 300.0f + rand() % 600;  // 5-15 minutes
    }

    // Smooth transition to target weather
    float transition_speed = delta_time * 0.1f;

    switch (current_weather) {
        case Weather::Clear:
            atmos.cloud_coverage = lerp(atmos.cloud_coverage, 0.0f, transition_speed);
            atmos.fog_density = lerp(atmos.fog_density, 0.00001f, transition_speed);
            atmos.rain_intensity = lerp(atmos.rain_intensity, 0.0f, transition_speed);
            break;

        case Weather::Cloudy:
            atmos.cloud_coverage = lerp(atmos.cloud_coverage, 0.7f, transition_speed);
            atmos.fog_density = lerp(atmos.fog_density, 0.00005f, transition_speed);
            break;

        case Weather::Foggy:
            atmos.cloud_coverage = lerp(atmos.cloud_coverage, 0.3f, transition_speed);
            atmos.fog_density = lerp(atmos.fog_density, 0.001f, transition_speed);
            break;

        case Weather::Rainy:
            atmos.cloud_coverage = lerp(atmos.cloud_coverage, 0.9f, transition_speed);
            atmos.rain_intensity = lerp(atmos.rain_intensity, 0.7f, transition_speed);
            atmos.fog_density = lerp(atmos.fog_density, 0.0003f, transition_speed);
            break;

        case Weather::Stormy:
            atmos.cloud_coverage = lerp(atmos.cloud_coverage, 1.0f, transition_speed);
            atmos.cloud_opacity = lerp(atmos.cloud_opacity, 0.95f, transition_speed);
            atmos.rain_intensity = lerp(atmos.rain_intensity, 1.0f, transition_speed);
            atmos.fog_density = lerp(atmos.fog_density, 0.0005f, transition_speed);
            atmos.volumetric_intensity = lerp(atmos.volumetric_intensity, 0.9f, transition_speed);
            break;
    }
}
```

## Performance

### LUT Generation (One-Time Cost)

| LUT | Resolution | Samples | Time |
|-----|------------|---------|------|
| Transmittance | 256×64 | 40 | ~3ms |
| Scattering | 200×128×32 | 40 | ~7ms |
| **Total** | | | **~10ms** |

LUTs regenerate only when atmosphere parameters change.

### Per-Frame Cost

| Operation | Time | Description |
|-----------|------|-------------|
| Sun color calculation | ~0.1ms | Update celestial body colors |
| Atmospheric apply (1080p) | ~1.5ms | Full-screen atmospheric effects |
| Volumetric god rays (64 samples) | +0.5ms | Optional volumetric lighting |
| **Total (typical)** | **~2ms** | < 4% of 16ms budget (60 FPS) |

### LOD System

Distance-based quality scaling:
- **Close (< 1km)**: Full quality, all effects
- **Medium (1-5km)**: Reduced volumetric samples (32 instead of 64)
- **Far (> 5km)**: Aerial perspective only, skip volumetrics

## Shader Pipeline

### 1. Transmittance LUT Generation (`atmospheric_transmittance.comp`)
- Computes atmospheric absorption for all view angles and altitudes
- 256×64 2D texture, RGB channels
- Integration: 40 samples along ray
- Updates only when atmosphere parameters change

### 2. Scattering LUT Generation (`atmospheric_scattering.comp`)
- Computes single scattering (sky color) for all directions
- 200×128×32 3D texture, RGB channels
- Integration: 40 samples along ray, samples transmittance LUT
- Updates only when atmosphere parameters change

### 3. Atmospheric Application (`atmospheric_apply.comp`)
- Runs every frame on rendered scene
- Samples transmittance and scattering LUTs
- Applies aerial perspective, distance fog, volumetric god rays
- Full-screen compute shader (8×8 workgroups)

## Troubleshooting

### Sun color looks wrong
- Check `sun_base_color_rgb` is in linear color space (not sRGB)
- Verify `sun_direction` is normalized
- Ensure `surface_altitude_m` is correct (affects optical depth)
- Try increasing `rayleigh_density_multiplier` for more scattering

### Sky is too bright/dark
- Adjust `sun_intensity_w_m2` (Earth = 1361)
- Modify `rayleigh_density_multiplier` (1.0 = Earth standard)
- Check exposure/tone mapping in post-processing

### Fog is too thick
- Reduce `fog_density` (0.00001 = clear, 0.01 = very dense)
- Increase `fog_start_distance_m` to push fog farther away
- Adjust `fog_height_falloff_m` for height-based fog

### God rays not visible
- Increase `volumetric_intensity` (0.5-1.0)
- Increase `volumetric_samples` (64-128 for quality)
- Ensure `enable_volumetric_lighting = true`
- Check sun is above horizon (positive y component in direction)

### Pollution color not showing
- Verify `pollution_density_kg_m3 > 0`
- Increase `mie_scattering_coeff` for more haze
- Set `pollution_color_tint` to desired color
- Check `mie_scale_height_m` (lower = more pollution near ground)

### Multiple suns not rendering
- Verify `num_celestial_bodies` is set correctly
- Ensure all suns have `casts_light = true`
- Check directions are normalized
- Verify intensities are reasonable (> 0)

## API Reference

### AtmosphericComponent Methods

```cpp
// Factory presets
static AtmosphericComponent create_earth_clear_day();
static AtmosphericComponent create_mars();
static AtmosphericComponent create_dual_sun();
// ... 17 more presets

// Celestial body management
int32_t add_celestial_body(const CelestialBody& body);
void remove_celestial_body(uint32_t index);
const CelestialBody& get_primary_sun() const;

// Calculations
math::Vec3 calculate_sun_color(const math::Vec3& view_direction) const;
math::Vec3 calculate_sky_color(const math::Vec3& view_direction) const;
float calculate_optical_depth(float altitude_m, float zenith_angle_rad) const;
float calculate_visibility_distance() const;

// Time/position
void update_sun_position_from_time();  // Astronomical calculation
float get_sun_elevation() const;  // Radians above horizon
float get_sun_azimuth() const;    // Radians from north
```

### AtmosphericSystem Methods

```cpp
bool initialize(GPUComputeContext& gpu, const Config& config);
void update(World& world, float delta_time_s);
void apply_atmospheric_effects(
    VkCommandBuffer cmd,
    VkImage scene_color,
    VkImage depth,
    VkImage output,
    const World& world,
    const math::Vec3& camera_pos,
    const float* view_proj_matrix
);
void regenerate_luts(World& world);

// Static utilities
static const AtmosphericComponent* get_atmosphere(const World& world);
static math::Vec3 calculate_sun_color(const AtmosphericComponent&, const math::Vec3&);
```

## Related Systems

- **VolumetricSmokeComponent**: Uses atmospheric light color for smoke illumination
- **VolumetricFireComponent**: Fire glow affected by atmospheric haze
- **VisionSystem**: Visibility affected by fog and pollution
- **LightingSystem**: Directional lights use calculated sun color
- **WeatherSystem**: Rain/snow affects atmospheric parameters

---

**Note**: All units are SI (meters, kilograms, Kelvin, Watts) unless otherwise specified. Color values are in linear RGB space (not sRGB). Angles are in radians unless specified as degrees.