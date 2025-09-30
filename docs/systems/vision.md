# Vision and Visibility System

## Overview

The Vision and Visibility System provides a complete field-of-view (FOV) and line-of-sight solution for the Lore engine. It enables entities to perceive their surroundings realistically, with support for environmental conditions, AI behaviors, and dynamic occlusion.

### Key Features

- **Shadow Casting FOV Algorithm**: Efficient 8-octant shadow casting with proper occlusion handling
- **Environmental Modifiers**: Darkness, fog, rain, smoke, and other conditions affect visibility
- **Tile-Based Occlusion**: Walls, windows, foliage, and obstacles with height-based blocking
- **AI Vision Behaviors**: Detection, pursuit, fleeing, investigation, and state-driven decision making
- **Target Classification**: Prey, predator, neutral, and ally classification for AI behaviors
- **Customizable Profiles**: Presets for players, guards, creatures, and custom configurations
- **Performance Optimized**: Efficient visibility caching and per-entity enable/disable flags

### Architecture

The system consists of two layers:

1. **Vision Library** (`lore/vision/`): Standalone FOV calculation library
   - `ShadowCastingFOV`: Core FOV algorithm implementation
   - `VisionProfile`: Entity vision capabilities (range, FOV, perception, night vision)
   - `EnvironmentalConditions`: Weather, lighting, smoke effects
   - `IVisionWorld`: Interface for tilemap integration

2. **ECS Integration** (`lore/ecs/`): Game-ready components and systems
   - `VisionComponent`: Attach to entities that can see
   - `VisibilityComponent`: Stores what entity currently sees (tiles and entities)
   - `AIVisionBehaviorComponent`: AI state machine driven by vision
   - `VisionSystem`: Updates entity FOV each frame
   - `AIVisionSystem`: Processes AI behaviors based on visibility

---

## Quick Start

### Adding Vision to an Entity

```cpp
#include <lore/ecs/components/vision_component.hpp>
#include <lore/ecs/components/visibility_component.hpp>

// Create entity that can see
Entity player = world.create_entity();
world.add_component(player, VisionComponent::create_player());
world.add_component(player, VisibilityComponent{});

// Entity will automatically have FOV calculated by VisionSystem
```

### Checking What an Entity Can See

```cpp
auto* visibility = world.get_component<VisibilityComponent>(entity_id);
if (visibility && visibility->is_valid) {
    // Check if entity can see a specific tile
    world::TileCoord target_tile = {10, 5, 0};
    if (visibility->can_see_tile(target_tile)) {
        float visibility_factor = visibility->get_tile_visibility(target_tile);
        // visibility_factor ranges from 0.0 (barely visible) to 1.0 (fully visible)
    }

    // Check if entity can see another entity
    if (visibility->can_see_entity(other_entity_id)) {
        // Target is visible!
    }

    // Get all visible tiles
    for (const auto& tile : visibility->visible_tiles) {
        // Process visible tile
    }
}
```

### Adding AI Vision Behavior

```cpp
#include <lore/ecs/components/ai_vision_behavior_component.hpp>

// Create guard that reacts to what it sees
Entity guard = world.create_entity();
world.add_component(guard, VisionComponent::create_guard());
world.add_component(guard, VisibilityComponent{});
world.add_component(guard, AIVisionBehaviorComponent::create_guard());

// AIVisionSystem will automatically update guard's behavior based on visibility
```

---

## Component Reference

### VisionComponent

Defines an entity's vision capabilities. Attach to any entity that needs to see (players, NPCs, creatures).

#### Factory Methods

```cpp
// Default humanoid vision (20m range, 120° FOV)
auto vision = VisionComponent::create_default();

// Player vision (50m range, 110° FOV, better perception)
auto vision = VisionComponent::create_player();

// Guard vision (30m range, 140° FOV, trained observer)
auto vision = VisionComponent::create_guard();

// Custom creature vision
auto vision = VisionComponent::create_creature(40.0f, 180.0f);
```

#### Vision Profile Configuration

