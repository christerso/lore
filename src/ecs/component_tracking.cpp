#include <lore/ecs/component_tracking.hpp>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <iostream>
#include <cstring>
#include <immintrin.h>

namespace lore::ecs {

// ComponentDependencyManager implementation
ComponentDependencyManager::ComponentDependencyManager() = default;
ComponentDependencyManager::~ComponentDependencyManager() = default;

void ComponentDependencyManager::clear_all_dependencies() {
    std::unique_lock<std::shared_mutex> lock(dependency_mutex_);
    dependencies_.clear();
    dependents_.clear();
}

bool ComponentDependencyManager::validate_no_cycles() const {
    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);

    std::unordered_set<ComponentID> visited;
    std::unordered_set<ComponentID> in_stack;

    for (const auto& [component_id, _] : dependencies_) {
        if (visited.find(component_id) == visited.end()) {
            if (has_cycle_from(component_id, visited, in_stack)) {
                return false;
            }
        }
    }

    return true;
}

std::vector<std::vector<ComponentID>> ComponentDependencyManager::find_dependency_cycles() const {
    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);

    std::vector<std::vector<ComponentID>> cycles;
    std::unordered_set<ComponentID> visited;
    std::unordered_set<ComponentID> in_stack;
    std::vector<ComponentID> current_path;

    std::function<void(ComponentID)> dfs = [&](ComponentID component_id) {
        if (in_stack.find(component_id) != in_stack.end()) {
            // Found a cycle - extract it from current path
            auto cycle_start = std::find(current_path.begin(), current_path.end(), component_id);
            if (cycle_start != current_path.end()) {
                cycles.emplace_back(cycle_start, current_path.end());
            }
            return;
        }

        if (visited.find(component_id) != visited.end()) {
            return;
        }

        visited.insert(component_id);
        in_stack.insert(component_id);
        current_path.push_back(component_id);

        auto deps_it = dependencies_.find(component_id);
        if (deps_it != dependencies_.end()) {
            for (ComponentID dep : deps_it->second) {
                dfs(dep);
            }
        }

        current_path.pop_back();
        in_stack.erase(component_id);
    };

    for (const auto& [component_id, _] : dependencies_) {
        if (visited.find(component_id) == visited.end()) {
            dfs(component_id);
        }
    }

    return cycles;
}

std::vector<ComponentID> ComponentDependencyManager::get_update_order() const {
    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);

    std::vector<ComponentID> order;
    std::unordered_map<ComponentID, int> in_degree;
    std::queue<ComponentID> zero_in_degree;

    // Calculate in-degrees
    std::unordered_set<ComponentID> all_components;
    for (const auto& [component_id, deps] : dependencies_) {
        all_components.insert(component_id);
        for (ComponentID dep : deps) {
            all_components.insert(dep);
        }
    }

    for (ComponentID component_id : all_components) {
        in_degree[component_id] = 0;
    }

    for (const auto& [component_id, deps] : dependencies_) {
        in_degree[component_id] = static_cast<int>(deps.size());
    }

    // Find components with no dependencies
    for (const auto& [component_id, degree] : in_degree) {
        if (degree == 0) {
            zero_in_degree.push(component_id);
        }
    }

    // Topological sort
    while (!zero_in_degree.empty()) {
        ComponentID current = zero_in_degree.front();
        zero_in_degree.pop();
        order.push_back(current);

        // Update in-degrees of dependents
        auto dependents_it = dependents_.find(current);
        if (dependents_it != dependents_.end()) {
            for (ComponentID dependent : dependents_it->second) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    zero_in_degree.push(dependent);
                }
            }
        }
    }

    return order;
}

std::vector<ComponentID> ComponentDependencyManager::get_update_order_for(ComponentID component_id) const {
    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);

    std::vector<ComponentID> order;
    std::unordered_set<ComponentID> visited;

    std::function<void(ComponentID)> collect_dependencies = [&](ComponentID id) {
        if (visited.find(id) != visited.end()) {
            return;
        }
        visited.insert(id);

        auto deps_it = dependencies_.find(id);
        if (deps_it != dependencies_.end()) {
            for (ComponentID dep : deps_it->second) {
                collect_dependencies(dep);
            }
        }

        order.push_back(id);
    };

    collect_dependencies(component_id);
    return order;
}

