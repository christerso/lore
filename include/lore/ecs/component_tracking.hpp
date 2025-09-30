#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/entity_manager.hpp>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <functional>
#include <bitset>
#include <typeindex>

namespace lore::ecs {

// Forward declarations
class ComponentDependencyGraph;
class ComponentChangeTracker;
class AdvancedWorld;
class ReactiveSystem;

// Component dependency tracking
class ComponentDependencyManager {
public:
    ComponentDependencyManager();
    ~ComponentDependencyManager();

    // Dependency registration
    template<typename Dependent, typename Dependency>
    void add_dependency();

    template<typename Dependent>
    void remove_dependencies();

    void clear_all_dependencies();

    // Dependency queries
    template<typename T>
    std::vector<ComponentID> get_dependencies() const;

    template<typename T>
    std::vector<ComponentID> get_dependents() const;

    template<typename T>
    bool has_dependencies() const;

    template<typename T>
    bool is_dependency_of(ComponentID component_id) const;

    // Validation
    bool validate_no_cycles() const;
    std::vector<std::vector<ComponentID>> find_dependency_cycles() const;

    // Component update ordering based on dependencies
    std::vector<ComponentID> get_update_order() const;
    std::vector<ComponentID> get_update_order_for(ComponentID component_id) const;

private:
    std::unordered_map<ComponentID, std::unordered_set<ComponentID>> dependencies_;
    std::unordered_map<ComponentID, std::unordered_set<ComponentID>> dependents_;
    mutable std::shared_mutex dependency_mutex_;

    void internal_add_dependency(ComponentID dependent, ComponentID dependency);
    void internal_remove_dependencies(ComponentID dependent);
    bool has_cycle_from(ComponentID start, std::unordered_set<ComponentID>& visited, std::unordered_set<ComponentID>& in_stack) const;
};

// Component archetype system for efficient queries
class ComponentArchetype {
public:
    ComponentArchetype();
    explicit ComponentArchetype(const ComponentBitSet& components);
    ~ComponentArchetype();

    // Archetype management
    void add_component(ComponentID component_id);
    void remove_component(ComponentID component_id);
    bool has_component(ComponentID component_id) const;
    bool matches(const ComponentBitSet& query) const;
    bool matches_all(const std::vector<ComponentID>& required) const;
    bool matches_none(const std::vector<ComponentID>& excluded) const;

    // Entity management
    void add_entity(EntityHandle entity);
    void remove_entity(EntityHandle entity);
    bool contains_entity(EntityHandle entity) const;
    const std::vector<EntityHandle>& get_entities() const { return entities_; }

    // Archetype information
    const ComponentBitSet& get_component_mask() const { return component_mask_; }
    std::size_t get_entity_count() const { return entities_.size(); }
    std::vector<ComponentID> get_component_list() const;

    // Performance optimization
    void reserve_entities(std::size_t count);
    void compact_storage();

    // Comparison
    bool operator==(const ComponentArchetype& other) const;
    bool operator!=(const ComponentArchetype& other) const;
    std::size_t hash() const;

private:
    ComponentBitSet component_mask_;
    std::vector<EntityHandle> entities_;
    std::unordered_set<Entity> entity_lookup_;
    mutable std::shared_mutex archetype_mutex_;
};

// Archetype manager for efficient entity queries
class ArchetypeManager {
public:
    ArchetypeManager();
    ~ArchetypeManager();

    // Archetype lifecycle
    std::shared_ptr<ComponentArchetype> get_or_create_archetype(const ComponentBitSet& components);
    std::shared_ptr<ComponentArchetype> find_archetype(const ComponentBitSet& components) const;
    void remove_empty_archetypes();

    // Entity management
    void add_entity_to_archetype(EntityHandle entity, const ComponentBitSet& components);
    void remove_entity_from_archetype(EntityHandle entity, const ComponentBitSet& components);
    void move_entity_between_archetypes(EntityHandle entity,
                                       const ComponentBitSet& old_components,
                                       const ComponentBitSet& new_components);

    // Query support
    std::vector<std::shared_ptr<ComponentArchetype>> find_matching_archetypes(
        const std::vector<ComponentID>& required,
        const std::vector<ComponentID>& excluded = {}) const;