```cpp
VisionComponent vision;

// Core vision attributes
vision.profile.base_range_meters = 50.0f;      // Maximum vision distance
vision.profile.fov_angle_degrees = 110.0f;     // Field of view cone
vision.profile.eye_height_meters = 1.7f;       // Eye height above ground

// Vision quality modifiers
vision.profile.perception = 0.7f;              // Detail perception (0.0-1.0)
vision.profile.night_vision = 0.2f;            // Low-light vision (0.0-1.0)
vision.profile.visual_acuity = 1.0f;           // Clarity at distance (1.0 = 20/20)

// Special vision types
vision.profile.has_thermal_vision = false;     // See heat signatures
vision.profile.has_xray_vision = false;        // See through walls (debug)

// Status effects
vision.profile.is_blind = false;               // Cannot see at all
vision.profile.is_dazed = false;               // Reduced range and clarity

// Focused/aiming mode
vision.profile.focused_fov_angle = 60.0f;      // Narrow FOV when focused
vision.profile.focus_range_bonus = 1.5f;       // Range multiplier when aiming

// Entity-specific vision state
vision.look_direction = {0.0f, 0.0f, -1.0f};   // Forward vector
vision.is_focused = false;                     // Aiming/using binoculars
vision.enabled = true;                         // Vision active (optimization)
```

#### Example: Night Vision Goggles

```cpp
VisionComponent vision = VisionComponent::create_player();

// Apply night vision goggles effect
vision.profile.night_vision = 0.9f;           // Almost perfect night vision
vision.profile.base_range_meters = 30.0f;     // Limited range
vision.profile.fov_angle_degrees = 90.0f;     // Narrower FOV
```

#### Example: Focused Aiming

```cpp
auto* vision = world.get_component<VisionComponent>(player);
if (is_aiming) {
    vision->is_focused = true;
    // FOV narrows to focused_fov_angle (60°)
    // Range increases by focus_range_bonus (1.5x = 75m)
}
```

---

### VisibilityComponent

Stores what an entity can currently see. Updated automatically by `VisionSystem` each frame.

#### Reading Visibility Data

```cpp
auto* visibility = world.get_component<VisibilityComponent>(entity_id);

// Check if visibility data is ready
if (!visibility->is_valid) {
    // Wait for VisionSystem to calculate first frame
    return;
}

// Query tile visibility
world::TileCoord target = {x, y, z};
if (visibility->can_see_tile(target)) {
    float factor = visibility->get_tile_visibility(target);
    // factor = 0.0-1.0 based on distance, lighting, occlusion
}

// Query entity visibility
if (visibility->can_see_entity(other_entity_id)) {
    // Entity is in FOV and not occluded
}

// Iterate all visible tiles
for (size_t i = 0; i < visibility->visible_tiles.size(); ++i) {
    const auto& tile = visibility->visible_tiles[i];
    float factor = visibility->visibility_factors[i];

    // Apply fog of war, render visible tiles, etc.
}

// Get counts
size_t tile_count = visibility->visible_tile_count();
size_t entity_count = visibility->visible_entity_count();

// Check when data was last updated
float age = current_time - visibility->last_update_time;
```

#### Visibility Factors

The `visibility_factors` array contains values from 0.0 to 1.0 for each visible tile:

- **1.0**: Fully visible (close, well-lit, no obstruction)
- **0.7**: Moderately visible (medium distance, some fog)
- **0.3**: Barely visible (far away, heavy fog, partial occlusion)
- **0.0**: Not visible (not in FOV or fully occluded)

Use these factors for:
- Fog of war rendering (alpha blending)
- AI detection probability
- Targeting accuracy penalties

---

### AIVisionBehaviorComponent

AI state machine that drives entity behavior based on what it can see. Requires `VisionComponent` and `VisibilityComponent`.

#### Factory Methods

```cpp
// Neutral NPC (basic detection, can pursue and flee)
auto ai = AIVisionBehaviorComponent::create_default();

// Guard/patrol NPC (long pursuit, no fleeing, investigates)
auto ai = AIVisionBehaviorComponent::create_guard();

// Prey animal (no pursuit, flees from predators)
auto ai = AIVisionBehaviorComponent::create_prey();

// Predator (long pursuit, keen senses, hunts prey)
auto ai = AIVisionBehaviorComponent::create_predator();
```