void ComponentDependencyManager::internal_add_dependency(ComponentID dependent, ComponentID dependency) {
    std::unique_lock<std::shared_mutex> lock(dependency_mutex_);

    dependencies_[dependent].insert(dependency);
    dependents_[dependency].insert(dependent);
}

void ComponentDependencyManager::internal_remove_dependencies(ComponentID dependent) {
    std::unique_lock<std::shared_mutex> lock(dependency_mutex_);

    auto deps_it = dependencies_.find(dependent);
    if (deps_it != dependencies_.end()) {
        for (ComponentID dependency : deps_it->second) {
            auto dependents_it = dependents_.find(dependency);
            if (dependents_it != dependents_.end()) {
                dependents_it->second.erase(dependent);
                if (dependents_it->second.empty()) {
                    dependents_.erase(dependents_it);
                }
            }
        }
        dependencies_.erase(deps_it);
    }
}

bool ComponentDependencyManager::has_cycle_from(ComponentID start,
                                               std::unordered_set<ComponentID>& visited,
                                               std::unordered_set<ComponentID>& in_stack) const {
    if (in_stack.find(start) != in_stack.end()) {
        return true; // Cycle detected
    }

    if (visited.find(start) != visited.end()) {
        return false; // Already processed
    }

    visited.insert(start);
    in_stack.insert(start);

    auto deps_it = dependencies_.find(start);
    if (deps_it != dependencies_.end()) {
        for (ComponentID dep : deps_it->second) {
            if (has_cycle_from(dep, visited, in_stack)) {
                return true;
            }
        }
    }

    in_stack.erase(start);
    return false;
}

// ComponentArchetype implementation
ComponentArchetype::ComponentArchetype() = default;

ComponentArchetype::ComponentArchetype(const ComponentBitSet& components)
    : component_mask_(components) {
    entities_.reserve(64);
    entity_lookup_.reserve(64);
}

ComponentArchetype::~ComponentArchetype() = default;

void ComponentArchetype::add_component(ComponentID component_id) {
    if (component_id < MAX_COMPONENT_TYPES) {
        std::unique_lock<std::shared_mutex> lock(archetype_mutex_);
        component_mask_.set(component_id);
    }
}

void ComponentArchetype::remove_component(ComponentID component_id) {
    if (component_id < MAX_COMPONENT_TYPES) {
        std::unique_lock<std::shared_mutex> lock(archetype_mutex_);
        component_mask_.reset(component_id);
    }
}

bool ComponentArchetype::has_component(ComponentID component_id) const {
    if (component_id >= MAX_COMPONENT_TYPES) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    return component_mask_.test(component_id);
}

bool ComponentArchetype::matches(const ComponentBitSet& query) const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    return (component_mask_ & query) == query;
}

bool ComponentArchetype::matches_all(const std::vector<ComponentID>& required) const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    for (ComponentID component_id : required) {
        if (component_id >= MAX_COMPONENT_TYPES || !component_mask_.test(component_id)) {
            return false;
        }
    }
    return true;
}

bool ComponentArchetype::matches_none(const std::vector<ComponentID>& excluded) const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    for (ComponentID component_id : excluded) {
        if (component_id < MAX_COMPONENT_TYPES && component_mask_.test(component_id)) {
            return false;
        }
    }
    return true;
}

void ComponentArchetype::add_entity(EntityHandle entity) {
    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);

    if (entity_lookup_.find(entity.id) == entity_lookup_.end()) {
        entities_.push_back(entity);
        entity_lookup_.insert(entity.id);
    }
}

