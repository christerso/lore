#include <lore/ecs/components/visibility_component.hpp>
#include <algorithm>

namespace lore::ecs {

bool VisibilityComponent::can_see_tile(const world::TileCoord& coord) const {
    auto it = std::find(visible_tiles.begin(), visible_tiles.end(), coord);
    return it != visible_tiles.end();
}

float VisibilityComponent::get_tile_visibility(const world::TileCoord& coord) const {
    auto it = std::find(visible_tiles.begin(), visible_tiles.end(), coord);
    if (it != visible_tiles.end()) {
        size_t index = std::distance(visible_tiles.begin(), it);
        return visibility_factors[index];
    }
    return 0.0f;
}

bool VisibilityComponent::can_see_entity(uint32_t entity_id) const {
    return visible_entities.find(entity_id) != visible_entities.end();
}

void VisibilityComponent::clear() {
    visible_tiles.clear();
    visibility_factors.clear();
    visible_entities.clear();
    is_valid = false;
}

} // namespace lore::ecs