#### Behavior States

```cpp
enum class State {
    Idle,           // No targets visible, patrolling or idle
    Alert,          // Target detected but not yet pursuing
    Pursuing,       // Actively chasing visible target
    Fleeing,        // Running from visible threat
    Investigating,  // Searching last known position
    Hiding          // Taking cover from threat
};

// Check entity state
auto* ai = world.get_component<AIVisionBehaviorComponent>(entity);
if (ai->is_in_state(AIVisionBehaviorComponent::State::Pursuing)) {
    // Entity is chasing a target
}

// Get time in current state
float state_duration = ai->time_in_state(current_time);
```

#### Target Classification

```cpp
enum class TargetType {
    None,       // No target
    Prey,       // Something to hunt/chase
    Predator,   // Something to flee from
    Neutral,    // Observe but don't react
    Ally        // Friendly entity
};

// Custom target classification
ai.classify_target = [](Entity target_entity) -> TargetType {
    // Your custom logic here
    auto* tag = world.get_component<TagComponent>(target_entity);
    if (tag->is_hostile) return TargetType::Predator;
    if (tag->is_friendly) return TargetType::Ally;
    return TargetType::Neutral;
};
```

#### Configuration Options

```cpp
AIVisionBehaviorComponent ai;

// Detection ranges
ai.config.detection_distance = 30.0f;       // Max distance to detect targets
ai.config.alert_distance = 20.0f;           // Distance to become alert
ai.config.pursuit_distance = 50.0f;         // Max pursuit distance
ai.config.flee_distance = 40.0f;            // How far to flee

// Timing
ai.config.investigation_time = 5.0f;        // How long to investigate
ai.config.alert_timeout = 3.0f;             // Alert duration after losing sight

// Capabilities
ai.config.can_pursue = true;                // Can chase targets
ai.config.can_flee = true;                  // Can run from threats
ai.config.can_investigate = true;           // Can search last known position

// Perception skill
ai.config.perception_multiplier = 1.5f;     // Detection ability modifier
```

#### Behavior Callbacks

```cpp
// State change notification
ai.on_state_changed = [](State old_state, State new_state) {
    if (new_state == State::Alert) {
        // Play alert sound, animation, etc.
    }
};

// Target detection
ai.on_target_detected = [](Entity target_entity) {
    // Trigger alert, notify allies, etc.
};

// Target lost
ai.on_target_lost = [](Entity target_entity) {
    // Begin investigation, return to patrol, etc.
};

// Custom target classification
ai.classify_target = [](Entity target_entity) -> TargetType {
    // Custom faction/relationship logic
    return TargetType::Predator;
};
```

#### Reading Target Information

```cpp
auto* ai = world.get_component<AIVisionBehaviorComponent>(entity);

if (ai->has_target()) {
    Entity target_id = ai->current_target.entity_id;
    TargetType type = ai->current_target.type;
    math::Vec3 last_pos = ai->current_target.last_known_position;
    float last_seen = ai->current_target.last_seen_time;
    float threat = ai->current_target.threat_level;  // 0.0-1.0

    // Use target info for AI decision making
}

// Get investigation point
if (ai->is_in_state(State::Investigating)) {
    math::Vec3 search_point = ai->investigation_point;
    // Move entity toward search point
}
```

---

## System Reference

### VisionSystem

Updates entity visibility each frame. Call `update()` in your game loop.

#### Initialization

```cpp
#include <lore/ecs/systems/vision_system.hpp>

// Create vision system with reference to tilemap world
VisionSystem vision_system(tilemap_world);

// Register with ECS
world.register_system(&vision_system);
```

#### Environmental Conditions

