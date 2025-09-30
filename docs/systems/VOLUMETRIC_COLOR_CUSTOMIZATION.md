# Volumetric Effects Color Customization Guide

## Overview

All volumetric effects (fire, smoke, explosions) support full color customization through temperature-based gradients. This allows creating:
- **Magic flames** (any color)
- **Colored smoke** (poison gas, magic effects)
- **Custom explosions** (magical, sci-fi, etc.)
- **Special effects** (portals, energy fields)

## Color System Architecture

All volumetric components use the same color system:
```cpp
struct ColorGradientPoint {
    float temperature_k;    // Temperature at this point (K)
    math::Vec4 color;       // RGBA (R, G, B, Emission)
};

std::vector<ColorGradientPoint> temperature_colors;
```

**How it works**:
1. Each voxel has a temperature value
2. GPU shader samples the gradient based on temperature
3. Linear interpolation between gradient points
4. Alpha channel controls emission intensity

## Magic Flame Examples

### Blue Fire (Cold Flames)
```cpp
Entity blue_fire = world.create_entity();
auto fire = VolumetricFireComponent::create_campfire();

// Replace default gradient with blue
fire.temperature_colors = {
    {600.0f,  {0.0f, 0.05f, 0.2f, 0.5f}},   // Dark blue (cool)
    {800.0f,  {0.0f, 0.3f, 0.8f, 2.0f}},    // Blue
    {1000.0f, {0.2f, 0.6f, 1.0f, 4.0f}},    // Bright blue
    {1200.0f, {0.6f, 0.9f, 1.0f, 6.0f}},    // Cyan
    {1400.0f, {0.9f, 0.95f, 1.0f, 8.0f}},   // White-blue (hottest)
};

world.add_component(blue_fire, fire);
```

### Green Fire (Toxic/Poison)
```cpp
auto fire = VolumetricFireComponent::create_campfire();
fire.temperature_colors = {
    {600.0f,  {0.05f, 0.1f, 0.0f, 0.5f}},   // Dark green
    {800.0f,  {0.2f, 0.6f, 0.0f, 2.0f}},    // Green
    {1000.0f, {0.4f, 0.9f, 0.1f, 4.0f}},    // Bright green
    {1200.0f, {0.7f, 1.0f, 0.3f, 6.0f}},    // Yellow-green
    {1400.0f, {0.9f, 1.0f, 0.6f, 8.0f}},    // White-green
};
```

### Purple Fire (Arcane Magic)
```cpp
auto fire = VolumetricFireComponent::create_torch();
fire.temperature_colors = {
    {600.0f,  {0.1f, 0.0f, 0.2f, 0.5f}},    // Dark purple
    {800.0f,  {0.4f, 0.0f, 0.7f, 2.0f}},    // Purple
    {1000.0f, {0.7f, 0.2f, 1.0f, 4.0f}},    // Bright purple
    {1200.0f, {0.9f, 0.5f, 1.0f, 6.0f}},    // Magenta
    {1400.0f, {1.0f, 0.9f, 1.0f, 8.0f}},    // White-purple
};
```

### Rainbow Fire (Multi-Color)
```cpp
auto fire = VolumetricFireComponent::create_campfire();
fire.temperature_colors = {
    {600.0f,  {1.0f, 0.0f, 0.0f, 1.0f}},    // Red
    {800.0f,  {1.0f, 0.5f, 0.0f, 2.0f}},    // Orange
    {1000.0f, {1.0f, 1.0f, 0.0f, 3.0f}},    // Yellow
    {1100.0f, {0.0f, 1.0f, 0.0f, 4.0f}},    // Green
    {1200.0f, {0.0f, 0.5f, 1.0f, 5.0f}},    // Blue
    {1300.0f, {0.5f, 0.0f, 1.0f, 6.0f}},    // Purple
    {1400.0f, {1.0f, 1.0f, 1.0f, 8.0f}},    // White
};
```

