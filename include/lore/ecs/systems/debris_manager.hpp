#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>
#include <lore/physics/physics.hpp>
#include <vector>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief Debris entity data for tracking and management
 */
struct DebrisData {
    Entity entity;                    ///< Entity handle
    float creation_time = 0.0f;       ///< When debris was created
    uint32_t triangle_count = 0;      ///< Number of triangles in this debris
    math::Vec3 position{0.0f};        ///< Current position
    float distance_from_camera = 0.0f; ///< Distance from camera (for LOD)
    bool is_merged = false;           ///< Has been merged with other debris
    bool use_gpu_instancing = false;  ///< Using GPU instancing?
};

/**
 * @brief Manages fracture debris with polygon budget constraints
 *
 * Handles:
 * - Polygon budget enforcement (max triangles, max entities)
 * - LOD for distant debris (reduce triangle count)
 * - Debris lifetime management (remove old debris)
 * - Debris merging (combine nearby small pieces)
 * - GPU instancing for similar debris
 * - Automatic cleanup when budget exceeded
 *
 * This prevents fracture systems from overwhelming the renderer with geometry.
 *
 * Example:
 * @code
 * auto& manager = world.get_system<DebrisManager>();
 * manager.set_max_debris_entities(500);
 * manager.set_max_total_triangles(50000);
 *
 * // Register debris when created
 * manager.register_debris(debris_entity, triangle_count);
 *
 * // Manager automatically handles cleanup
 * manager.update(world, delta_time);
 * @endcode
 */
class DebrisManager : public System {
public:
    /**
     * @brief Configuration for debris management
     */
    struct Config {
        uint32_t max_debris_entities = 500;        ///< Max number of debris entities
        uint32_t max_total_triangles = 50000;      ///< Max total triangles across all debris
        float debris_lifetime_seconds = 30.0f;     ///< Time before debris removed (0 = never)
        float merge_distance = 0.5f;               ///< Distance to merge debris (meters)
        bool use_gpu_instancing = true;            ///< Enable GPU instancing for similar debris
        bool enable_lod = true;                    ///< Enable LOD for distant debris

        // LOD distances
        float lod_distance_near = 20.0f;           ///< Distance for no LOD reduction
        float lod_distance_far = 50.0f;            ///< Distance for maximum LOD reduction
        float lod_reduction_near = 1.0f;           ///< Triangle multiplier at near distance (1.0 = 100%)
        float lod_reduction_far = 0.25f;           ///< Triangle multiplier at far distance (0.25 = 25%)
    };

    DebrisManager() = default;
    ~DebrisManager() override = default;

    void init(World& world) override {
        current_time_ = 0.0f;
        total_triangle_count_ = 0;
    }

    /**
     * @brief Update debris management
     *
     * - Removes expired debris
     * - Applies LOD based on distance
     * - Merges nearby debris
     * - Removes oldest debris if budget exceeded
     */
    void update(World& world, float delta_time) override {
        current_time_ += delta_time;

        // Update distances from camera
        update_distances_from_camera(world);

        // Remove expired debris (lifetime exceeded)
        if (config_.debris_lifetime_seconds > 0.0f) {
            remove_expired_debris(world);
        }

        // Apply LOD based on distance
        if (config_.enable_lod) {
            apply_lod(world);
        }

        // Merge nearby debris to reduce entity count
        merge_nearby_debris(world);

        // If budget exceeded, remove oldest debris
        while (debris_entities_.size() > config_.max_debris_entities ||
               total_triangle_count_ > config_.max_total_triangles) {
            remove_oldest_debris(world);
        }
    }

    void shutdown(World& world) override {
        // Clean up all debris
        for (auto& [entity_id, data] : debris_entities_) {
            if (world.is_valid(data.entity)) {
                world.destroy_entity(data.entity);
            }
        }
        debris_entities_.clear();
        total_triangle_count_ = 0;
    }

    /**
     * @brief Register new debris entity
     *
     * @param entity Debris entity
     * @param triangle_count Number of triangles in debris
     */
    void register_debris(Entity entity, uint32_t triangle_count) {
        DebrisData data{
            .entity = entity,
            .creation_time = current_time_,
            .triangle_count = triangle_count,
            .position = math::Vec3{0.0f},
            .distance_from_camera = 0.0f,
            .is_merged = false,
            .use_gpu_instancing = config_.use_gpu_instancing
        };

        debris_entities_[entity.id()] = data;
        total_triangle_count_ += triangle_count;
    }

    /**
     * @brief Unregister debris entity
     */
    void unregister_debris(Entity entity) {
        auto it = debris_entities_.find(entity.id());
        if (it != debris_entities_.end()) {
            total_triangle_count_ -= it->second.triangle_count;
            debris_entities_.erase(it);
        }
    }

    /**
     * @brief Get configuration (mutable)
     */
    Config& get_config() noexcept {
        return config_;
    }

    /**
     * @brief Get configuration (const)
     */
    const Config& get_config() const noexcept {
        return config_;
    }

    /**
     * @brief Set maximum debris entities
     */
    void set_max_debris_entities(uint32_t max) noexcept {
        config_.max_debris_entities = max;
    }

    /**
     * @brief Set maximum total triangles
     */
    void set_max_total_triangles(uint32_t max) noexcept {
        config_.max_total_triangles = max;
    }