```cpp
// Set environmental conditions affecting all entities
vision::EnvironmentalConditions env;

// Time and lighting
env.time_of_day = 0.1f;           // 0.0=midnight, 0.5=noon, 1.0=midnight
env.ambient_light_level = 0.3f;   // 0.0=pitch black, 1.0=bright daylight

// Weather
env.fog_density = 0.5f;           // 0.0=clear, 1.0=opaque fog
env.rain_intensity = 0.3f;        // 0.0=no rain, 1.0=heavy downpour
env.snow_intensity = 0.0f;        // 0.0=clear, 1.0=blizzard
env.dust_density = 0.0f;          // Sandstorm, etc.

// Local effects
env.smoke_density = 0.7f;         // Smoke grenades, fires
env.gas_density = 0.0f;           // Tear gas, chemical weapons

vision_system.set_environment(env);
```

#### Runtime Control

```cpp
// Disable vision system for performance (e.g., during cutscenes)
vision_system.set_enabled(false);

// Re-enable vision system
vision_system.set_enabled(true);

// Check if enabled
if (vision_system.is_enabled()) {
    // System is active
}

// Get current environmental conditions
const auto& env = vision_system.get_environment();
```

#### Example: Day/Night Cycle

```cpp
void update_time_of_day(float delta_time) {
    vision::EnvironmentalConditions env = vision_system.get_environment();

    // Advance time (24-hour cycle over 20 minutes)
    env.time_of_day += delta_time / (20.0f * 60.0f);
    if (env.time_of_day > 1.0f) env.time_of_day -= 1.0f;

    // Automatic light level from time
    env.ambient_light_level = 1.0f;  // Auto-calculated from time_of_day

    vision_system.set_environment(env);
}
```

#### Example: Dynamic Smoke Effect

```cpp
void create_smoke_grenade(const math::Vec3& position) {
    vision::EnvironmentalConditions env = vision_system.get_environment();

    // Add smoke effect (gradually increase density)
    env.smoke_density = std::min(env.smoke_density + 0.5f, 1.0f);

    vision_system.set_environment(env);

    // Schedule smoke dissipation
    schedule_callback(5.0f, []() {
        auto env = vision_system.get_environment();
        env.smoke_density = std::max(env.smoke_density - 0.5f, 0.0f);
        vision_system.set_environment(env);
    });
}
```

---

### AIVisionSystem

Processes AI vision behaviors. Updates entity AI states based on visibility data.

#### Initialization

```cpp
#include <lore/ecs/systems/ai_vision_system.hpp>

// Create AI vision system
AIVisionSystem ai_vision_system;

// Register with ECS (should update AFTER VisionSystem)
world.register_system(&ai_vision_system);
```

#### Runtime Control

```cpp
// Disable AI vision processing
ai_vision_system.set_enabled(false);

// Enable debug visualization (console logging, debug draws)
ai_vision_system.set_debug_mode(true);

// Check state
if (ai_vision_system.is_debug_mode()) {
    // Debug mode active
}
```

---

## Integration Guide

### Adding Vision to Your Game

#### Step 1: Initialize Systems

```cpp
// In your game initialization
void Game::init() {
    // Create tilemap world system first
    tilemap_world = std::make_unique<world::TilemapWorldSystem>();

    // Create vision systems
    vision_system = std::make_unique<VisionSystem>(*tilemap_world);
    ai_vision_system = std::make_unique<AIVisionSystem>();

    // Register with ECS
    ecs_world.register_system(vision_system.get());
    ecs_world.register_system(ai_vision_system.get());
}
```

#### Step 2: Update Systems Each Frame

```cpp
void Game::update(float delta_time) {
    // Update world first
    tilemap_world->update(delta_time);

    // Update vision (calculates FOV for all entities)
    vision_system->update(ecs_world, delta_time);

    // Update AI behaviors (reacts to vision)
    ai_vision_system->update(ecs_world, delta_time);

    // Your other systems...
}
```

#### Step 3: Create Entities with Vision

```cpp
Entity create_player(const math::Vec3& position) {
    Entity player = world.create_entity();

    // Position
    world.add_component(player, TransformComponent{position});

    // Vision
    world.add_component(player, VisionComponent::create_player());
    world.add_component(player, VisibilityComponent{});

    return player;
}

Entity create_guard(const math::Vec3& position) {
    Entity guard = world.create_entity();

    // Position
    world.add_component(guard, TransformComponent{position});

    // Vision
    world.add_component(guard, VisionComponent::create_guard());
    world.add_component(guard, VisibilityComponent{});

    // AI behavior
    auto ai = AIVisionBehaviorComponent::create_guard();
    ai.faction_id = FACTION_ENEMY;
    world.add_component(guard, ai);

    return guard;
}
```