### Black Fire (Shadow/Dark Magic)
```cpp
auto fire = VolumetricFireComponent::create_torch();
fire.temperature_colors = {
    {600.0f,  {0.0f, 0.0f, 0.0f, 0.0f}},    // Pure black
    {800.0f,  {0.05f, 0.0f, 0.05f, 0.2f}},  // Dark purple-black
    {1000.0f, {0.1f, 0.0f, 0.1f, 0.5f}},    // Barely visible
    {1200.0f, {0.2f, 0.0f, 0.2f, 1.0f}},    // Dark purple glow
    {1400.0f, {0.4f, 0.0f, 0.4f, 2.0f}},    // Deep purple
};

// Note: Black fire needs negative density influence
fire.config.smoke_opacity_multiplier = -0.5f; // Absorbs light
```

## Colored Smoke Examples

### Poison Gas (Green Smoke)
```cpp
Entity poison_cloud = world.create_entity();
auto smoke = VolumetricSmokeComponent();

smoke.resolution_x = 128;
smoke.resolution_y = 128;
smoke.resolution_z = 128;
smoke.cell_size_m = 0.1f;

// Green tinted smoke
smoke.base_color = {0.2f, 0.8f, 0.2f};  // Green
smoke.opacity = 0.9f;                   // Dense
smoke.rise_speed_m_s = 0.5f;            // Slow (heavier than air)

world.add_component(poison_cloud, smoke);
```

### Magic Mist (Blue)
```cpp
auto smoke = VolumetricSmokeComponent();
smoke.base_color = {0.3f, 0.6f, 1.0f};  // Light blue
smoke.opacity = 0.6f;                    // Semi-transparent
smoke.rise_speed_m_s = -0.2f;            // Sinks (cold air)
smoke.noise_octaves = 6;                 // Very detailed
```

### Shadow Smoke (Dark)
```cpp
auto smoke = VolumetricSmokeComponent();
smoke.base_color = {0.05f, 0.0f, 0.1f}; // Very dark purple
smoke.opacity = 1.0f;                    // Completely opaque
smoke.rise_speed_m_s = 0.0f;             // Spreads horizontally
smoke.config.smoke_opacity_multiplier = 2.0f; // Extra dark
```

### Portal Effect (Swirling Colors)
```cpp
auto smoke = VolumetricSmokeComponent();

// Use multiple smoke entities with different colors
// overlapping at the portal location

// Layer 1: Purple core
auto purple_smoke = VolumetricSmokeComponent();
purple_smoke.base_color = {0.5f, 0.0f, 0.8f};
purple_smoke.source_radius_m = 1.0f;

// Layer 2: Blue outer
auto blue_smoke = VolumetricSmokeComponent();
blue_smoke.base_color = {0.0f, 0.3f, 1.0f};
blue_smoke.source_radius_m = 1.5f;

// Layer 3: White highlights
auto white_smoke = VolumetricSmokeComponent();
white_smoke.base_color = {1.0f, 1.0f, 1.0f};
white_smoke.source_radius_m = 0.5f;
```

## Colored Explosions

### Magic Explosion (Purple)
```cpp
Entity magic_explosion = world.create_entity();
auto explosion = ExplosionComponent::create_magic_explosion(
    {0.5f, 0.0f, 1.0f, 1.0f},  // Purple
    10.0f                        // 10m radius
);

// Flash color
explosion.flash_color = {1.0f, 0.5f, 1.0f, 1.0f}; // Bright magenta

// Fireball color
explosion.fireball_color = {0.7f, 0.0f, 1.0f, 1.0f}; // Purple

// Smoke color
explosion.smoke_color = {0.3f, 0.0f, 0.5f, 0.8f}; // Dark purple smoke

world.add_component(magic_explosion, explosion);
```