    /**
     * @brief Set debris lifetime
     */
    void set_debris_lifetime(float seconds) noexcept {
        config_.debris_lifetime_seconds = seconds;
    }

    /**
     * @brief Get current debris count
     */
    size_t get_debris_count() const noexcept {
        return debris_entities_.size();
    }

    /**
     * @brief Get current total triangle count
     */
    uint32_t get_total_triangle_count() const noexcept {
        return total_triangle_count_;
    }

    /**
     * @brief Check if budget is exceeded
     */
    bool is_budget_exceeded() const noexcept {
        return debris_entities_.size() > config_.max_debris_entities ||
               total_triangle_count_ > config_.max_total_triangles;
    }

private:
    /**
     * @brief Update distances from camera for LOD
     */
    void update_distances_from_camera(World& world) {
        // TODO: Get camera position from camera system
        math::Vec3 camera_pos{0.0f, 0.0f, 0.0f};

        for (auto& [entity_id, data] : debris_entities_) {
            if (!world.is_valid(data.entity)) continue;

            // Get entity position
            auto* transform = world.try_get<TransformComponent>(data.entity);
            if (transform) {
                data.position = transform->position;
                data.distance_from_camera = math::length(camera_pos - transform->position);
            }
        }
    }

    /**
     * @brief Remove debris that exceeded lifetime
     */
    void remove_expired_debris(World& world) {
        std::vector<uint64_t> to_remove;

        for (auto& [entity_id, data] : debris_entities_) {
            float age = current_time_ - data.creation_time;
            if (age > config_.debris_lifetime_seconds) {
                to_remove.push_back(entity_id);
            }
        }

        for (uint64_t entity_id : to_remove) {
            auto it = debris_entities_.find(entity_id);
            if (it != debris_entities_.end()) {
                if (world.is_valid(it->second.entity)) {
                    world.destroy_entity(it->second.entity);
                }
                total_triangle_count_ -= it->second.triangle_count;
                debris_entities_.erase(it);
            }
        }
    }

    /**
     * @brief Apply LOD to distant debris
     */
    void apply_lod(World& world) {
        for (auto& [entity_id, data] : debris_entities_) {
            if (!world.is_valid(data.entity)) continue;

            // Calculate LOD factor based on distance
            float t = (data.distance_from_camera - config_.lod_distance_near) /
                     (config_.lod_distance_far - config_.lod_distance_near);
            t = math::clamp(t, 0.0f, 1.0f);

            float lod_factor = math::lerp(config_.lod_reduction_near, config_.lod_reduction_far, t);

            // TODO: Apply LOD to mesh component
            // For now, this is a placeholder
            // Real implementation would reduce triangle count in mesh
        }
    }

    /**
     * @brief Merge nearby small debris pieces
     */
    void merge_nearby_debris(World& world) {
        // Simple spatial hashing for proximity detection
        // TODO: Implement proper spatial hash for efficiency

        std::vector<uint64_t> merged_entities;

        for (auto& [entity_id_a, data_a] : debris_entities_) {
            if (data_a.is_merged) continue;
            if (!world.is_valid(data_a.entity)) continue;

            for (auto& [entity_id_b, data_b] : debris_entities_) {
                if (entity_id_a == entity_id_b) continue;
                if (data_b.is_merged) continue;
                if (!world.is_valid(data_b.entity)) continue;

                float distance = math::length(data_a.position - data_b.position);
                if (distance <= config_.merge_distance) {
                    // Merge debris_b into debris_a
                    // TODO: Implement actual mesh merging
                    // For now, just mark as merged and destroy

                    data_a.triangle_count += data_b.triangle_count;
                    data_b.is_merged = true;
                    merged_entities.push_back(entity_id_b);
                }
            }
        }

        // Remove merged entities
        for (uint64_t entity_id : merged_entities) {
            auto it = debris_entities_.find(entity_id);
            if (it != debris_entities_.end()) {
                if (world.is_valid(it->second.entity)) {
                    world.destroy_entity(it->second.entity);
                }
                // Triangle count already transferred, don't subtract
                debris_entities_.erase(it);
            }
        }
    }

    /**
     * @brief Remove oldest debris entity
     */
    void remove_oldest_debris(World& world) {
        if (debris_entities_.empty()) return;

        // Find oldest debris
        float oldest_time = std::numeric_limits<float>::max();
        uint64_t oldest_entity_id = 0;

        for (const auto& [entity_id, data] : debris_entities_) {
            if (data.creation_time < oldest_time) {
                oldest_time = data.creation_time;
                oldest_entity_id = entity_id;
            }
        }

        // Remove oldest
        auto it = debris_entities_.find(oldest_entity_id);
        if (it != debris_entities_.end()) {
            if (world.is_valid(it->second.entity)) {
                world.destroy_entity(it->second.entity);
            }
            total_triangle_count_ -= it->second.triangle_count;
            debris_entities_.erase(it);
        }
    }

    /// Configuration
    Config config_;

    /// All tracked debris entities
    std::unordered_map<uint64_t, DebrisData> debris_entities_;

    /// Current total triangle count
    uint32_t total_triangle_count_ = 0;

    /// Current simulation time
    float current_time_ = 0.0f;
};

} // namespace lore::ecs