---

## Advanced Usage Examples

### Example 1: Fog of War Rendering

```cpp
void render_fog_of_war(Entity player) {
    auto* visibility = world.get_component<VisibilityComponent>(player);
    if (!visibility || !visibility->is_valid) return;

    // Clear fog of war for visible tiles
    for (size_t i = 0; i < visibility->visible_tiles.size(); ++i) {
        const auto& tile = visibility->visible_tiles[i];
        float factor = visibility->visibility_factors[i];

        // Set tile as explored
        fog_of_war.set_explored(tile);

        // Set current visibility
        fog_of_war.set_visibility(tile, factor);
    }

    // Render world with fog overlay
    render_world();
    fog_of_war.render();
}
```

### Example 2: Stealth Detection System

```cpp
float calculate_detection_chance(Entity observer, Entity target) {
    auto* obs_vis = world.get_component<VisibilityComponent>(observer);
    auto* obs_vision = world.get_component<VisionComponent>(observer);
    auto* target_transform = world.get_component<TransformComponent>(target);

    if (!obs_vis->can_see_entity(target)) {
        return 0.0f;  // Not visible at all
    }

    // Get visibility factor
    world::TileCoord target_tile = tilemap_world.world_to_tile(target_transform->position);
    float visibility = obs_vis->get_tile_visibility(target_tile);

    // Get distance factor
    float distance = glm::length(target_transform->position - obs_transform->position);
    float distance_factor = 1.0f - (distance / obs_vision->profile.base_range_meters);

    // Get target stealth modifier
    auto* stealth = world.get_component<StealthComponent>(target);
    float stealth_factor = stealth ? stealth->concealment : 1.0f;

    // Calculate final detection chance
    float detection = visibility * distance_factor * obs_vision->profile.perception;
    detection *= (1.0f / stealth_factor);

    return std::clamp(detection, 0.0f, 1.0f);
}
```

### Example 3: Dynamic AI Faction System

```cpp
// Set up faction-based target classification
void setup_faction_ai(Entity entity, int faction_id) {
    auto* ai = world.get_component<AIVisionBehaviorComponent>(entity);
    ai->faction_id = faction_id;

    // Custom faction-based classification
    ai->classify_target = [faction_id](Entity target) -> TargetType {
        auto* target_faction = world.get_component<FactionComponent>(target);
        if (!target_faction) return TargetType::Neutral;

        int target_id = target_faction->faction_id;

        // Check faction relationships
        FactionRelation relation = faction_system.get_relation(faction_id, target_id);

        switch (relation) {
            case FactionRelation::Hostile:
                return TargetType::Predator;
            case FactionRelation::Friendly:
                return TargetType::Ally;
            case FactionRelation::Neutral:
                return TargetType::Neutral;
        }

        return TargetType::Neutral;
    };
}
```

### Example 4: Shared Vision for Allied Units

```cpp
void update_shared_vision(const std::vector<Entity>& allied_units) {
    // Collect all visible tiles from all allies
    std::unordered_set<world::TileCoord> shared_visible_tiles;

    for (Entity ally : allied_units) {
        auto* visibility = world.get_component<VisibilityComponent>(ally);
        if (!visibility || !visibility->is_valid) continue;

        for (const auto& tile : visibility->visible_tiles) {
            shared_visible_tiles.insert(tile);
        }
    }

    // Update shared vision data for UI/minimap
    shared_vision_system.set_visible_tiles(shared_visible_tiles);
}
```

### Example 5: Alert System with Propagation

