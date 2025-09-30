#pragma once

#include <lore/world/tilemap_world_system.hpp>
#include <vector>
#include <unordered_set>

namespace lore::ecs {

/**
 * @brief Component that stores what an entity can currently see
 *
 * Updated each frame by VisionSystem. Read by AI systems to determine
 * entity awareness and decision-making.
 */
struct VisibilityComponent {
    /// Tiles visible to this entity (from last FOV calculation)
    std::vector<world::TileCoord> visible_tiles;

    /// Visibility factor for each tile (0.0 = barely visible, 1.0 = fully visible)
    std::vector<float> visibility_factors;

    /// Entity IDs visible to this entity
    std::unordered_set<uint32_t> visible_entities;

    /// Whether visibility data is valid (false until first update)
    bool is_valid = false;

    /// Timestamp of last visibility update (for aging data)
    float last_update_time = 0.0f;

    /**
     * @brief Check if a specific tile is visible
     * @param coord Tile coordinate to check
     * @return true if tile is in visible set
     */
    bool can_see_tile(const world::TileCoord& coord) const;

    /**
     * @brief Get visibility factor for a specific tile
     * @param coord Tile coordinate to check
     * @return Visibility factor (0.0-1.0), or 0.0 if not visible
     */
    float get_tile_visibility(const world::TileCoord& coord) const;

    /**
     * @brief Check if a specific entity is visible
     * @param entity_id Entity ID to check
     * @return true if entity is in visible set
     */
    bool can_see_entity(uint32_t entity_id) const;

    /**
     * @brief Clear all visibility data
     */
    void clear();

    /**
     * @brief Get number of visible tiles
     */
    size_t visible_tile_count() const { return visible_tiles.size(); }

    /**
     * @brief Get number of visible entities
     */
    size_t visible_entity_count() const { return visible_entities.size(); }
};

} // namespace lore::ecs