### Ice Explosion (Blue-White)
```cpp
auto explosion = ExplosionComponent::create_magic_explosion(
    {0.7f, 0.9f, 1.0f, 1.0f},  // Light blue
    8.0f
);

explosion.flash_color = {0.9f, 0.95f, 1.0f, 1.0f}; // Icy white
explosion.fireball_color = {0.5f, 0.8f, 1.0f, 1.0f}; // Blue
explosion.smoke_color = {0.8f, 0.9f, 1.0f, 0.6f}; // Misty white-blue
explosion.fireball_temperature_k = 200.0f; // Cold!
```

### Energy Explosion (Yellow-Green)
```cpp
auto explosion = ExplosionComponent::create_magic_explosion(
    {0.8f, 1.0f, 0.2f, 1.0f},  // Yellow-green
    15.0f
);

explosion.flash_color = {1.0f, 1.0f, 0.5f, 1.0f}; // Bright yellow
explosion.fireball_color = {0.9f, 1.0f, 0.3f, 1.0f}; // Electric yellow-green
explosion.smoke_color = {0.4f, 0.6f, 0.1f, 0.7f}; // Green smoke
explosion.flash_intensity = 200.0f; // Very bright
```

## Temperature-Independent Colors

For effects where temperature doesn't make sense, use **single-point gradients**:

```cpp
// Constant color (no temperature variation)
auto fire = VolumetricFireComponent::create_torch();
fire.temperature_colors = {
    {0.0f,    {0.5f, 0.0f, 1.0f, 5.0f}},  // Purple at any temp
    {10000.0f, {0.5f, 0.0f, 1.0f, 5.0f}}, // Same purple at any temp
};
```

## Advanced: Multi-Layer Colored Effects

### Colored Flames with Matching Smoke
```cpp
// Create fire entity
Entity fire = world.create_entity();
auto fire_comp = VolumetricFireComponent::create_campfire();

// Green flames
fire_comp.temperature_colors = {
    {600.0f,  {0.05f, 0.2f, 0.0f, 0.5f}},
    {1400.0f, {0.5f, 1.0f, 0.2f, 8.0f}},
};

world.add_component(fire, fire_comp);

// Create matching smoke
Entity smoke = world.create_entity();
auto smoke_comp = VolumetricSmokeComponent::create_from_fire(
    fire_comp.source_position,
    fire_comp.source_radius_m
);

// Match smoke color to fire
smoke_comp.base_color = {0.2f, 0.4f, 0.1f}; // Dark green
smoke_comp.opacity = 0.8f;

world.add_component(smoke, smoke_comp);
```

### Explosion with Persistent Colored Smoke
```cpp
// Trigger explosion
Entity explosion_entity = world.create_entity();
auto explosion = ExplosionComponent::create_c4_explosion();

// Red explosion
explosion.flash_color = {1.0f, 0.5f, 0.3f, 1.0f};
explosion.fireball_color = {1.0f, 0.2f, 0.0f, 1.0f};
explosion.smoke_color = {0.3f, 0.1f, 0.05f, 0.9f}; // Dark red-brown

world.add_component(explosion_entity, explosion);
explosion_system.trigger(world, explosion_entity, position);

// Explosion system will automatically create matching smoke plume
// with the configured smoke_color
```

## Integration with Shaders

The color gradients are sampled in the GPU raymarching shader:

```glsl
// In fire raymarching shader
float temperature = texture(temperature_field, uvw).r;

// Sample color gradient based on temperature
vec4 fire_color = sample_gradient(temperature_colors, temperature);

// Accumulate along ray
color += fire_color.rgb * fire_color.a * density * step_size;
```

## Performance Notes

**Color complexity does NOT affect performance!**
- Color lookup is constant time O(1)
- Gradient interpolation is 2 texture samples
- No FPS difference between 2 colors or 20 colors

**Guidelines**:
- **2-3 points**: Simple, clean gradients
- **5-7 points**: Smooth, detailed gradients
- **10+ points**: Only if you need very specific transitions

## Common Recipes

### Elemental Flames