```cpp
void setup_guard_alert_system(Entity guard) {
    auto* ai = world.get_component<AIVisionBehaviorComponent>(guard);

    // When guard detects target, alert nearby allies
    ai->on_target_detected = [guard](Entity target) {
        auto* transform = world.get_component<TransformComponent>(guard);

        // Find nearby allied guards
        std::vector<Entity> nearby_allies =
            spatial_query.find_entities_in_radius(transform->position, 30.0f);

        for (Entity ally : nearby_allies) {
            auto* ally_ai = world.get_component<AIVisionBehaviorComponent>(ally);
            if (!ally_ai) continue;

            // Share target information
            ally_ai->current_target.entity_id = target;
            ally_ai->current_target.last_known_position =
                world.get_component<TransformComponent>(target)->position;
            ally_ai->current_target.last_seen_time = current_time;

            // Force alert state
            ally_ai->state = AIVisionBehaviorComponent::State::Alert;
        }

        // Play alert sound
        audio_system.play_sound("guard_alert", transform->position);
    };
}
```

---

## Tile Configuration

### Setting Tile Vision Properties

```cpp
// Configure tile definitions in your tilemap
TileDefinition wall_tile;
wall_tile.blocks_sight = true;
wall_tile.transparency = 0.0f;
wall_tile.height_meters = 3.0f;

TileDefinition window_tile;
window_tile.blocks_sight = false;
window_tile.transparency = 0.8f;  // Can see through but reduces visibility
window_tile.height_meters = 2.0f;

TileDefinition bush_tile;
bush_tile.blocks_sight = false;
bush_tile.transparency = 0.3f;
bush_tile.height_meters = 1.5f;
bush_tile.is_foliage = true;  // Partial concealment

TileDefinition door_tile;
door_tile.blocks_sight = true;  // Closed door
door_tile.transparency = 0.0f;
door_tile.height_meters = 2.5f;

// When door opens
door_tile.blocks_sight = false;
door_tile.transparency = 1.0f;  // Open door allows full vision
```

---

## Performance Considerations

### Optimization Strategies

1. **Disable Vision for Distant Entities**
   ```cpp
   void optimize_vision_system() {
       // Disable vision for entities far from player
       auto* player_transform = world.get_component<TransformComponent>(player);

       world.for_each<VisionComponent, TransformComponent>(
           [&](Entity e, VisionComponent& vision, TransformComponent& transform) {
               float distance = glm::length(transform.position - player_transform->position);
               vision.enabled = (distance < 100.0f);  // Only update nearby entities
           }
       );
   }
   ```

2. **Reduce Update Frequency for NPCs**
   ```cpp
   // Update NPC vision every N frames
   int frame_counter = 0;

   void update_npc_vision() {
       frame_counter++;

       world.for_each<VisionComponent, NPCComponent>(
           [&](Entity e, VisionComponent& vision, NPCComponent& npc) {
               // Stagger updates based on entity ID
               if ((frame_counter + e) % 4 == 0) {
                   vision.enabled = true;  // Update this frame
               } else {
                   vision.enabled = false;  // Skip this frame
               }
           }
       );
   }
   ```

3. **Limit FOV Range for Performance**
   ```cpp
   // Reduce vision range based on performance budget
   if (performance_monitor.get_fps() < 30.0f) {
       world.for_each<VisionComponent>([](Entity e, VisionComponent& vision) {
           vision.profile.base_range_meters *= 0.8f;  // Reduce by 20%
       });
   }
   ```

### Typical Performance Characteristics

- **Vision System Update**: ~0.5-2ms for 100 entities (depends on FOV range)
- **AI Vision System Update**: ~0.2-0.5ms for 50 AI entities
- **Memory per Entity**: ~4KB (VisionComponent + VisibilityComponent)
- **Recommended Max Entities**: 200-500 entities with vision (depends on hardware)

---

## Troubleshooting

### Entity Cannot See Anything

**Check:**
1. Does entity have both `VisionComponent` and `VisibilityComponent`?
2. Is `VisionComponent::enabled` set to `true`?
3. Is `VisionSystem` registered and updating?
4. Is entity position valid (has `TransformComponent`)?
5. Check environmental conditions (is `ambient_light_level` near 0.0?)

