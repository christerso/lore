# Environment Controller - Real-Time Lighting & Color System

## Overview

The EnvironmentController provides real-time control over time of day, lighting, contrast, and colors. Perfect for artists iterating on atmosphere without code changes or restarts.

**Key Features:**
- **Time of Day Control**: 0-24 hours with automatic sun positioning
- **Post-Processing**: Contrast, brightness, color temperature, saturation
- **Lighting**: Sun intensity, shadows, fog, ambient
- **10 Built-In Presets**: Mirror's Edge bright/indoor, high/low contrast, etc.
- **Real-Time Tweaking**: ImGui panel for instant feedback
- **Smooth Transitions**: Fade between times of day
- **Save/Load Presets**: Create custom looks

## Quick Start

### Code Setup

```cpp
#include "lore/ecs/systems/environment_controller.hpp"
#include "lore/ui/environment_debug_panel.hpp"

// Initialize
EnvironmentController env_controller;
env_controller.initialize(world);

// ImGui panel
EnvironmentDebugPanel debug_panel(env_controller);

// Game loop
void update(float delta_time) {
    env_controller.update(world, delta_time);
}

void render_ui() {
    debug_panel.render();  // ImGui panel
}
```

### Using Presets

```cpp
// Apply Mirror's Edge style
env_controller.apply_preset("mirrors_edge_bright");

// Change time of day
env_controller.set_time_of_day(18.5f);  // 6:30 PM sunset
```

## Mirror's Edge Prototype Style

Perfect for untextured white geometry with crisp shadows:

```cpp
// Apply Mirror's Edge preset
env_controller.apply_preset("mirrors_edge_bright");

// What you get:
// - Clean white surfaces (high contrast)
// - Bright, harsh sun (crisp shadows)
// - Minimal fog/haze (clear visibility)
// - Slightly warm tone
// - Strong ambient occlusion (depth in corners)
```

### Mirror's Edge Parameters

| Setting | Value | Effect |
|---------|-------|--------|
| **Contrast** | 1.3 | High contrast (bright whites, dark shadows) |
| **Exposure** | +0.3 EV | Brighter than default |
| **Saturation** | 0.9 | Slightly desaturated (clean look) |
| **Sun Intensity** | 1.2× | Harsh, bright sun |
| **Ambient** | 0.4× | Low ambient (high contrast) |
| **Shadow Strength** | 1.0 | Full shadows |
| **AO Intensity** | 0.8 | Strong AO for depth |

### Mirror's Edge Indoor Variant

```cpp
env_controller.apply_preset("mirrors_edge_indoor");

// Differences:
// - Lower contrast (1.15 vs 1.3)
// - More ambient light (0.8× vs 0.4×)
// - Softer shadows (0.7 vs 1.0)
// - No fog
```

## Available Presets

### 1. Mirror's Edge Bright
**Use for**: Outdoor prototyping, clean white geometry
**Time**: Noon
**Style**: High contrast, bright, crisp shadows

```cpp
env_controller.apply_preset("mirrors_edge_bright");
```

### 2. Mirror's Edge Indoor
**Use for**: Indoor levels, more ambient light
**Time**: Noon
**Style**: Moderate contrast, softer shadows

### 3. High Contrast
**Use for**: Dramatic lighting, strong shapes
**Contrast**: 1.5 (very high)
**Sun**: 1.3× intensity

### 4. Low Contrast
**Use for**: Soft, diffuse lighting
**Contrast**: 0.75 (low)
**Ambient**: 0.9× (high)

### 5. Warm Sunset
**Use for**: Golden hour, warm atmosphere
**Time**: 6:30 PM
**Temperature**: +0.35 (very warm orange)

### 6. Cool Morning
**Use for**: Dawn, cool blue tones
**Time**: 7:00 AM
**Temperature**: -0.15 (cool blue)

### 7. Neutral Noon
**Use for**: Reference/baseline
**Everything**: 1.0× (neutral)

### 8. Moody Overcast
**Use for**: Gray, low-saturation atmosphere
**Saturation**: 0.7
**Fog**: 2.0× density

### 9. Cinematic Night
**Use for**: Dark, dramatic night scenes
**Time**: 10:00 PM
**Temperature**: -0.25 (blue tint)
**Brightness**: -0.4

### 10. Vibrant Day
**Use for**: Colorful, saturated look
**Saturation**: 1.4
**Vibrance**: +0.3

## Manual Control Examples

### Change Time of Day

```cpp
// Instant change
env_controller.set_time_of_day(6.0f);   // 6:00 AM
env_controller.set_time_of_day(12.5f);  // 12:30 PM
env_controller.set_time_of_day(18.0f);  // 6:00 PM
env_controller.set_time_of_day(22.0f);  // 10:00 PM

// Smooth transition (fade over 5 seconds)
env_controller.transition_to_time(18.5f, 5.0f);  // Sunset over 5s

// Animated day/night cycle
env_controller.advance_time(0.01f, delta_time);  // 1 hour per 100 seconds
```

