# Line of Sight & Vision System Design

## Overview

A comprehensive 3D Line of Sight (LOS) and Field of View (FOV) system for the Lore engine. Designed as a **separate, reusable library** (`lore_vision`) with support for environmental factors, entity attributes, and GPU acceleration.

## Core Requirements

### 1. Environmental Factors
- **Time of Day**: Day/night cycle affects vision range and clarity
- **Weather**: Rain, fog, snow, dust storms reduce visibility
- **Lighting**: Ambient light, point lights, directional lights affect what's visible
- **Smoke/Gas**: Dynamic occlusion from particles and effects
- **Terrain**: Height differences, water, vegetation

### 2. Entity Attributes
Each entity has vision properties that modify LOS:
- **Base Vision Range**: How far entity can see in ideal conditions (meters)
- **Perception Skill**: Ability to detect details/hidden objects (0.0-1.0)
- **Night Vision**: Ability to see in darkness (0.0-1.0, 1.0 = full night vision)
- **Blind/Deaf/Other**: Status effects that modify senses
- **Eye Height**: Vertical position of eyes (affects what can be seen over obstacles)
- **FOV Angle**: Cone of vision (humans: ~210°, focused: ~60°)
- **Visual Acuity**: Detail at distance (affects identification range)

### 3. Occlusion Types
- **Full Occlusion**: Solid walls, closed doors
- **Partial Occlusion**: Windows, fences, foliage (reduce visibility but don't block)
- **Transparency**: Glass, water (distort but don't fully block)
- **Height-Based**: Low walls, furniture (block based on eye height)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    VisionSystem (High-Level)                 │
│  - Manages entity vision queries                            │
│  - Applies environmental modifiers                          │
│  - Integrates with weather/time systems                     │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │ LOS Core │    │ FOV Core │    │ GPU LOS  │
    │ (CPU)    │    │ (CPU)    │    │ (Compute)│
    └──────────┘    └──────────┘    └──────────┘
           │               │               │
           └───────────────┴───────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │Bresenham │    │ Shadow   │    │  Ray     │
    │3D Line   │    │ Casting  │    │ Casting  │
    └──────────┘    └──────────┘    └──────────┘
```

## Data Structures

### Entity Vision Profile

```cpp
namespace lore::vision {

// Entity's vision capabilities
struct VisionProfile {
    // Base attributes
    float base_range_meters = 50.0f;        // Vision range in ideal conditions
    float perception = 0.5f;                // Perception skill (0.0-1.0)
    float night_vision = 0.0f;              // Night vision ability (0.0-1.0)
    float fov_angle_degrees = 210.0f;       // Field of view cone angle
    float visual_acuity = 1.0f;             // Detail perception at distance
    float eye_height_meters = 1.7f;         // Height of eyes above ground

    // Status effects
    bool is_blind = false;                  // Cannot see
    bool has_thermal_vision = false;        // See through smoke/darkness
    bool has_xray_vision = false;           // See through walls (debug/special)

    // Targeting/focus
    float focused_fov_angle = 60.0f;        // Narrow FOV when aiming
    float focus_range_bonus = 1.5f;         // Range multiplier when focused
};

// Environmental conditions affecting vision
struct EnvironmentalConditions {
    // Time and weather
    float time_of_day = 0.5f;               // 0.0 = midnight, 0.5 = noon, 1.0 = midnight
    float ambient_light_level = 1.0f;       // 0.0 = pitch black, 1.0 = full daylight
    float fog_density = 0.0f;               // 0.0 = clear, 1.0 = opaque fog
    float rain_intensity = 0.0f;            // 0.0 = no rain, 1.0 = heavy rain
    float snow_intensity = 0.0f;            // 0.0 = no snow, 1.0 = blizzard
    float dust_density = 0.0f;              // Airborne particles

    // Dynamic occlusion
    float smoke_density = 0.0f;             // Smoke from fires/grenades
    float gas_density = 0.0f;               // Tear gas, etc.

    // Lighting sources (managed separately)
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;
    DirectionalLight sun_light;
};

// Tile occlusion properties (from TileDefinition)
struct TileOcclusion {
    bool blocks_sight = false;              // Full occlusion
    float transparency = 0.0f;              // 0.0 = opaque, 1.0 = transparent
    float height_meters = 0.0f;             // Height of obstacle
    bool is_window = false;                 // Special case for windows
    bool is_foliage = false;                // Vegetation (partial occlusion)
};

} // namespace lore::vision
```

### LOS Query Result

```cpp
namespace lore::vision {

// Result of LOS check between two points
struct LOSResult {
    bool has_line_of_sight = false;         // Can A see B?
    float effective_range = 0.0f;           // Distance after environmental modifiers
    float visibility_factor = 0.0f;         // 0.0 = invisible, 1.0 = fully visible

    // Occlusion details
    math::Vec3 occlusion_point;             // Where LOS was blocked (if blocked)
    TileCoord blocking_tile;                // Tile that blocked vision
    OcclusionType occlusion_type;           // Full, partial, height-based

    // Environmental effects
    float fog_reduction = 0.0f;             // Visibility lost to fog
    float darkness_reduction = 0.0f;        // Visibility lost to darkness
    float weather_reduction = 0.0f;         // Visibility lost to rain/snow
};

enum class OcclusionType {
    None,           // Clear line of sight
    FullBlock,      // Solid wall
    PartialBlock,   // Window, fence
    HeightBlock,    // Low wall (based on eye height)
    FoliageBlock,   // Vegetation
    Fog,            // Environmental
    Darkness,       // Lighting
    Weather         // Rain/snow
};

} // namespace lore::vision
```

## Algorithm Selection

### 3D Bresenham Line Algorithm
**Use Case**: Fast single-ray LOS checks (entity A → entity B)

**Advantages**:
- Very fast (integer only)
- No floating point errors
- Perfect for tile-based worlds

**Implementation**:
```cpp
bool has_line_of_sight(
    const TileCoord& from,
    const TileCoord& to,
    const TilemapWorldSystem& world,
    const VisionProfile& viewer,
    const EnvironmentalConditions& env
) {
    // 1. Bresenham 3D line from 'from' to 'to'
    // 2. For each tile along line:
    //    - Check TileOcclusion
    //    - Apply height-based checks
    //    - Accumulate transparency losses
    // 3. Apply environmental modifiers
    // 4. Return visibility result
}
```

### Shadow Casting FOV
**Use Case**: Calculate entire visible area for an entity (360° or cone)

**Advantages**:
- Efficient for large FOV
- Handles shadows naturally
- Good for AI awareness zones

**Algorithm**:
- Cast rays at regular angular intervals
- Track shadow regions cast by obstacles
- Mark all tiles in unshaded regions as visible

### Ray Casting (GPU)
**Use Case**: Massive parallel LOS checks (hundreds of entities)

**Advantages**:
- Extremely fast with GPU
- Can handle 1000+ simultaneous queries
- Perfect for RTS/strategy games

**Implementation**:
- Upload world occlusion data to GPU buffer
- Dispatch compute shader with entity pairs
- Read back results in batch

## Environmental Modifiers

### Time of Day
```cpp
float calculate_light_level(float time_of_day) {
    // 0.0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk, 1.0 = midnight

    if (time_of_day < 0.2f || time_of_day > 0.8f) {
        return 0.1f; // Night
    } else if (time_of_day < 0.3f || time_of_day > 0.7f) {
        return 0.5f; // Dawn/dusk
    } else {
        return 1.0f; // Day
    }
}
```

### Weather Effects
```cpp
float calculate_weather_visibility_modifier(const EnvironmentalConditions& env) {
    float modifier = 1.0f;

    // Fog: exponential falloff
    modifier *= (1.0f - env.fog_density * 0.8f);

    // Rain: linear reduction
    modifier *= (1.0f - env.rain_intensity * 0.3f);

    // Snow: moderate reduction
    modifier *= (1.0f - env.snow_intensity * 0.5f);

    // Dust/smoke: heavy reduction
    modifier *= (1.0f - env.dust_density * 0.7f);
    modifier *= (1.0f - env.smoke_density * 0.9f);

    return std::max(0.0f, modifier);
}
```

### Entity Abilities
```cpp
float calculate_entity_vision_range(
    const VisionProfile& profile,
    const EnvironmentalConditions& env)
{
    float base_range = profile.base_range_meters;

    // Night vision compensates for darkness
    float light_level = env.ambient_light_level;
    light_level = std::max(light_level, profile.night_vision);

    // Thermal vision ignores smoke/fog
    float env_modifier = 1.0f;
    if (!profile.has_thermal_vision) {
        env_modifier = calculate_weather_visibility_modifier(env);
    }

    // Blindness overrides everything
    if (profile.is_blind) {
        return 0.0f;
    }

    return base_range * light_level * env_modifier * profile.visual_acuity;
}
```

## API Design

### High-Level VisionSystem

```cpp
namespace lore::vision {

class VisionSystem {
public:
    VisionSystem(TilemapWorldSystem& world);

    // Single entity-to-entity LOS check
    LOSResult check_line_of_sight(
        ecs::EntityID viewer,
        ecs::EntityID target,
        const EnvironmentalConditions& env
    );

    // Calculate visible tiles for entity (FOV)
    std::vector<TileCoord> calculate_visible_tiles(
        ecs::EntityID viewer,
        const EnvironmentalConditions& env,
        float max_range_override = -1.0f
    );

    // Batch LOS checks (GPU accelerated if available)
    std::vector<LOSResult> batch_check_line_of_sight(
        const std::vector<std::pair<ecs::EntityID, ecs::EntityID>>& pairs,
        const EnvironmentalConditions& env
    );

    // Query: "Who can see this position?"
    std::vector<ecs::EntityID> get_observers_of_position(
        const math::Vec3& world_pos,
        const std::vector<ecs::EntityID>& potential_observers,
        const EnvironmentalConditions& env
    );

    // Environmental conditions management
    void set_time_of_day(float time); // 0.0-1.0
    void set_weather(float fog, float rain, float snow);
    void add_dynamic_occlusion(const math::Vec3& pos, float radius, float density);

private:
    TilemapWorldSystem& world_;
    EnvironmentalConditions current_env_;

    // Algorithm implementations
    std::unique_ptr<BresenhamLOS> bresenham_;
    std::unique_ptr<ShadowCastingFOV> shadow_casting_;
    std::unique_ptr<GPULOSCompute> gpu_los_;
};

} // namespace lore::vision
```

## Integration with ECS

Add VisionProfile component to entities:

```cpp
// In entity creation
entity_manager.add_component<vision::VisionProfile>(entity, {
    .base_range_meters = 50.0f,
    .perception = 0.7f,  // Skilled observer
    .night_vision = 0.3f, // Moderate night vision
    .fov_angle_degrees = 210.0f,
    .eye_height_meters = 1.75f
});
```

## Performance Considerations

### CPU Performance
- **Single LOS check**: ~5-20 microseconds (Bresenham)
- **FOV calculation**: ~0.5-2 milliseconds (Shadow Casting, 30m range)
- **Batch operations**: Use GPU for >100 simultaneous checks

### GPU Performance
- **1000 LOS checks**: ~0.5 milliseconds (Compute shader)
- **Bandwidth**: Minimize CPU↔GPU transfers
- **Occlusion data**: Upload once per chunk load

### Memory Usage
- **Per-entity**: ~64 bytes (VisionProfile)
- **Per-chunk**: ~4 KB (occlusion cache)
- **GPU buffers**: ~16 MB (for 1000 entities + world data)

## Testing Strategy

1. **Unit Tests**:
   - Bresenham correctness (known paths)
   - Environmental modifier math
   - Edge cases (same tile, adjacent tiles)

2. **Integration Tests**:
   - LOS through walls (should fail)
   - LOS through windows (partial visibility)
   - Height-based occlusion

3. **Performance Tests**:
   - Single LOS: <20μs
   - FOV 30m radius: <2ms
   - GPU batch 1000 checks: <1ms

4. **Visual Tests**:
   - Debug rendering of FOV
   - Comparison with player expectations

## Future Enhancements

1. **Sound Propagation**: Similar system for audio LOS
2. **Smell/Scent**: Tracking for tracking mechanics
3. **Predictive LOS**: Pre-compute for AI optimization
4. **Cached FOV**: Store entity FOV between frames
5. **Hierarchical Occlusion**: Chunk-level occlusion culling

## Implementation Plan

1. ✅ **Phase 1**: Design and documentation (this file)
2. **Phase 2**: Core data structures and API
3. **Phase 3**: Bresenham 3D LOS algorithm
4. **Phase 4**: Environmental modifiers
5. **Phase 5**: Shadow Casting FOV
6. **Phase 6**: GPU compute shader
7. **Phase 7**: Integration with tilemap system
8. **Phase 8**: Testing and optimization

## References

- [Red Blob Games: 2D Visibility](https://www.redblobgames.com/articles/visibility/)
- [RogueBasin: Field of Vision](http://www.roguebasin.com/index.php?title=Field_of_Vision)
- [3D Bresenham Line Algorithm](http://www.ict.griffith.edu.au/anthony/info/graphics/bresenham.procs)
- Tactical Games: XCOM, Jagged Alliance, Battle Brothers (reference implementations)