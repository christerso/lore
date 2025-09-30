#pragma once

#include <lore/ecs/ecs.hpp>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <unordered_set>
#include <chrono>

namespace lore::ecs {

// Advanced entity ID with generation counter for safe reuse
struct EntityDescriptor {
    Entity id;
    Generation generation;
    std::chrono::steady_clock::time_point creation_time;
    std::chrono::steady_clock::time_point destruction_time;
    bool is_alive;

    bool operator==(const EntityDescriptor& other) const noexcept {
        return id == other.id && generation == other.generation;
    }

    bool operator!=(const EntityDescriptor& other) const noexcept {
        return !(*this == other);
    }
};

// Memory pool for entity descriptors to reduce allocation overhead
class EntityPool {
public:
    EntityPool(std::size_t initial_capacity = 1024);
    ~EntityPool();

    EntityDescriptor* acquire();
    void release(EntityDescriptor* descriptor);

    std::size_t get_active_count() const noexcept;
    std::size_t get_pool_size() const noexcept;

private:
    mutable std::mutex pool_mutex_;
    std::vector<std::unique_ptr<EntityDescriptor>> pool_;
    std::vector<EntityDescriptor*> available_;
    std::atomic<std::size_t> active_count_{0};
};

// Advanced entity manager with generation tracking and recycling
class AdvancedEntityManager {
public:
    AdvancedEntityManager();
    ~AdvancedEntityManager();

    // Entity lifecycle with enhanced safety
    EntityHandle create_entity();
    EntityHandle create_entity_with_hint(Entity preferred_id);
    void destroy_entity(EntityHandle handle);
    void destroy_entity_immediate(EntityHandle handle);
    bool is_valid(EntityHandle handle) const;
    bool is_alive(EntityHandle handle) const;

    // Entity introspection
    EntityDescriptor get_descriptor(EntityHandle handle) const;
    std::chrono::steady_clock::time_point get_creation_time(EntityHandle handle) const;
    Generation get_generation(Entity entity) const;

    // Bulk operations for performance
    std::vector<EntityHandle> create_entities(std::size_t count);
    void destroy_entities(const std::vector<EntityHandle>& handles);
    void destroy_entities_immediate(const std::vector<EntityHandle>& handles);

    // Memory management
    void compact_storage();
    void reserve_entities(std::size_t count);
    void shrink_to_fit();

    // Statistics and debugging
    std::size_t get_entity_count() const noexcept;
    std::size_t get_free_entity_count() const noexcept;
    std::size_t get_recycled_count() const noexcept;
    std::size_t get_memory_usage() const noexcept;

    // Iteration support for systems
    class EntityIterator {
    public:
        EntityIterator(const AdvancedEntityManager& manager, std::size_t index);

        EntityHandle operator*() const;
        EntityIterator& operator++();
        bool operator!=(const EntityIterator& other) const;

    private:
        const AdvancedEntityManager& manager_;
        std::size_t current_index_;
    };

    EntityIterator begin() const;
    EntityIterator end() const;

    // Thread safety
    void enable_thread_safety(bool enable = true);
    bool is_thread_safe() const noexcept;

    // Validation (public for EntityValidator)
    bool validate_entity_id(Entity id) const noexcept;

private:
    // Core storage
    std::vector<EntityDescriptor> entity_descriptors_;
    std::vector<Entity> free_entities_;
    std::unordered_set<Entity> pending_destruction_;

    // Allocation tracking
    Entity next_entity_id_;
    std::atomic<std::size_t> living_entity_count_{0};
    std::atomic<std::size_t> recycled_count_{0};

    // Memory pooling
    std::unique_ptr<EntityPool> entity_pool_;

    // Thread safety
    mutable std::shared_mutex entity_mutex_;
    std::atomic<bool> thread_safe_{false};

    // Performance optimization
    mutable std::vector<Entity> alive_entities_cache_;
    mutable std::atomic<bool> cache_dirty_{true};

    // Internal helpers
    void mark_cache_dirty();
    void rebuild_alive_cache() const;
    void internal_destroy_entity(EntityHandle handle, bool immediate);
    EntityHandle internal_create_entity(Entity preferred_id = INVALID_ENTITY);

    // Validation
    void validate_generation_overflow(Entity id) const;
};

// Entity relationship management for hierarchical structures
class EntityRelationshipManager {
public:
    EntityRelationshipManager();
    ~EntityRelationshipManager();

    // Parent-child relationships
    void set_parent(EntityHandle child, EntityHandle parent);
    void remove_parent(EntityHandle child);
    EntityHandle get_parent(EntityHandle child) const;
    std::vector<EntityHandle> get_children(EntityHandle parent) const;
    std::vector<EntityHandle> get_all_descendants(EntityHandle parent) const;

    // Hierarchy queries
    bool is_ancestor(EntityHandle ancestor, EntityHandle descendant) const;
    bool is_descendant(EntityHandle descendant, EntityHandle ancestor) const;
    std::size_t get_depth(EntityHandle entity) const;
    EntityHandle get_root(EntityHandle entity) const;

    // Bulk operations
    void destroy_hierarchy(EntityHandle root);
    void reparent_children(EntityHandle old_parent, EntityHandle new_parent);

    // Iteration
    class HierarchyIterator {
    public:
        enum class TraversalOrder {
            PreOrder,
            PostOrder,
            BreadthFirst
        };

        HierarchyIterator(const EntityRelationshipManager& manager,
                         EntityHandle root,
                         TraversalOrder order = TraversalOrder::PreOrder);

        EntityHandle operator*() const;
        HierarchyIterator& operator++();
        bool operator!=(const HierarchyIterator& other) const;

    private:
        const EntityRelationshipManager& manager_;
        std::vector<EntityHandle> traversal_queue_;
        std::size_t current_index_;
        TraversalOrder order_;

        void build_traversal_queue(EntityHandle root);
    };

    HierarchyIterator iterate_hierarchy(EntityHandle root,
                                       HierarchyIterator::TraversalOrder order = HierarchyIterator::TraversalOrder::PreOrder) const;

    // Statistics
    std::size_t get_hierarchy_count() const noexcept;
    std::size_t get_orphan_count() const noexcept;
    std::size_t get_max_depth() const noexcept;

private:
    std::unordered_map<Entity, EntityHandle> parent_map_;
    std::unordered_map<Entity, std::vector<EntityHandle>> children_map_;
    mutable std::shared_mutex relationship_mutex_;

    void internal_remove_child(EntityHandle parent, EntityHandle child);
    void validate_no_cycles(EntityHandle child, EntityHandle new_parent) const;
};

// Entity validation and debugging utilities
class EntityValidator {
public:
    struct ValidationResult {
        bool is_valid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    static ValidationResult validate_entity(const AdvancedEntityManager& manager, EntityHandle handle);
    static ValidationResult validate_all_entities(const AdvancedEntityManager& manager);
    static ValidationResult validate_hierarchy(const EntityRelationshipManager& relationship_manager, EntityHandle root);

    // Performance analysis
    struct PerformanceMetrics {
        std::size_t total_entities;
        std::size_t alive_entities;
        std::size_t free_entities;
        double fragmentation_ratio;
        std::size_t memory_usage_bytes;
        double allocation_efficiency;
        std::chrono::microseconds average_creation_time;
        std::chrono::microseconds average_destruction_time;
    };

    static PerformanceMetrics analyze_performance(const AdvancedEntityManager& manager);
    static void log_performance_report(const PerformanceMetrics& metrics);
};

} // namespace lore::ecs