void ComponentArchetype::remove_entity(EntityHandle entity) {
    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);

    auto lookup_it = entity_lookup_.find(entity.id);
    if (lookup_it != entity_lookup_.end()) {
        // Find and remove from entities vector
        auto it = std::find(entities_.begin(), entities_.end(), entity);
        if (it != entities_.end()) {
            // Swap with last element for O(1) removal
            if (it != entities_.end() - 1) {
                *it = entities_.back();
            }
            entities_.pop_back();
        }
        entity_lookup_.erase(lookup_it);
    }
}

bool ComponentArchetype::contains_entity(EntityHandle entity) const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    return entity_lookup_.find(entity.id) != entity_lookup_.end();
}

std::vector<ComponentID> ComponentArchetype::get_component_list() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    std::vector<ComponentID> components;
    for (std::size_t i = 0; i < MAX_COMPONENT_TYPES; ++i) {
        if (component_mask_.test(i)) {
            components.push_back(static_cast<ComponentID>(i));
        }
    }
    return components;
}

void ComponentArchetype::reserve_entities(std::size_t count) {
    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);
    entities_.reserve(count);
    entity_lookup_.reserve(count);
}

void ComponentArchetype::compact_storage() {
    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);
    entities_.shrink_to_fit();
}

bool ComponentArchetype::operator==(const ComponentArchetype& other) const {
    std::shared_lock<std::shared_mutex> lock1(archetype_mutex_);
    std::shared_lock<std::shared_mutex> lock2(other.archetype_mutex_);
    return component_mask_ == other.component_mask_;
}

bool ComponentArchetype::operator!=(const ComponentArchetype& other) const {
    return !(*this == other);
}

std::size_t ComponentArchetype::hash() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    return std::hash<ComponentBitSet>{}(component_mask_);
}

// ArchetypeManager implementation
ArchetypeManager::ArchetypeManager() {
    archetypes_.reserve(256);
}

ArchetypeManager::~ArchetypeManager() = default;

std::shared_ptr<ComponentArchetype> ArchetypeManager::get_or_create_archetype(const ComponentBitSet& components) {
    std::size_t hash = hash_component_mask(components);

    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);
    auto it = archetypes_.find(hash);
    if (it != archetypes_.end()) {
        return it->second;
    }

    auto archetype = std::make_shared<ComponentArchetype>(components);
    archetypes_[hash] = archetype;
    return archetype;
}

std::shared_ptr<ComponentArchetype> ArchetypeManager::find_archetype(const ComponentBitSet& components) const {
    std::size_t hash = hash_component_mask(components);

    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    auto it = archetypes_.find(hash);
    return (it != archetypes_.end()) ? it->second : nullptr;
}

void ArchetypeManager::remove_empty_archetypes() {
    std::unique_lock<std::shared_mutex> lock(archetype_mutex_);

    auto it = archetypes_.begin();
    while (it != archetypes_.end()) {
        if (it->second->get_entity_count() == 0) {
            it = archetypes_.erase(it);
        } else {
            ++it;
        }
    }
}

void ArchetypeManager::add_entity_to_archetype(EntityHandle entity, const ComponentBitSet& components) {
    auto archetype = get_or_create_archetype(components);
    archetype->add_entity(entity);
}

void ArchetypeManager::remove_entity_from_archetype(EntityHandle entity, const ComponentBitSet& components) {
    auto archetype = find_archetype(components);
    if (archetype) {
        archetype->remove_entity(entity);
    }
}

void ArchetypeManager::move_entity_between_archetypes(EntityHandle entity,
                                                     const ComponentBitSet& old_components,
                                                     const ComponentBitSet& new_components) {
    remove_entity_from_archetype(entity, old_components);
    add_entity_to_archetype(entity, new_components);
}

std::vector<std::shared_ptr<ComponentArchetype>> ArchetypeManager::find_matching_archetypes(
    const std::vector<ComponentID>& required,
    const std::vector<ComponentID>& excluded) const {

    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    std::vector<std::shared_ptr<ComponentArchetype>> matching;
    for (const auto& [hash, archetype] : archetypes_) {
        if (archetype->matches_all(required) && archetype->matches_none(excluded)) {
            matching.push_back(archetype);
        }
    }

    return matching;
}