    // Statistics
    std::size_t get_archetype_count() const;
    std::size_t get_total_entities() const;
    std::size_t get_largest_archetype_size() const;
    float get_archetype_fragmentation() const;

private:
    std::unordered_map<std::size_t, std::shared_ptr<ComponentArchetype>> archetypes_;
    mutable std::shared_mutex archetype_mutex_;

    std::size_t hash_component_mask(const ComponentBitSet& mask) const;
};

// Advanced query builder with compile-time optimization
template<typename... RequiredComponents>
class TypedQuery {
public:
    TypedQuery();
    ~TypedQuery();

    // Query refinement
    template<typename... ExcludedComponents>
    TypedQuery<RequiredComponents...>& without();

    TypedQuery<RequiredComponents...>& in_archetype(std::shared_ptr<ComponentArchetype> archetype);
    TypedQuery<RequiredComponents...>& with_relationship(EntityHandle target, bool is_parent = true);

    // Execution
    template<typename Func>
    void for_each(const AdvancedWorld& world, Func&& func) const;

    template<typename Func>
    void for_each_parallel(const AdvancedWorld& world, Func&& func, std::size_t thread_count = 0) const;

    std::vector<EntityHandle> collect(const AdvancedWorld& world) const;
    std::size_t count(const AdvancedWorld& world) const;

    // Performance optimization
    void enable_caching(bool enable = true);
    void invalidate_cache();
    bool is_cached() const;

private:
    std::vector<ComponentID> required_components_;
    std::vector<ComponentID> excluded_components_;
    std::shared_ptr<ComponentArchetype> archetype_filter_;
    std::optional<std::pair<EntityHandle, bool>> relationship_filter_;

    mutable bool caching_enabled_{false};
    mutable std::vector<EntityHandle> cached_results_;
    mutable bool cache_valid_{false};
    mutable std::mutex cache_mutex_;

    void build_component_lists();
    bool matches_filters(const AdvancedWorld& world, EntityHandle entity) const;
};

// Component change tracking for reactive systems
class ComponentChangeTracker {
public:
    struct ChangeRecord {
        EntityHandle entity;
        ComponentID component_id;
        std::chrono::steady_clock::time_point timestamp;
        enum class Type { Added, Modified, Removed } type;
    };

    ComponentChangeTracker();
    ~ComponentChangeTracker();

    // Change recording
    void record_component_added(EntityHandle entity, ComponentID component_id);
    void record_component_modified(EntityHandle entity, ComponentID component_id);
    void record_component_removed(EntityHandle entity, ComponentID component_id);

    // Change queries
    std::vector<ChangeRecord> get_changes_since(std::chrono::steady_clock::time_point timestamp) const;
    std::vector<ChangeRecord> get_changes_for_entity(EntityHandle entity) const;
    std::vector<ChangeRecord> get_changes_for_component(ComponentID component_id) const;

    // Reactive system support
    template<typename ComponentType>
    void register_reactive_system(std::shared_ptr<ReactiveSystem> system);

    void process_pending_changes();
    void clear_old_changes(std::chrono::steady_clock::duration max_age);

    // Configuration
    void set_max_change_history(std::size_t max_changes);
    void set_change_batching(bool enable);

private:
    std::vector<ChangeRecord> change_history_;
    std::size_t max_history_size_{10000};
    std::atomic<bool> batching_enabled_{false};

    std::unordered_map<ComponentID, std::vector<std::weak_ptr<ReactiveSystem>>> reactive_systems_;
    mutable std::shared_mutex tracker_mutex_;

    void notify_reactive_systems(const ChangeRecord& change);
    void cleanup_weak_references();
};

// Base class for reactive systems that respond to component changes
class ReactiveSystem : public System {
public:
    ReactiveSystem();
    virtual ~ReactiveSystem();

    // Component interest registration
    template<typename ComponentType>
    void watch_component_added();

    template<typename ComponentType>
    void watch_component_modified();

    template<typename ComponentType>
    void watch_component_removed();

    // Reactive callbacks
    virtual void on_component_added([[maybe_unused]] EntityHandle entity, [[maybe_unused]] ComponentID component_id) {}
    virtual void on_component_modified([[maybe_unused]] EntityHandle entity, [[maybe_unused]] ComponentID component_id) {}
    virtual void on_component_removed([[maybe_unused]] EntityHandle entity, [[maybe_unused]] ComponentID component_id) {}