### Tweak Post-Processing

```cpp
// Increase contrast (Mirror's Edge style)
env_controller.set_contrast(1.3f);  // 1.0 = neutral, 1.5 = very high

// Make brighter
env_controller.set_exposure(0.5f);  // +0.5 EV (1.4× brighter)

// Warm color temperature
env_controller.set_color_temperature(0.2f);  // Orange tint

// Cool color temperature
env_controller.set_color_temperature(-0.2f); // Blue tint

// Desaturate (clean look)
env_controller.set_saturation(0.8f);  // 0 = grayscale, 1 = normal, 2 = oversaturated

// Add vignette
env_controller.set_vignette(0.3f);  // 0 = none, 1 = strong
```

### Adjust Lighting

```cpp
// Brighter sun
env_controller.set_sun_intensity(1.5f);  // 1.5× default

// More ambient (less contrast)
env_controller.set_ambient_intensity(1.2f);

// Reduce shadow strength
env_controller.set_shadow_strength(0.7f);  // 0 = no shadows, 1 = full

// Add fog
env_controller.set_fog_density(2.0f);  // 2× default density

// Custom fog color
env_controller.set_fog_color({0.8f, 0.9f, 1.0f});  // Blue fog
```

## ImGui Debug Panel

### Features

**4 Collapsible Sections:**
1. **Quick Presets**: 10 one-click presets
2. **Time of Day**: 24-hour slider, 8 quick time buttons, smooth transitions
3. **Post-Processing**: Exposure, contrast, color grading, saturation, vignette
4. **Lighting**: Sun, ambient, shadows, fog

### Keyboard Shortcuts (Suggested)

```cpp
// In your input handling
if (input.is_key_pressed(Key::F1)) {
    debug_panel.set_visible(!debug_panel.is_visible());
}

if (input.is_key_pressed(Key::F2)) {
    env_controller.apply_preset("mirrors_edge_bright");
}

if (input.is_key_pressed(Key::F3)) {
    env_controller.apply_noon();
}
```

### Panel Usage

1. **Open panel**: `F1` key (or custom binding)
2. **Click preset button**: Instant change
3. **Drag sliders**: Real-time preview
4. **Click "Reset" buttons**: Return to defaults

## Creating Custom Presets

### In Code

```cpp
// Create custom preset
EnvironmentController::Preset my_preset;
my_preset.name = "My Custom Look";
my_preset.time_of_day_hours = 14.0f;  // 2 PM

// Post-processing
my_preset.post_processing.contrast = 1.25f;
my_preset.post_processing.temperature = 0.1f;  // Warm
my_preset.post_processing.saturation = 1.1f;

// Lighting
my_preset.lighting.sun_intensity_multiplier = 1.1f;
my_preset.lighting.ambient_intensity_multiplier = 0.6f;
my_preset.lighting.shadow_strength = 0.9f;

// Save preset
env_controller.save_preset("my_custom_look");

// Apply later
env_controller.apply_preset("my_custom_look");
```

### From ImGui Panel

1. Adjust sliders to desired look
2. (Future): Click "Save Current as Preset" button
3. Enter preset name
4. Preset saved to list

## Integration with Other Systems

### With Atmospheric System

The EnvironmentController modifies AtmosphericComponent:

```cpp
// EnvironmentController automatically updates:
auto& atmos = world.get_component<AtmosphericComponent>(atmos_entity);

// - Sun position (from time of day)
atmos.update_sun_position_from_time();

// - Sun intensity
atmos.sun_intensity_w_m2 *= lighting.sun_intensity_multiplier;

// - Fog density
atmos.fog_density *= lighting.fog_density_multiplier;
```

### With Deferred Renderer

Post-processing settings feed into tone mapping:

```glsl
// In tone mapping shader
uniform float u_exposure_ev;
uniform float u_contrast;
uniform float u_brightness;
uniform float u_temperature;
uniform float u_saturation;

// Apply in order:
vec3 color = scene_color;
color = apply_exposure(color, u_exposure_ev);
color = apply_contrast(color, u_contrast, u_brightness);
color = apply_color_grading(color, u_temperature, u_saturation);
color = apply_tone_mapping(color);  // ACES or Reinhard
```

### With Material System

For Mirror's Edge prototyping:

```cpp
// All materials use clean white albedo
for (auto entity : geometry_entities) {
    auto& material = world.get_component<MaterialComponent>(entity);
    material.albedo = {0.95f, 0.95f, 0.95f};  // White
    material.roughness = 0.6f;
    material.metallic = 0.0f;
}

// Important objects get color accents
auto& door_material = world.get_component<MaterialComponent>(door_entity);
door_material.albedo = {0.95f, 0.1f, 0.1f};  // Red accent (Mirror's Edge style)
```