std::size_t ArchetypeManager::get_archetype_count() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);
    return archetypes_.size();
}

std::size_t ArchetypeManager::get_total_entities() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    std::size_t total = 0;
    for (const auto& [hash, archetype] : archetypes_) {
        total += archetype->get_entity_count();
    }
    return total;
}

std::size_t ArchetypeManager::get_largest_archetype_size() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    std::size_t largest = 0;
    for (const auto& [hash, archetype] : archetypes_) {
        largest = std::max(largest, archetype->get_entity_count());
    }
    return largest;
}

float ArchetypeManager::get_archetype_fragmentation() const {
    std::shared_lock<std::shared_mutex> lock(archetype_mutex_);

    if (archetypes_.empty()) {
        return 0.0f;
    }

    std::size_t total_entities = get_total_entities();
    std::size_t non_empty_archetypes = 0;

    for (const auto& [hash, archetype] : archetypes_) {
        if (archetype->get_entity_count() > 0) {
            non_empty_archetypes++;
        }
    }

    if (total_entities == 0) {
        return 0.0f;
    }

    // Fragmentation = number of archetypes / entities ratio
    return static_cast<float>(non_empty_archetypes) / static_cast<float>(total_entities);
}

std::size_t ArchetypeManager::hash_component_mask(const ComponentBitSet& mask) const {
    return std::hash<ComponentBitSet>{}(mask);
}

// ComponentChangeTracker implementation
ComponentChangeTracker::ComponentChangeTracker() {
    change_history_.reserve(max_history_size_);
}

ComponentChangeTracker::~ComponentChangeTracker() = default;

void ComponentChangeTracker::record_component_added(EntityHandle entity, ComponentID component_id) {
    ChangeRecord record;
    record.entity = entity;
    record.component_id = component_id;
    record.timestamp = std::chrono::steady_clock::now();
    record.type = ChangeRecord::Type::Added;

    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    change_history_.push_back(record);

    if (change_history_.size() > max_history_size_) {
        change_history_.erase(change_history_.begin());
    }

    notify_reactive_systems(record);
}

void ComponentChangeTracker::record_component_modified(EntityHandle entity, ComponentID component_id) {
    ChangeRecord record;
    record.entity = entity;
    record.component_id = component_id;
    record.timestamp = std::chrono::steady_clock::now();
    record.type = ChangeRecord::Type::Modified;

    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    change_history_.push_back(record);

    if (change_history_.size() > max_history_size_) {
        change_history_.erase(change_history_.begin());
    }

    notify_reactive_systems(record);
}

void ComponentChangeTracker::record_component_removed(EntityHandle entity, ComponentID component_id) {
    ChangeRecord record;
    record.entity = entity;
    record.component_id = component_id;
    record.timestamp = std::chrono::steady_clock::now();
    record.type = ChangeRecord::Type::Removed;

    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    change_history_.push_back(record);

    if (change_history_.size() > max_history_size_) {
        change_history_.erase(change_history_.begin());
    }

    notify_reactive_systems(record);
}

std::vector<ComponentChangeTracker::ChangeRecord> ComponentChangeTracker::get_changes_since(std::chrono::steady_clock::time_point timestamp) const {
    std::shared_lock<std::shared_mutex> lock(tracker_mutex_);

    std::vector<ChangeRecord> recent_changes;
    for (const auto& record : change_history_) {
        if (record.timestamp >= timestamp) {
            recent_changes.push_back(record);
        }
    }

    return recent_changes;
}

std::vector<ComponentChangeTracker::ChangeRecord> ComponentChangeTracker::get_changes_for_entity(EntityHandle entity) const {
    std::shared_lock<std::shared_mutex> lock(tracker_mutex_);

    std::vector<ChangeRecord> entity_changes;
    for (const auto& record : change_history_) {
        if (record.entity == entity) {
            entity_changes.push_back(record);
        }
    }

    return entity_changes;
}