    // System interface
    void update(World& world, float delta_time) override final;

    // Configuration
    void set_update_frequency(float frequency_hz);
    void set_batch_processing(bool enable);

protected:
    virtual void reactive_update([[maybe_unused]] World& world, [[maybe_unused]] float delta_time) {}

private:
    std::unordered_set<ComponentID> watched_added_;
    std::unordered_set<ComponentID> watched_modified_;
    std::unordered_set<ComponentID> watched_removed_;

    float update_frequency_hz_{60.0f};
    bool batch_processing_{true};
    std::chrono::steady_clock::time_point last_update_;

    std::vector<ComponentChangeTracker::ChangeRecord> pending_changes_;
    mutable std::mutex pending_mutex_;

    friend class ComponentChangeTracker;
    void notify_change(const ComponentChangeTracker::ChangeRecord& change);
};

// Advanced query system with SIMD optimization
class SIMDQuery {
public:
    SIMDQuery();
    ~SIMDQuery();

    // Query building
    template<typename... Components>
    SIMDQuery& with();

    template<typename... Components>
    SIMDQuery& without();

    // SIMD-optimized execution
    template<typename Func>
    void for_each_simd(const AdvancedWorld& world, Func&& func) const;

    // Batch processing with vectorization
    template<typename ComponentType>
    void process_components_vectorized(const AdvancedWorld& world,
                                     std::function<void(ComponentType*, std::size_t)> processor) const;

    // Memory-aligned iteration
    template<typename ComponentType>
    void iterate_aligned(const AdvancedWorld& world,
                        std::function<void(const ComponentType&, EntityHandle)> callback) const;

private:
    std::vector<ComponentID> required_components_;
    std::vector<ComponentID> excluded_components_;

    static constexpr std::size_t SIMD_ALIGNMENT = 32; // AVX2 alignment
    static constexpr std::size_t BATCH_SIZE = 8; // Process 8 entities at once

    std::vector<EntityHandle> get_matching_entities_simd(const AdvancedWorld& world) const;
    bool is_simd_compatible() const;
};

// Memory pool for component storage optimization
class ComponentMemoryPool {
public:
    ComponentMemoryPool(std::size_t component_size, std::size_t alignment, std::size_t initial_capacity = 1024);
    ~ComponentMemoryPool();

    // Memory allocation
    void* allocate();
    void deallocate(void* ptr);

    // Bulk operations
    std::vector<void*> allocate_batch(std::size_t count);
    void deallocate_batch(const std::vector<void*>& ptrs);

    // Memory management
    void compact();
    void reserve(std::size_t additional_capacity);
    void shrink_to_fit();

    // Statistics
    std::size_t get_allocated_count() const;
    std::size_t get_capacity() const;
    std::size_t get_memory_usage() const;
    float get_fragmentation() const;

private:
    struct Block {
        void* memory;
        std::size_t size;
        bool in_use;
    };

    std::size_t component_size_;
    std::size_t alignment_;
    std::vector<std::unique_ptr<std::byte[]>> memory_chunks_;
    std::vector<Block> blocks_;
    std::vector<void*> free_blocks_;

    mutable std::mutex pool_mutex_;

    void* allocate_new_chunk(std::size_t chunk_size);
    void initialize_blocks_in_chunk(void* chunk, std::size_t chunk_size);
};

// Global component pool manager
class ComponentPoolManager {
public:
    static ComponentPoolManager& instance();

    template<typename T>
    ComponentMemoryPool& get_pool();

    template<typename T>
    void configure_pool(std::size_t initial_capacity);

    // Global memory management
    void compact_all_pools();
    std::size_t get_total_memory_usage() const;
    void set_memory_budget(std::size_t bytes);

private:
    ComponentPoolManager() = default;

    std::unordered_map<std::type_index, std::unique_ptr<ComponentMemoryPool>> pools_;
    mutable std::mutex pools_mutex_;
    std::atomic<std::size_t> memory_budget_{0};
};

} // namespace lore::ecs

#include "component_tracking.inl"