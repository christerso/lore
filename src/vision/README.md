# Lore Vision Library

A standalone, high-performance 3D Line of Sight (LOS) and Field of View (FOV) library for tile-based games.

## Features

- **3D Bresenham Line Algorithm**: Fast integer-only line tracing through 3D tile grids
- **Environmental Factors**: Time of day, weather (fog, rain, snow), lighting
- **Entity Vision Attributes**: Range, perception, night vision, thermal vision, FOV angle, eye height
- **Tile Occlusion**: Full/partial blocking, transparency, height-based occlusion, foliage
- **Standalone**: Minimal dependencies, works with any tile-based world system
- **Interface-based**: Implement `IVisionWorld` to integrate with your world representation

## Quick Start

### 1. Implement the World Interface

```cpp
#include <lore/vision/vision_world_interface.hpp>

class MyTileWorld : public lore::vision::IVisionWorld {
public:
    const lore::vision::TileVisionData* get_tile_vision_data(
        const lore::vision::TileCoord& coord) const override
    {
        // Return vision properties for tile at coord
        // Return nullptr for air/empty space
    }

    lore::vision::TileCoord world_to_tile(
        const lore::math::Vec3& world_pos) const override
    {
        return {
            static_cast<int32_t>(std::floor(world_pos.x)),
            static_cast<int32_t>(std::floor(world_pos.y)),
            static_cast<int32_t>(std::floor(world_pos.z))
        };
    }

    lore::math::Vec3 tile_to_world(
        const lore::vision::TileCoord& tile) const override
    {
        return lore::math::Vec3(
            static_cast<float>(tile.x) + 0.5f,
            static_cast<float>(tile.y) + 0.5f,
            static_cast<float>(tile.z) + 0.5f
        );
    }
};
```

### 2. Define Entity Vision

```cpp
#include <lore/vision/vision_profile.hpp>

lore::vision::VisionProfile human_soldier{
    .base_range_meters = 50.0f,      // Can see 50m in ideal conditions
    .fov_angle_degrees = 210.0f,     // Human peripheral vision
    .eye_height_meters = 1.7f,       // Average human eye height
    .perception = 0.7f,              // Trained observer
    .night_vision = 0.2f,            // Limited night vision
    .visual_acuity = 1.0f            // 20/20 vision
};
```

### 3. Set Environmental Conditions

```cpp
#include <lore/vision/vision_profile.hpp>

lore::vision::EnvironmentalConditions foggy_dawn{
    .time_of_day = 0.25f,            // Dawn (6 AM)
    .ambient_light_level = 0.5f,     // Dim morning light
    .fog_density = 0.6f,             // Heavy fog
    .rain_intensity = 0.2f           // Light rain
};
```

### 4. Check Line of Sight

```cpp
#include <lore/vision/bresenham_3d.hpp>

MyTileWorld world;
lore::math::Vec3 viewer_pos(10.0f, 10.0f, 0.0f);
lore::math::Vec3 target_pos(30.0f, 30.0f, 0.0f);

auto result = lore::vision::check_line_of_sight(
    viewer_pos,
    target_pos,
    human_soldier,
    foggy_dawn,
    world
);

if (result.result == lore::vision::LOSResult::Clear) {
    // Target is visible!
    float visibility = result.visibility_factor; // 0.0-1.0
}
```

## API Reference

### Core Functions

#### `check_line_of_sight()`
High-level LOS check between two world positions.

```cpp
DetailedLOSResult check_line_of_sight(
    const math::Vec3& from_world_pos,
    const math::Vec3& to_world_pos,
    const VisionProfile& viewer_profile,
    const EnvironmentalConditions& env,
    const IVisionWorld& world,
    bool is_focused = false
);
```

**Returns**:
- `LOSResult::Clear` - Clear line of sight
- `LOSResult::Blocked` - Fully blocked by obstacle
- `LOSResult::PartiallyVisible` - Visible through window/foliage
- `LOSResult::TooFar` - Beyond effective vision range
- `LOSResult::OutOfFOV` - Outside field of view cone

#### `is_in_field_of_view()`
Check if target is within viewer's FOV cone.

```cpp
bool is_in_field_of_view(
    const math::Vec3& viewer_pos,
    const math::Vec3& viewer_forward,
    const math::Vec3& target_pos,
    float fov_angle_degrees
);
```

### Low-Level Bresenham API

#### `Bresenham3D::trace_line()`
Trace line and call visitor for each tile.

