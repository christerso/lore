#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/entity_manager.hpp>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <future>
#include <chrono>
#include <array>

namespace lore::ecs {

// Forward declarations
class ComponentChangeNotifier;
class EntityQuery;
class SystemScheduler;

// World streaming for handling massive entity counts
class WorldRegion {
public:
    WorldRegion(int32_t x, int32_t y, int32_t z, float size);
    ~WorldRegion();

    // Region identification
    int32_t get_x() const noexcept { return x_; }
    int32_t get_y() const noexcept { return y_; }
    int32_t get_z() const noexcept { return z_; }
    float get_size() const noexcept { return size_; }

    // Entity management within region
    void add_entity(EntityHandle handle);
    void remove_entity(EntityHandle handle);
    bool contains_entity(EntityHandle handle) const;
    const std::unordered_set<Entity>& get_entities() const { return entities_; }

    // Region state
    bool is_active() const noexcept { return active_; }
    void set_active(bool active) { active_ = active; }

    // Memory management
    std::size_t get_memory_usage() const noexcept;
    void compact_storage();

private:
    int32_t x_, y_, z_;
    float size_;
    std::unordered_set<Entity> entities_;
    std::atomic<bool> active_{true};
    mutable std::shared_mutex region_mutex_;
};

// Level-of-detail system for entity processing
class LODManager {
public:
    enum class LODLevel {
        High = 0,     // Full processing
        Medium = 1,   // Reduced processing
        Low = 2,      // Minimal processing
        Culled = 3    // No processing
    };

    LODManager();
    ~LODManager();

    // LOD calculation
    LODLevel calculate_lod(EntityHandle entity, const float* observer_position) const;
    void set_observer_position(const float* position);
    void update_entity_lod(EntityHandle entity, LODLevel level);

    // LOD configuration
    void set_lod_distances(float high_distance, float medium_distance, float low_distance);
    void set_lod_update_frequency(float frequency_hz);

    // Entity queries by LOD
    std::vector<EntityHandle> get_entities_by_lod(LODLevel level) const;
    std::size_t get_entity_count_by_lod(LODLevel level) const;

private:
    std::array<float, 3> observer_position_{0.0f, 0.0f, 0.0f};
    std::array<float, 3> lod_distances_{100.0f, 500.0f, 1000.0f}; // High, Medium, Low thresholds
    std::unordered_map<Entity, LODLevel> entity_lod_cache_;
    mutable std::shared_mutex lod_mutex_;
    float update_frequency_hz_{10.0f};
    std::chrono::steady_clock::time_point last_update_;
};

// Enhanced world for massive scale simulation
class AdvancedWorld : public World {
    friend class EntityQuery;
    friend class SIMDQuery;
    friend class WorldSerializer;
    friend class LoreECS;
    template<typename...> friend class TypedQuery;
public:
    AdvancedWorld();
    ~AdvancedWorld() override;

    // Entity management
    EntityHandle create_entity() override;
    EntityHandle create_entity_in_region(int32_t x, int32_t y, int32_t z);
    void destroy_entity(EntityHandle handle) override;
    bool is_valid(EntityHandle handle) const override;

    // Component management with change notification
    template<typename T>
    void add_component(EntityHandle handle, T component);

    template<typename T>
    void remove_component(EntityHandle handle);

    template<typename T>
    T& get_component(EntityHandle handle);

    template<typename T>
    const T& get_component(EntityHandle handle) const;

    template<typename T>
    bool has_component(EntityHandle handle) const;

    // Batch component operations for performance
    template<typename T>
    void add_components_batch(const std::vector<EntityHandle>& entities, const std::vector<T>& components);

    template<typename T>
    void remove_components_batch(const std::vector<EntityHandle>& entities);

    // Component array access
    template<typename T>
    ComponentArray<T>& get_component_array();

    template<typename T>
    const ComponentArray<T>& get_component_array() const;