```cpp
// Fire (standard)
{{800, {1.0, 0.0, 0.0, 2}}, {1400, {1.0, 0.8, 0.2, 8}}}

// Water/Ice
{{200, {0.5, 0.8, 1.0, 2}}, {400, {0.9, 0.95, 1.0, 8}}}

// Earth/Stone
{{600, {0.3, 0.2, 0.1, 1}}, {1000, {0.6, 0.5, 0.3, 4}}}

// Lightning/Electric
{{800, {0.8, 0.9, 1.0, 4}}, {1400, {0.95, 1.0, 1.0, 12}}}

// Nature/Life
{{600, {0.2, 0.5, 0.1, 1}}, {1200, {0.6, 1.0, 0.3, 6}}}

// Death/Necromancy
{{400, {0.05, 0.0, 0.1, 0.5}}, {1000, {0.2, 0.0, 0.3, 3}}}

// Chaos/Void
{{500, {0.0, 0.0, 0.0, 0}}, {1200, {0.3, 0.0, 0.5, 4}}}
```

### Chemical Reactions

```cpp
// Copper sulfate (blue-green flame)
{{800, {0.0, 0.5, 0.3, 2}}, {1200, {0.2, 0.8, 0.6, 6}}}

// Sodium (yellow)
{{800, {1.0, 0.9, 0.0, 3}}, {1200, {1.0, 1.0, 0.5, 8}}}

// Barium (pale green)
{{800, {0.5, 0.8, 0.4, 2}}, {1200, {0.7, 1.0, 0.6, 6}}}

// Potassium (pink-purple)
{{800, {0.8, 0.2, 0.5, 2}}, {1200, {1.0, 0.5, 0.8, 6}}}
```

## Example: Complete Magic System

```cpp
class MagicSpellSystem {
public:
    void cast_fire_spell(World& world, Entity caster, SpellType type) {
        Entity spell = world.create_entity();

        switch (type) {
            case SpellType::Fireball: {
                // Standard orange fire
                auto explosion = ExplosionComponent::create_magic_explosion(
                    {1.0f, 0.4f, 0.0f, 1.0f}, 8.0f
                );
                world.add_component(spell, explosion);
                break;
            }

            case SpellType::Frostbolt: {
                // Ice explosion (blue-white)
                auto explosion = ExplosionComponent::create_magic_explosion(
                    {0.7f, 0.9f, 1.0f, 1.0f}, 6.0f
                );
                explosion.fireball_temperature_k = 200.0f; // Cold!
                world.add_component(spell, explosion);
                break;
            }

            case SpellType::ArcaneBlast: {
                // Purple magic explosion
                auto explosion = ExplosionComponent::create_magic_explosion(
                    {0.6f, 0.0f, 1.0f, 1.0f}, 10.0f
                );
                world.add_component(spell, explosion);
                break;
            }

            case SpellType::PoisonCloud: {
                // Green toxic smoke
                auto smoke = VolumetricSmokeComponent();
                smoke.base_color = {0.2f, 0.8f, 0.1f};
                smoke.opacity = 0.9f;
                smoke.source_radius_m = 5.0f;
                world.add_component(spell, smoke);
                break;
            }
        }

        explosion_system.trigger(world, spell, caster_position);
    }
};
```

## Tips and Tricks

1. **Match flash to fireball**: `flash_color = fireball_color * 2.0`
2. **Smoke follows fire**: `smoke_color = fireball_color * 0.3`
3. **High emission = bright**: Use alpha channel > 5.0 for glowing effects
4. **Low temperature for cold**: Use < 500K for icy/cold effects
5. **Black "fire"**: Use very low RGB (0.05 or less) for shadow effects
6. **Rainbow effects**: 7+ gradient points with ROYGBIV
7. **Pulsing colors**: Animate gradient points over time
8. **Two-tone**: Use only 2 gradient points for stark contrast

---

*Volumetric Effects Color Customization - Lore Engine v1.0*