std::vector<ComponentChangeTracker::ChangeRecord> ComponentChangeTracker::get_changes_for_component(ComponentID component_id) const {
    std::shared_lock<std::shared_mutex> lock(tracker_mutex_);

    std::vector<ChangeRecord> component_changes;
    for (const auto& record : change_history_) {
        if (record.component_id == component_id) {
            component_changes.push_back(record);
        }
    }

    return component_changes;
}

void ComponentChangeTracker::process_pending_changes() {
    // This would process any batched changes if batching is enabled
    // Implementation depends on specific batching strategy
}

void ComponentChangeTracker::clear_old_changes(std::chrono::steady_clock::duration max_age) {
    auto cutoff_time = std::chrono::steady_clock::now() - max_age;

    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    change_history_.erase(
        std::remove_if(change_history_.begin(), change_history_.end(),
            [cutoff_time](const ChangeRecord& record) {
                return record.timestamp < cutoff_time;
            }),
        change_history_.end());
}

void ComponentChangeTracker::set_max_change_history(std::size_t max_changes) {
    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    max_history_size_ = max_changes;

    if (change_history_.size() > max_history_size_) {
        change_history_.erase(change_history_.begin(),
                            change_history_.begin() + (change_history_.size() - max_history_size_));
    }
}

void ComponentChangeTracker::set_change_batching(bool enable) {
    batching_enabled_.store(enable, std::memory_order_relaxed);
}

void ComponentChangeTracker::notify_reactive_systems(const ChangeRecord& change) {
    auto it = reactive_systems_.find(change.component_id);
    if (it != reactive_systems_.end()) {
        // Clean up expired weak pointers and notify active systems
        auto& systems = it->second;
        systems.erase(std::remove_if(systems.begin(), systems.end(),
            [&change](const std::weak_ptr<ReactiveSystem>& weak_system) {
                if (auto system = weak_system.lock()) {
                    system->notify_change(change);
                    return false;
                } else {
                    return true; // Remove expired weak pointer
                }
            }), systems.end());

        if (systems.empty()) {
            reactive_systems_.erase(it);
        }
    }
}

void ComponentChangeTracker::cleanup_weak_references() {
    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);

    auto it = reactive_systems_.begin();
    while (it != reactive_systems_.end()) {
        auto& systems = it->second;
        systems.erase(std::remove_if(systems.begin(), systems.end(),
            [](const std::weak_ptr<ReactiveSystem>& weak_system) {
                return weak_system.expired();
            }), systems.end());

        if (systems.empty()) {
            it = reactive_systems_.erase(it);
        } else {
            ++it;
        }
    }
}

// ReactiveSystem implementation
ReactiveSystem::ReactiveSystem() = default;
ReactiveSystem::~ReactiveSystem() = default;

void ReactiveSystem::update(World& world, float delta_time) {
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_);
    float update_interval_ms = 1000.0f / update_frequency_hz_;

    if (time_since_update.count() >= update_interval_ms) {
        // Process pending changes
        if (batch_processing_) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            for (const auto& change : pending_changes_) {
                switch (change.type) {
                    case ComponentChangeTracker::ChangeRecord::Type::Added:
                        on_component_added(change.entity, change.component_id);
                        break;
                    case ComponentChangeTracker::ChangeRecord::Type::Modified:
                        on_component_modified(change.entity, change.component_id);
                        break;
                    case ComponentChangeTracker::ChangeRecord::Type::Removed:
                        on_component_removed(change.entity, change.component_id);
                        break;
                }
            }
            pending_changes_.clear();
        }

        // Call derived class update
        reactive_update(world, delta_time);

        last_update_ = now;
    }
}

void ReactiveSystem::set_update_frequency(float frequency_hz) {
    update_frequency_hz_ = frequency_hz;
}

void ReactiveSystem::set_batch_processing(bool enable) {
    batch_processing_ = enable;
}