    // Advanced system management
    template<typename T, typename... Args>
    T& add_system(Args&&... args);

    template<typename T>
    T& get_system();

    template<typename T>
    void remove_system();

    void update(float delta_time);
    void update_parallel(float delta_time, std::size_t thread_count = 0);

    // World streaming and regions
    void set_region_size(float size);
    void set_active_region_bounds(const float* min_bounds, const float* max_bounds);
    WorldRegion* get_region(int32_t x, int32_t y, int32_t z);
    std::vector<WorldRegion*> get_active_regions() const;

    // Level of detail
    LODManager& get_lod_manager() { return lod_manager_; }
    const LODManager& get_lod_manager() const { return lod_manager_; }

    // Entity queries
    template<typename... ComponentTypes>
    std::unique_ptr<EntityQuery> create_query();

    // Component change notifications
    ComponentChangeNotifier& get_change_notifier() { return *change_notifier_; }

    // Memory management
    void compact_all_storage();
    void set_memory_budget(std::size_t bytes);
    std::size_t get_memory_usage() const;

    // Performance monitoring
    struct PerformanceStats {
        std::size_t total_entities;
        std::size_t active_entities;
        std::size_t systems_count;
        std::chrono::microseconds last_update_time;
        std::chrono::microseconds average_update_time;
        std::size_t memory_usage_bytes;
        float memory_fragmentation;
    };

    PerformanceStats get_performance_stats() const;

    // Statistics
    std::size_t get_entity_count() const noexcept;
    std::size_t get_component_type_count() const noexcept;
    std::size_t get_active_region_count() const noexcept;

private:
    std::unique_ptr<AdvancedEntityManager> entity_manager_;
    std::unique_ptr<EntityRelationshipManager> relationship_manager_;
    std::unordered_map<ComponentID, std::unique_ptr<void, void(*)(void*)>> component_arrays_;
    std::unique_ptr<SystemScheduler> system_scheduler_;
    std::unique_ptr<ComponentChangeNotifier> change_notifier_;

    // World streaming
    std::unordered_map<uint64_t, std::unique_ptr<WorldRegion>> regions_;
    float region_size_{1000.0f};
    std::array<float, 3> active_min_bounds_{-5000.0f, -5000.0f, -5000.0f};
    std::array<float, 3> active_max_bounds_{5000.0f, 5000.0f, 5000.0f};
    mutable std::shared_mutex regions_mutex_;

    // Level of detail
    LODManager lod_manager_;

    // Memory management
    std::atomic<std::size_t> memory_budget_{0}; // 0 = unlimited
    mutable std::atomic<std::size_t> memory_usage_cache_{0};

    // Performance tracking
    mutable std::mutex performance_mutex_;
    std::vector<std::chrono::microseconds> update_times_;
    std::size_t max_update_samples_{100};

    // Thread safety
    mutable std::shared_mutex world_mutex_;

    // Internal helpers
    template<typename T>
    ComponentArray<T>& get_or_create_component_array();

    uint64_t region_key(int32_t x, int32_t y, int32_t z) const;
    void update_memory_usage_cache() const;
    void cleanup_inactive_regions();
};

// Component change notification system
class ComponentChangeNotifier {
public:
    enum class ChangeType {
        Added,
        Modified,
        Removed
    };

    using ChangeCallback = std::function<void(EntityHandle, ComponentID, ChangeType)>;

    ComponentChangeNotifier();
    ~ComponentChangeNotifier();

    // Callback registration
    std::size_t register_callback(ComponentID component_id, ChangeCallback callback);
    std::size_t register_global_callback(ChangeCallback callback);
    void unregister_callback(std::size_t callback_id);

    // Change notification
    void notify_component_added(EntityHandle entity, ComponentID component_id);
    void notify_component_modified(EntityHandle entity, ComponentID component_id);
    void notify_component_removed(EntityHandle entity, ComponentID component_id);

