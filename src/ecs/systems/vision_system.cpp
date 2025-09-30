#include <lore/ecs/systems/vision_system.hpp>
#include <lore/ecs/ecs.hpp>
#include <lore/ecs/components/vision_component.hpp>
#include <lore/ecs/components/visibility_component.hpp>
#include <lore/input/input_ecs.hpp> // For TransformComponent
#include <lore/vision/bresenham_3d.hpp> // For DetailedLOSResult and check_line_of_sight
#include <lore/core/log.hpp>

namespace lore::ecs {

VisionSystem::VisionSystem(world::TilemapWorldSystem& world)
    : world_(world)
    , vision_adapter_(std::make_unique<world::TilemapVisionAdapter>(world))
{
    // Initialize default environmental conditions (daytime, clear weather)
    env_conditions_.time_of_day = 0.5f; // Noon
    env_conditions_.ambient_light_level = 1.0f;
    env_conditions_.fog_density = 0.0f;
    env_conditions_.rain_intensity = 0.0f;
    env_conditions_.snow_intensity = 0.0f;
    env_conditions_.dust_density = 0.0f;
    env_conditions_.smoke_density = 0.0f;
    env_conditions_.gas_density = 0.0f;

    LOG_INFO(Game, "VisionSystem initialized");
}

void VisionSystem::init(World& world) {
    (void)world; // Suppress unused parameter warning
    LOG_INFO(Game, "VisionSystem::init called");
}

void VisionSystem::shutdown(World& world) {
    (void)world; // Suppress unused parameter warning
    LOG_INFO(Game, "VisionSystem::shutdown called");
}

void VisionSystem::update(World& world, float delta_time) {
    if (!enabled_) {
        return;
    }

    // Get component arrays for entities with vision
    auto& vision_array = world.get_component_array<VisionComponent>();
    auto& visibility_array = world.get_component_array<VisibilityComponent>();
    auto& transform_array = world.get_component_array<input::TransformComponent>();

    const Entity* vision_entities = vision_array.entities();
    const std::size_t vision_count = vision_array.size();

    // Iterate all entities with VisionComponent
    for (std::size_t i = 0; i < vision_count; ++i) {
        Entity entity_id = vision_entities[i];

        // Check if entity also has VisibilityComponent and TransformComponent
        if (!visibility_array.has_component(entity_id) || !transform_array.has_component(entity_id)) {
            continue;
        }

        auto& vision = vision_array.get_component(entity_id);
        auto& visibility = visibility_array.get_component(entity_id);
        const auto& transform = transform_array.get_component(entity_id);

        if (!vision.enabled) {
            continue;
        }

        update_entity_vision(world, entity_id, transform.position, vision, visibility);

        visibility.last_update_time += delta_time;
    }
}

void VisionSystem::set_environment(const vision::EnvironmentalConditions& conditions) {
    env_conditions_ = conditions;
    LOG_DEBUG(Game, "Environmental conditions updated: time={}, light={}, fog={}",
             conditions.time_of_day,
             conditions.ambient_light_level,
             conditions.fog_density);
}

void VisionSystem::update_entity_vision(
    World& world,
    Entity entity_id,
    const math::Vec3& position,
    VisionComponent& vision,
    VisibilityComponent& visibility)
{
    // Calculate eye position
    math::Vec3 eye_pos = position;
    eye_pos.y += vision.eye_height;

    // Calculate field of view using shadow casting algorithm
    vision::FOVResult fov_result = vision::ShadowCastingFOV::calculate_fov(
        eye_pos,
        vision.profile,
        env_conditions_,
        *vision_adapter_,
        vision.is_focused
    );

    // Update visibility component - convert vision::TileCoord to world::TileCoord
    visibility.visible_tiles.clear();
    visibility.visible_tiles.reserve(fov_result.visible_tiles.size());
    for (const auto& tile : fov_result.visible_tiles) {
        visibility.visible_tiles.push_back(world::TileCoord{tile.x, tile.y, tile.z});
    }
    visibility.visibility_factors = std::move(fov_result.visibility_factors);
    visibility.is_valid = true;

    // Find visible entities
    update_visible_entities(world, entity_id, eye_pos, vision.profile, visibility);
}

void VisionSystem::update_visible_entities(
    World& world,
    Entity observer_entity_id,
    const math::Vec3& observer_position,
    const vision::VisionProfile& observer_profile,
    VisibilityComponent& visibility)
{
    visibility.visible_entities.clear();

    // Get all entities with Transform (potential visibility targets)
    auto& transform_array = world.get_component_array<input::TransformComponent>();
    const Entity* entities = transform_array.entities();
    const std::size_t entity_count = transform_array.size();

    for (std::size_t i = 0; i < entity_count; ++i) {
        Entity target_entity_id = entities[i];

        // Don't check visibility to self
        if (target_entity_id == observer_entity_id) {
            continue;
        }

        const auto& target_transform = transform_array.get_component(target_entity_id);

        // Check if target position is within visible tiles
        world::TileCoord target_tile = world_.world_to_tile(target_transform.position);

        if (visibility.can_see_tile(target_tile)) {
            // Additional check: verify line of sight not blocked
            vision::DetailedLOSResult los_result = vision::check_line_of_sight(
                observer_position,
                target_transform.position,
                observer_profile,
                env_conditions_,
                *vision_adapter_,
                false // not focused for entity checks
            );

            if (los_result.result == vision::LOSResult::Clear ||
                los_result.result == vision::LOSResult::PartiallyVisible)
            {
                visibility.visible_entities.insert(target_entity_id);
            }
        }
    }
}

} // namespace lore::ecs