void ReactiveSystem::notify_change(const ComponentChangeTracker::ChangeRecord& change) {
    bool should_notify = false;

    switch (change.type) {
        case ComponentChangeTracker::ChangeRecord::Type::Added:
            should_notify = watched_added_.find(change.component_id) != watched_added_.end();
            break;
        case ComponentChangeTracker::ChangeRecord::Type::Modified:
            should_notify = watched_modified_.find(change.component_id) != watched_modified_.end();
            break;
        case ComponentChangeTracker::ChangeRecord::Type::Removed:
            should_notify = watched_removed_.find(change.component_id) != watched_removed_.end();
            break;
    }

    if (!should_notify) {
        return;
    }

    if (batch_processing_) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_changes_.push_back(change);
    } else {
        // Immediate processing
        switch (change.type) {
            case ComponentChangeTracker::ChangeRecord::Type::Added:
                on_component_added(change.entity, change.component_id);
                break;
            case ComponentChangeTracker::ChangeRecord::Type::Modified:
                on_component_modified(change.entity, change.component_id);
                break;
            case ComponentChangeTracker::ChangeRecord::Type::Removed:
                on_component_removed(change.entity, change.component_id);
                break;
        }
    }
}

// SIMDQuery implementation
SIMDQuery::SIMDQuery() = default;
SIMDQuery::~SIMDQuery() = default;

std::vector<EntityHandle> SIMDQuery::get_matching_entities_simd(const AdvancedWorld& world) const {
    std::vector<EntityHandle> results;

    // This is a simplified implementation
    // In a real scenario, we'd use SIMD instructions to accelerate the filtering
    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;

        // Check required components (simplified)
        bool matches = true;
        for ([[maybe_unused]] ComponentID required_id : required_components_) {
            // We'd need a SIMD-optimized way to check component presence
            // This is a placeholder
        }

        if (matches) {
            results.push_back(entity);
        }
    }

    return results;
}

bool SIMDQuery::is_simd_compatible() const {
    // Check if the query can benefit from SIMD optimization
    // For now, return true if we have a reasonable number of components to check
    return required_components_.size() + excluded_components_.size() >= 2;
}

// ComponentMemoryPool implementation
ComponentMemoryPool::ComponentMemoryPool(std::size_t component_size, std::size_t alignment, std::size_t initial_capacity)
    : component_size_(component_size), alignment_(alignment) {

    // Align component size to alignment boundary
    component_size_ = (component_size_ + alignment_ - 1) & ~(alignment_ - 1);

    // Allocate initial chunk
    std::size_t initial_chunk_size = component_size_ * initial_capacity;
    void* initial_chunk = allocate_new_chunk(initial_chunk_size);
    initialize_blocks_in_chunk(initial_chunk, initial_chunk_size);
}

ComponentMemoryPool::~ComponentMemoryPool() = default;

void* ComponentMemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (free_blocks_.empty()) {
        // Allocate new chunk
        std::size_t chunk_size = component_size_ * 256; // 256 components per chunk
        void* new_chunk = allocate_new_chunk(chunk_size);
        initialize_blocks_in_chunk(new_chunk, chunk_size);
    }

    void* block = free_blocks_.back();
    free_blocks_.pop_back();

    // Mark block as in use
    for (auto& block_info : blocks_) {
        if (block_info.memory == block) {
            block_info.in_use = true;
            break;
        }
    }

    return block;
}

void ComponentMemoryPool::deallocate(void* ptr) {
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(pool_mutex_);

    // Mark block as free
    for (auto& block_info : blocks_) {
        if (block_info.memory == ptr) {
            block_info.in_use = false;
            free_blocks_.push_back(ptr);
            break;
        }
    }
}

std::vector<void*> ComponentMemoryPool::allocate_batch(std::size_t count) {
    std::vector<void*> allocated;
    allocated.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        allocated.push_back(allocate());
    }

    return allocated;
}

void ComponentMemoryPool::deallocate_batch(const std::vector<void*>& ptrs) {
    for (void* ptr : ptrs) {
        deallocate(ptr);
    }
}