    // Batch notifications
    void begin_batch();
    void end_batch();

private:
    struct CallbackInfo {
        std::size_t id;
        ComponentID component_id; // INVALID_ENTITY for global callbacks
        ChangeCallback callback;
    };

    std::vector<CallbackInfo> callbacks_;
    std::atomic<std::size_t> next_callback_id_{1};
    mutable std::shared_mutex callbacks_mutex_;

    // Batching
    std::atomic<bool> batch_mode_{false};
    std::vector<std::tuple<EntityHandle, ComponentID, ChangeType>> batched_changes_;
    std::mutex batch_mutex_;

    void internal_notify(EntityHandle entity, ComponentID component_id, ChangeType type);
};

// Advanced entity query system
class EntityQuery {
public:
    EntityQuery();
    virtual ~EntityQuery();

    // Query building
    template<typename T>
    EntityQuery& with();

    template<typename T>
    EntityQuery& without();

    EntityQuery& in_region(int32_t x, int32_t y, int32_t z);
    EntityQuery& in_lod_level(LODManager::LODLevel level);
    EntityQuery& with_relationship(EntityHandle target, bool is_parent = true);

    // Execution
    std::vector<EntityHandle> execute(const AdvancedWorld& world) const;
    void execute_foreach(const AdvancedWorld& world, std::function<void(EntityHandle)> callback) const;

    // Cached execution for performance
    void cache_results(const AdvancedWorld& world);
    const std::vector<EntityHandle>& get_cached_results() const;
    void invalidate_cache();
    bool is_cache_valid() const;

    // Statistics
    std::size_t get_result_count(const AdvancedWorld& world) const;
    std::chrono::microseconds get_last_execution_time() const;

private:
    std::vector<ComponentID> required_components_;
    std::vector<ComponentID> excluded_components_;
    std::optional<std::tuple<int32_t, int32_t, int32_t>> region_filter_;
    std::optional<LODManager::LODLevel> lod_filter_;
    std::optional<std::pair<EntityHandle, bool>> relationship_filter_;

    mutable std::vector<EntityHandle> cached_results_;
    mutable bool cache_valid_{false};
    mutable std::chrono::microseconds last_execution_time_{0};
    mutable std::mutex query_mutex_;

    bool matches_entity(const AdvancedWorld& world, EntityHandle entity) const;
};

// System scheduling with dependencies
class SystemScheduler {
public:
    SystemScheduler();
    ~SystemScheduler();

    // System management
    template<typename T, typename... Args>
    T& add_system(Args&&... args);

    template<typename T>
    T& get_system();

    template<typename T>
    void remove_system();

    // System dependencies
    template<typename Before, typename After>
    void add_dependency();

    void clear_dependencies();

    // Execution
    void update_all(AdvancedWorld& world, float delta_time);
    void update_parallel(AdvancedWorld& world, float delta_time, std::size_t thread_count);

    // System groups for parallel execution
    void create_system_group(const std::string& name, const std::vector<std::type_index>& system_types);
    void execute_system_group(const std::string& name, AdvancedWorld& world, float delta_time);

    // Performance monitoring
    struct SystemPerformance {
        std::string name;
        std::chrono::microseconds last_execution_time;
        std::chrono::microseconds average_execution_time;
        std::size_t execution_count;
    };

    std::vector<SystemPerformance> get_system_performance() const;

private:
    std::unordered_map<std::type_index, std::unique_ptr<System>> systems_;
    std::vector<std::type_index> execution_order_;
    std::unordered_map<std::type_index, std::vector<std::type_index>> dependencies_;
    std::unordered_map<std::string, std::vector<std::type_index>> system_groups_;

    // Performance tracking
    mutable std::mutex performance_mutex_;
    std::unordered_map<std::type_index, SystemPerformance> performance_data_;

    void compute_execution_order();
    void update_system_performance(const std::type_index& type, std::chrono::microseconds execution_time);
};

} // namespace lore::ecs

#include "world_manager.inl"