```cpp
void trace_line(
    const TileCoord& start,
    const TileCoord& end,
    TileVisitor visitor // bool(const TileCoord&, float distance)
);
```

#### `Bresenham3D::get_line_tiles()`
Get all tiles along a line.

```cpp
std::vector<TileCoord> get_line_tiles(
    const TileCoord& start,
    const TileCoord& end
);
```

## Environmental Modifiers

### Time of Day
- `0.0-0.2` = Night (0.1 light level)
- `0.2-0.3` = Dawn (0.5 light level)
- `0.3-0.7` = Day (1.0 light level)
- `0.7-0.8` = Dusk (0.5 light level)
- `0.8-1.0` = Night (0.1 light level)

### Weather Effects
| Effect | Visibility Reduction |
|--------|---------------------|
| Fog | 80% (0.8 factor) |
| Rain | 30% (0.3 factor) |
| Snow | 50% (0.5 factor) |
| Dust/Sand | 70% (0.7 factor) |
| Smoke | 90% (0.9 factor) |

### Special Vision Modes
- **Thermal Vision**: Ignores fog, smoke, and weather
- **X-Ray Vision**: See through walls (debug/special ability)
- **Night Vision**: Compensates for darkness

## Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Single LOS check | ~5-20 μs | Bresenham 3D |
| 50m distance | ~10 μs | Typical combat range |
| Through 10 tiles | ~15 μs | With occlusion checks |

**Optimizations**:
- Integer-only arithmetic (Bresenham)
- Early termination on occlusion
- No floating point in line tracing
- Minimal allocations

## Integration Examples

### With Lore Engine TilemapWorldSystem

```cpp
// Adapter class
class TilemapVisionAdapter : public lore::vision::IVisionWorld {
    lore::world::TilemapWorldSystem& tilemap_;

public:
    TilemapVisionAdapter(lore::world::TilemapWorldSystem& tilemap)
        : tilemap_(tilemap) {}

    const lore::vision::TileVisionData* get_tile_vision_data(
        const lore::vision::TileCoord& coord) const override
    {
        // Convert vision::TileCoord to world::TileCoord
        lore::world::TileCoord world_coord{coord.x, coord.y, coord.z};

        const auto* tile_instance = tilemap_.get_tile(world_coord);
        if (!tile_instance) return nullptr;

        const auto* def = tilemap_.get_tile_definition(tile_instance->definition_id);
        if (!def) return nullptr;

        // Convert TileDefinition to TileVisionData
        static lore::vision::TileVisionData vision_data;
        vision_data.blocks_sight = def->blocks_sight;
        vision_data.transparency = 1.0f; // TODO: Add to TileDefinition
        vision_data.height_meters = 1.0f; // TODO: Add to TileDefinition
        vision_data.is_foliage = false; // TODO: Add to TileDefinition

        return &vision_data;
    }

    // ... implement other methods
};
```

### With ECS

```cpp
// VisionProfile as ECS component
struct VisionComponent {
    lore::vision::VisionProfile profile;
};

// System to update entity visibility
void VisionSystem::update() {
    auto viewer_entities = ecs_.get_entities_with<VisionComponent, Transform>();
    auto target_entities = ecs_.get_entities_with<Transform>();

    for (auto viewer_id : viewer_entities) {
        auto& vision = ecs_.get_component<VisionComponent>(viewer_id);
        auto& viewer_transform = ecs_.get_component<Transform>(viewer_id);

        for (auto target_id : target_entities) {
            if (viewer_id == target_id) continue;

            auto& target_transform = ecs_.get_component<Transform>(target_id);

            auto result = lore::vision::check_line_of_sight(
                viewer_transform.position,
                target_transform.position,
                vision.profile,
                current_environment_,
                *world_adapter_
            );

            // Update visibility state
            if (result.result == lore::vision::LOSResult::Clear) {
                // Target is visible to viewer
            }
        }
    }
}
```

## Future Enhancements

Planned features (not yet implemented):
- Shadow Casting FOV algorithm for 360° visible area calculation
- GPU compute shader for massive parallel LOS checks (1000+ simultaneous)
- Sound propagation (similar to vision but for audio)
- Cached FOV for performance optimization
- Visual debug rendering

## Dependencies

- `lore_math`: Vec3, basic math operations only
- C++20 or later

## License

Part of the Lore Engine project.

## Contributing

This is a standalone library that can be extracted and used in other projects. Contributions welcome!