void ComponentMemoryPool::compact() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    // Remove empty chunks
    auto chunk_it = memory_chunks_.begin();
    while (chunk_it != memory_chunks_.end()) {
        void* chunk_start = chunk_it->get();
        bool chunk_has_used_blocks = false;

        for (const auto& block : blocks_) {
            if (block.memory >= chunk_start &&
                block.memory < static_cast<std::byte*>(chunk_start) + (component_size_ * 256) &&
                block.in_use) {
                chunk_has_used_blocks = true;
                break;
            }
        }

        if (!chunk_has_used_blocks) {
            // Remove blocks from this chunk
            blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(),
                [chunk_start, this](const Block& block) {
                    return block.memory >= chunk_start &&
                           block.memory < static_cast<std::byte*>(chunk_start) + (component_size_ * 256);
                }), blocks_.end());

            // Remove from free blocks
            free_blocks_.erase(std::remove_if(free_blocks_.begin(), free_blocks_.end(),
                [chunk_start, this](void* ptr) {
                    return ptr >= chunk_start &&
                           ptr < static_cast<std::byte*>(chunk_start) + (component_size_ * 256);
                }), free_blocks_.end());

            chunk_it = memory_chunks_.erase(chunk_it);
        } else {
            ++chunk_it;
        }
    }
}

void ComponentMemoryPool::reserve(std::size_t additional_capacity) {
    std::size_t chunk_size = component_size_ * additional_capacity;
    void* new_chunk = allocate_new_chunk(chunk_size);
    initialize_blocks_in_chunk(new_chunk, chunk_size);
}

void ComponentMemoryPool::shrink_to_fit() {
    compact();
}

std::size_t ComponentMemoryPool::get_allocated_count() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    return std::count_if(blocks_.begin(), blocks_.end(),
        [](const Block& block) { return block.in_use; });
}

std::size_t ComponentMemoryPool::get_capacity() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return blocks_.size();
}

std::size_t ComponentMemoryPool::get_memory_usage() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    std::size_t usage = sizeof(*this);
    for ([[maybe_unused]] const auto& chunk : memory_chunks_) {
        usage += component_size_ * 256; // Standard chunk size
    }
    return usage;
}

float ComponentMemoryPool::get_fragmentation() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (blocks_.empty()) {
        return 0.0f;
    }

    std::size_t used_blocks = get_allocated_count();
    return 1.0f - (static_cast<float>(used_blocks) / static_cast<float>(blocks_.size()));
}

void* ComponentMemoryPool::allocate_new_chunk(std::size_t chunk_size) {
    auto chunk = std::make_unique<std::byte[]>(chunk_size);
    void* chunk_ptr = chunk.get();
    memory_chunks_.push_back(std::move(chunk));
    return chunk_ptr;
}

void ComponentMemoryPool::initialize_blocks_in_chunk(void* chunk, std::size_t chunk_size) {
    std::size_t block_count = chunk_size / component_size_;
    std::byte* current = static_cast<std::byte*>(chunk);

    for (std::size_t i = 0; i < block_count; ++i) {
        Block block;
        block.memory = current;
        block.size = component_size_;
        block.in_use = false;

        blocks_.push_back(block);
        free_blocks_.push_back(current);

        current += component_size_;
    }
}

// ComponentPoolManager implementation
ComponentPoolManager& ComponentPoolManager::instance() {
    static ComponentPoolManager instance;
    return instance;
}

void ComponentPoolManager::compact_all_pools() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    for (auto& [type, pool] : pools_) {
        pool->compact();
    }
}

std::size_t ComponentPoolManager::get_total_memory_usage() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    std::size_t total = 0;
    for (const auto& [type, pool] : pools_) {
        total += pool->get_memory_usage();
    }
    return total;
}

void ComponentPoolManager::set_memory_budget(std::size_t bytes) {
    memory_budget_.store(bytes, std::memory_order_relaxed);
}

} // namespace lore::ecs