## Performance

**CPU Cost**: ~0.05ms per frame
- Updates only change a few floats
- No expensive calculations

**GPU Cost**: Integrated with existing post-processing
- Tone mapping already runs
- EnvironmentController just changes uniforms

## Workflow: Prototyping to Final

### Phase 1: White Geometry (Mirror's Edge Style)

```cpp
// Start with clean prototype
env_controller.apply_preset("mirrors_edge_bright");

// All geometry white, no textures
// Focus on:
// - Level layout
// - Shadow placement
// - Lighting direction
// - Contrast
```

### Phase 2: Time of Day Testing

```cpp
// Test different times
env_controller.apply_dawn();      // Soft lighting
env_controller.apply_noon();      // Harsh shadows
env_controller.apply_golden_hour(); // Warm atmosphere
env_controller.apply_night();     // Dark mood

// Pick best time for your level
```

### Phase 3: Color Grading

```cpp
// Adjust colors for mood
env_controller.set_color_temperature(0.15f);  // Slightly warm
env_controller.set_saturation(0.95f);         // Slightly desaturated
env_controller.set_contrast(1.2f);            // Punchier
```

### Phase 4: Add Textures

```cpp
// Replace white albedo with textures
material.albedo_texture = load_texture("brick.png");

// Keep same lighting/post-processing!
// Textures just replace white color
```

### Phase 5: Final Polish

```cpp
// Fine-tune everything
// Save as custom preset
env_controller.save_preset("level_01_final");
```

## Best Practices

### For Prototyping

1. **Start with Mirror's Edge Bright**: Clean, clear, high contrast
2. **Focus on shapes first**: Lighting reveals form
3. **Use color accents**: Red = important, blue = interactive (Mirror's Edge convention)
4. **Iterate quickly**: ImGui panel allows instant feedback

### For Production

1. **Test all times of day**: Level should work at multiple times
2. **Save presets**: One per level or mood
3. **Smooth transitions**: Use `transition_to_time()` for cinematics
4. **Match art direction**: Adjust contrast/saturation to game's style

### Performance Tips

1. **Disable auto-exposure**: Manual exposure is faster
2. **Reduce vignette**: Costs fillrate if not needed
3. **Optimize fog**: High fog density = more raymarching cost
4. **LOD shadows**: Reduce shadow resolution for distant objects

## Troubleshooting

### Shadows too harsh
```cpp
env_controller.set_shadow_strength(0.7f);
env_controller.set_ambient_intensity(0.8f);
```

### Too dark
```cpp
env_controller.set_exposure(0.5f);  // Brighter
env_controller.set_brightness(0.2f);
```

### Too washed out
```cpp
env_controller.set_contrast(1.3f);  // More contrast
env_controller.set_saturation(1.1f);
```

### Wrong color tone
```cpp
// Too blue? Make warmer
env_controller.set_color_temperature(0.2f);

// Too orange? Make cooler
env_controller.set_color_temperature(-0.2f);
```

## API Reference

### Time Control

```cpp
void set_time_of_day(float hours);  // 0-24
void transition_to_time(float target_hours, float duration_s);
void advance_time(float hours_per_second, float delta_time_s);
void set_day_of_year(uint32_t day);  // 1-365
void set_latitude(float degrees);    // -90 to +90
```

### Quick Times

```cpp
void apply_dawn();          // 6:00 AM
void apply_morning();       // 9:00 AM
void apply_noon();          // 12:00 PM
void apply_afternoon();     // 3:00 PM
void apply_golden_hour();   // 6:30 PM
void apply_dusk();          // 8:00 PM
void apply_night();         // 12:00 AM
void apply_midnight();      // 2:00 AM
```

### Post-Processing

```cpp
void set_exposure(float ev_offset);         // -5 to +5
void set_contrast(float contrast);          // 0.5 to 2.0
void set_brightness(float brightness);      // -1 to +1
void set_color_temperature(float temp);     // -1 to +1
void set_saturation(float saturation);      // 0 to 2
void set_vignette(float intensity);         // 0 to 1
```

### Lighting

```cpp
void set_sun_intensity(float multiplier);     // 0.5 to 2.0
void set_ambient_intensity(float multiplier); // 0.0 to 2.0
void set_shadow_strength(float strength);     // 0 to 1
void set_fog_density(float multiplier);       // 0.1 to 5.0
void set_fog_color(const math::Vec3& color);
```

### Presets

```cpp
void apply_preset(const std::string& name);
void save_preset(const std::string& name);
std::vector<std::string> get_preset_names() const;
```

---

**Note**: All changes are real-time - no restart required. Use ImGui panel for instant iteration!