```cpp
// Debug vision issue
auto* vision = world.get_component<VisionComponent>(entity);
auto* visibility = world.get_component<VisibilityComponent>(entity);

if (!vision) LOG_ERROR("Missing VisionComponent");
if (!visibility) LOG_ERROR("Missing VisibilityComponent");
if (vision && !vision->enabled) LOG_ERROR("Vision disabled");
if (vision && vision->profile.is_blind) LOG_ERROR("Entity is blind");
if (visibility && !visibility->is_valid) LOG_ERROR("Visibility not yet calculated");
```

### AI Not Reacting to Targets

**Check:**
1. Does entity have `AIVisionBehaviorComponent`?
2. Is `AIVisionSystem` registered and updating?
3. Is target classification working correctly?
4. Check detection distances in `Config`

```cpp
// Debug AI behavior
auto* ai = world.get_component<AIVisionBehaviorComponent>(entity);
auto* visibility = world.get_component<VisibilityComponent>(entity);

LOG_INFO("AI State: {}", (int)ai->state);
LOG_INFO("Visible Entities: {}", visibility->visible_entity_count());
LOG_INFO("Has Target: {}", ai->has_target());
LOG_INFO("Detection Distance: {}", ai->config.detection_distance);
```

### Vision Through Walls

**Check:**
1. Are tiles correctly marked with `blocks_sight = true`?
2. Is `TilemapVisionAdapter` correctly implementing `IVisionWorld`?
3. Check tile transparency values (should be 0.0 for solid walls)

```cpp
// Debug tile occlusion
world::TileCoord coord = {x, y, z};
const auto* tile_data = tilemap_world.get_tile_vision_data(coord);

if (tile_data) {
    LOG_INFO("Blocks Sight: {}", tile_data->blocks_sight);
    LOG_INFO("Transparency: {}", tile_data->transparency);
    LOG_INFO("Height: {}", tile_data->height_meters);
}
```

---

## Related Systems

- **World System** (`lore/world/`): Provides tilemap data for vision queries
- **Transform System**: Entity positions required for FOV calculation
- **Spatial Query System**: Useful for finding entities in radius
- **Faction System**: Can integrate with AI target classification
- **Stealth System**: Can use visibility factors for detection

---

## Future Enhancements

Potential improvements for the vision system:

1. **Audio-based Detection**: Integrate sound propagation with vision
2. **Smell/Scent Tracking**: Additional perception channel for creatures
3. **Memory System**: Entities remember previously seen areas
4. **Dynamic Lighting**: Integrate with lighting system for accurate light levels
5. **Occlusion Queries**: GPU-accelerated visibility for rendering
6. **Peripheral Vision**: Reduced detail/color in peripheral FOV
7. **Vision Cones**: Debug visualization of entity FOV

---

## References

- **Shadow Casting Algorithm**: Based on "Precise Permissive Field of View" by Jonathon Duerig
- **FOV Implementation**: `include/lore/vision/shadow_casting_fov.hpp`
- **World Integration**: `include/lore/vision/vision_world_interface.hpp`
- **Component Headers**: `include/lore/ecs/components/vision_*.hpp`
- **System Headers**: `include/lore/ecs/systems/*_vision_system.hpp`

---

## API Summary

### Quick Reference

```cpp
// Components
VisionComponent::create_player()
VisionComponent::create_guard()
VisionComponent::create_creature(range, fov)
AIVisionBehaviorComponent::create_guard()
AIVisionBehaviorComponent::create_prey()
AIVisionBehaviorComponent::create_predator()

// Visibility Queries
visibility.can_see_tile(coord)
visibility.get_tile_visibility(coord)
visibility.can_see_entity(entity_id)
visibility.visible_tile_count()
visibility.visible_entity_count()

// AI State Checks
ai.is_in_state(State::Pursuing)
ai.time_in_state(current_time)
ai.has_target()
ai.clear_target()

// System Control
vision_system.set_environment(env)
vision_system.set_enabled(false)
ai_vision_system.set_debug_mode(true)

// Environmental Conditions
env.time_of_day = 0.5f;           // Noon
env.ambient_light_level = 1.0f;   // Bright daylight
env.fog_density = 0.3f;           // Light fog
env.rain_intensity = 0.5f;        // Moderate rain
env.smoke_density = 0.8f;         // Heavy smoke
```