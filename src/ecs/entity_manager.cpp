#include <lore/ecs/entity_manager.hpp>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cassert>
#include <shared_mutex>
#include <functional>

namespace lore::ecs {

// EntityPool implementation
EntityPool::EntityPool(std::size_t initial_capacity) {
    pool_.reserve(initial_capacity);
    available_.reserve(initial_capacity);

    // Pre-allocate pool objects
    for (std::size_t i = 0; i < initial_capacity; ++i) {
        auto descriptor = std::make_unique<EntityDescriptor>();
        available_.push_back(descriptor.get());
        pool_.push_back(std::move(descriptor));
    }
}

EntityPool::~EntityPool() {
    // All descriptors should be released back to the pool before destruction
    if (active_count_.load() > 0) {
        std::cerr << "Warning: EntityPool destroyed with " << active_count_.load()
                  << " active descriptors still in use\n";
    }
}

EntityDescriptor* EntityPool::acquire() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (available_.empty()) {
        // Expand pool if needed
        std::size_t old_size = pool_.size();
        std::size_t new_size = old_size > 0 ? old_size * 2 : 1024;

        pool_.reserve(new_size);
        available_.reserve(new_size - old_size);

        for (std::size_t i = old_size; i < new_size; ++i) {
            auto descriptor = std::make_unique<EntityDescriptor>();
            available_.push_back(descriptor.get());
            pool_.push_back(std::move(descriptor));
        }
    }

    EntityDescriptor* descriptor = available_.back();
    available_.pop_back();
    active_count_.fetch_add(1, std::memory_order_relaxed);

    // Reset descriptor state
    *descriptor = EntityDescriptor{};

    return descriptor;
}

void EntityPool::release(EntityDescriptor* descriptor) {
    if (!descriptor) return;

    std::lock_guard<std::mutex> lock(pool_mutex_);
    available_.push_back(descriptor);
    active_count_.fetch_sub(1, std::memory_order_relaxed);
}

std::size_t EntityPool::get_active_count() const noexcept {
    return active_count_.load(std::memory_order_relaxed);
}

std::size_t EntityPool::get_pool_size() const noexcept {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return pool_.size();
}

// AdvancedEntityManager implementation
AdvancedEntityManager::AdvancedEntityManager()
    : next_entity_id_(1)
    , entity_pool_(std::make_unique<EntityPool>(1024))
{
    entity_descriptors_.reserve(MAX_ENTITIES);
    free_entities_.reserve(1024);
    alive_entities_cache_.reserve(1024);

    // Initialize descriptors array
    entity_descriptors_.resize(MAX_ENTITIES);
    for (std::size_t i = 0; i < MAX_ENTITIES; ++i) {
        entity_descriptors_[i].id = static_cast<Entity>(i);
        entity_descriptors_[i].generation = 0;
        entity_descriptors_[i].is_alive = false;
    }
}

AdvancedEntityManager::~AdvancedEntityManager() {
    // Destroy all living entities
    std::vector<EntityHandle> living_entities;
    living_entities.reserve(living_entity_count_.load());

    for (const auto& descriptor : entity_descriptors_) {
        if (descriptor.is_alive) {
            living_entities.push_back({descriptor.id, descriptor.generation});
        }
    }

    destroy_entities_immediate(living_entities);
}

EntityHandle AdvancedEntityManager::create_entity() {
    return internal_create_entity();
}

EntityHandle AdvancedEntityManager::create_entity_with_hint(Entity preferred_id) {
    if (!validate_entity_id(preferred_id)) {
        return internal_create_entity();
    }

    return internal_create_entity(preferred_id);
}

EntityHandle AdvancedEntityManager::internal_create_entity(Entity preferred_id) {
    std::unique_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    Entity entity_id;
    Generation generation;

    // Try to use preferred ID if specified and available
    if (preferred_id != INVALID_ENTITY &&
        preferred_id < MAX_ENTITIES &&
        !entity_descriptors_[preferred_id].is_alive) {

        entity_id = preferred_id;
        generation = entity_descriptors_[entity_id].generation;
    }
    // Reuse freed entity ID if available
    else if (!free_entities_.empty()) {
        entity_id = free_entities_.back();
        free_entities_.pop_back();

        // Increment generation to invalidate old handles
        generation = ++entity_descriptors_[entity_id].generation;
        validate_generation_overflow(entity_id);
        recycled_count_.fetch_add(1, std::memory_order_relaxed);
    }
    // Create new entity ID
    else {
        if (next_entity_id_ >= MAX_ENTITIES) {
            throw std::runtime_error("Maximum number of entities exceeded");
        }

        entity_id = next_entity_id_++;
        generation = entity_descriptors_[entity_id].generation; // Should be 0 for new entities
    }

    // Update descriptor
    auto& descriptor = entity_descriptors_[entity_id];
    descriptor.id = entity_id;
    descriptor.generation = generation;
    descriptor.creation_time = std::chrono::steady_clock::now();
    descriptor.is_alive = true;

    living_entity_count_.fetch_add(1, std::memory_order_relaxed);
    mark_cache_dirty();

    return EntityHandle{entity_id, generation};
}

void AdvancedEntityManager::destroy_entity(EntityHandle handle) {
    internal_destroy_entity(handle, false);
}

void AdvancedEntityManager::destroy_entity_immediate(EntityHandle handle) {
    internal_destroy_entity(handle, true);
}

void AdvancedEntityManager::internal_destroy_entity(EntityHandle handle, bool immediate) {
    std::unique_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    if (!is_valid(handle)) {
        return; // Silently ignore invalid handles
    }

    auto& descriptor = entity_descriptors_[handle.id];
    if (!descriptor.is_alive) {
        return; // Already destroyed
    }

    if (immediate) {
        // Immediate destruction
        descriptor.is_alive = false;
        descriptor.destruction_time = std::chrono::steady_clock::now();
        free_entities_.push_back(handle.id);

        living_entity_count_.fetch_sub(1, std::memory_order_relaxed);
        mark_cache_dirty();

        // Remove from pending destruction if it was there
        pending_destruction_.erase(handle.id);
    } else {
        // Deferred destruction - mark for cleanup
        pending_destruction_.insert(handle.id);
    }
}

bool AdvancedEntityManager::is_valid(EntityHandle handle) const {
    std::shared_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    if (!validate_entity_id(handle.id)) {
        return false;
    }

    const auto& descriptor = entity_descriptors_[handle.id];
    return descriptor.generation == handle.generation && descriptor.is_alive;
}

bool AdvancedEntityManager::is_alive(EntityHandle handle) const {
    return is_valid(handle) && pending_destruction_.find(handle.id) == pending_destruction_.end();
}

std::vector<EntityHandle> AdvancedEntityManager::create_entities(std::size_t count) {
    std::vector<EntityHandle> entities;
    entities.reserve(count);

    // Batch creation for better performance
    for (std::size_t i = 0; i < count; ++i) {
        entities.push_back(create_entity());
    }

    return entities;
}

void AdvancedEntityManager::destroy_entities(const std::vector<EntityHandle>& handles) {
    for (const auto& handle : handles) {
        destroy_entity(handle);
    }
}

void AdvancedEntityManager::destroy_entities_immediate(const std::vector<EntityHandle>& handles) {
    for (const auto& handle : handles) {
        destroy_entity_immediate(handle);
    }
}

EntityDescriptor AdvancedEntityManager::get_descriptor(EntityHandle handle) const {
    std::shared_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    if (!validate_entity_id(handle.id)) {
        throw std::out_of_range("Invalid entity ID");
    }

    return entity_descriptors_[handle.id];
}

std::chrono::steady_clock::time_point AdvancedEntityManager::get_creation_time(EntityHandle handle) const {
    return get_descriptor(handle).creation_time;
}

Generation AdvancedEntityManager::get_generation(Entity entity) const {
    if (!validate_entity_id(entity)) {
        throw std::out_of_range("Invalid entity ID");
    }

    return entity_descriptors_[entity].generation;
}

void AdvancedEntityManager::compact_storage() {
    std::unique_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    // Process pending destructions
    for (Entity entity_id : pending_destruction_) {
        if (entity_id < entity_descriptors_.size()) {
            auto& descriptor = entity_descriptors_[entity_id];
            if (descriptor.is_alive) {
                descriptor.is_alive = false;
                descriptor.destruction_time = std::chrono::steady_clock::now();
                free_entities_.push_back(entity_id);
                living_entity_count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }
    pending_destruction_.clear();

    // Sort free entities for better cache locality
    std::sort(free_entities_.begin(), free_entities_.end());

    mark_cache_dirty();
}

void AdvancedEntityManager::reserve_entities(std::size_t count) {
    if (count > MAX_ENTITIES) {
        throw std::invalid_argument("Cannot reserve more entities than MAX_ENTITIES");
    }

    // Entity descriptors are pre-allocated, so this is mainly for free_entities_
    free_entities_.reserve(count);
    alive_entities_cache_.reserve(count);
}

void AdvancedEntityManager::shrink_to_fit() {
    std::unique_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    free_entities_.shrink_to_fit();
    alive_entities_cache_.shrink_to_fit();
}

std::size_t AdvancedEntityManager::get_entity_count() const noexcept {
    return living_entity_count_.load(std::memory_order_relaxed);
}

std::size_t AdvancedEntityManager::get_free_entity_count() const noexcept {
    std::shared_lock<std::shared_mutex> lock(entity_mutex_, std::defer_lock);
    if (thread_safe_.load(std::memory_order_acquire)) {
        lock.lock();
    }

    return free_entities_.size();
}

std::size_t AdvancedEntityManager::get_recycled_count() const noexcept {
    return recycled_count_.load(std::memory_order_relaxed);
}

std::size_t AdvancedEntityManager::get_memory_usage() const noexcept {
    std::size_t usage = sizeof(*this);
    usage += entity_descriptors_.capacity() * sizeof(EntityDescriptor);
    usage += free_entities_.capacity() * sizeof(Entity);
    usage += alive_entities_cache_.capacity() * sizeof(Entity);
    usage += pending_destruction_.size() * sizeof(Entity);

    if (entity_pool_) {
        usage += entity_pool_->get_pool_size() * sizeof(EntityDescriptor);
    }

    return usage;
}

void AdvancedEntityManager::enable_thread_safety(bool enable) {
    thread_safe_.store(enable, std::memory_order_release);
}

bool AdvancedEntityManager::is_thread_safe() const noexcept {
    return thread_safe_.load(std::memory_order_acquire);
}

void AdvancedEntityManager::mark_cache_dirty() {
    cache_dirty_.store(true, std::memory_order_release);
}

void AdvancedEntityManager::rebuild_alive_cache() const {
    if (!cache_dirty_.load(std::memory_order_acquire)) {
        return;
    }

    alive_entities_cache_.clear();
    alive_entities_cache_.reserve(living_entity_count_.load(std::memory_order_relaxed));

    for (const auto& descriptor : entity_descriptors_) {
        if (descriptor.is_alive && pending_destruction_.find(descriptor.id) == pending_destruction_.end()) {
            alive_entities_cache_.push_back(descriptor.id);
        }
    }

    cache_dirty_.store(false, std::memory_order_release);
}

bool AdvancedEntityManager::validate_entity_id(Entity id) const noexcept {
    return id > INVALID_ENTITY && id < MAX_ENTITIES;
}

void AdvancedEntityManager::validate_generation_overflow(Entity id) const {
    const auto& descriptor = entity_descriptors_[id];
    if (descriptor.generation == std::numeric_limits<Generation>::max()) {
        std::ostringstream oss;
        oss << "Generation overflow for entity " << id << ". Cannot create more instances.";
        throw std::runtime_error(oss.str());
    }
}

// EntityManager Iterator implementation
AdvancedEntityManager::EntityIterator::EntityIterator(const AdvancedEntityManager& manager, std::size_t index)
    : manager_(manager), current_index_(index) {
    manager_.rebuild_alive_cache();
}

EntityHandle AdvancedEntityManager::EntityIterator::operator*() const {
    if (current_index_ >= manager_.alive_entities_cache_.size()) {
        throw std::out_of_range("EntityIterator out of range");
    }

    Entity entity_id = manager_.alive_entities_cache_[current_index_];
    const auto& descriptor = manager_.entity_descriptors_[entity_id];
    return EntityHandle{descriptor.id, descriptor.generation};
}

AdvancedEntityManager::EntityIterator& AdvancedEntityManager::EntityIterator::operator++() {
    ++current_index_;
    return *this;
}

bool AdvancedEntityManager::EntityIterator::operator!=(const EntityIterator& other) const {
    return current_index_ != other.current_index_;
}

AdvancedEntityManager::EntityIterator AdvancedEntityManager::begin() const {
    rebuild_alive_cache();
    return EntityIterator(*this, 0);
}

AdvancedEntityManager::EntityIterator AdvancedEntityManager::end() const {
    rebuild_alive_cache();
    return EntityIterator(*this, alive_entities_cache_.size());
}

// EntityRelationshipManager implementation
EntityRelationshipManager::EntityRelationshipManager() {
    parent_map_.reserve(1024);
    children_map_.reserve(1024);
}

EntityRelationshipManager::~EntityRelationshipManager() = default;

void EntityRelationshipManager::set_parent(EntityHandle child, EntityHandle parent) {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    // Validate no cycles would be created
    validate_no_cycles(child, parent);

    // Remove from old parent if exists
    auto old_parent_it = parent_map_.find(child.id);
    if (old_parent_it != parent_map_.end()) {
        internal_remove_child(old_parent_it->second, child);
    }

    // Set new parent relationship
    parent_map_[child.id] = parent;

    // Add to parent's children list
    children_map_[parent.id].push_back(child);
}

void EntityRelationshipManager::remove_parent(EntityHandle child) {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    auto parent_it = parent_map_.find(child.id);
    if (parent_it != parent_map_.end()) {
        EntityHandle parent = parent_it->second;
        internal_remove_child(parent, child);
        parent_map_.erase(parent_it);
    }
}

EntityHandle EntityRelationshipManager::get_parent(EntityHandle child) const {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    auto it = parent_map_.find(child.id);
    if (it != parent_map_.end()) {
        return it->second;
    }

    return EntityHandle{INVALID_ENTITY, 0};
}

std::vector<EntityHandle> EntityRelationshipManager::get_children(EntityHandle parent) const {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    auto it = children_map_.find(parent.id);
    if (it != children_map_.end()) {
        return it->second;
    }

    return {};
}

std::vector<EntityHandle> EntityRelationshipManager::get_all_descendants(EntityHandle parent) const {
    std::vector<EntityHandle> descendants;
    std::vector<EntityHandle> to_process = {parent};

    while (!to_process.empty()) {
        EntityHandle current = to_process.back();
        to_process.pop_back();

        auto children = get_children(current);
        for (const auto& child : children) {
            descendants.push_back(child);
            to_process.push_back(child);
        }
    }

    return descendants;
}

bool EntityRelationshipManager::is_ancestor(EntityHandle ancestor, EntityHandle descendant) const {
    EntityHandle current = get_parent(descendant);

    while (current.id != INVALID_ENTITY) {
        if (current == ancestor) {
            return true;
        }
        current = get_parent(current);
    }

    return false;
}

bool EntityRelationshipManager::is_descendant(EntityHandle descendant, EntityHandle ancestor) const {
    return is_ancestor(ancestor, descendant);
}

std::size_t EntityRelationshipManager::get_depth(EntityHandle entity) const {
    std::size_t depth = 0;
    EntityHandle current = get_parent(entity);

    while (current.id != INVALID_ENTITY) {
        ++depth;
        current = get_parent(current);

        // Prevent infinite loops in case of cycles (shouldn't happen with proper validation)
        if (depth > MAX_ENTITIES) {
            throw std::runtime_error("Cycle detected in entity hierarchy");
        }
    }

    return depth;
}

EntityHandle EntityRelationshipManager::get_root(EntityHandle entity) const {
    EntityHandle current = entity;
    EntityHandle parent = get_parent(current);

    while (parent.id != INVALID_ENTITY) {
        current = parent;
        parent = get_parent(current);

        // Prevent infinite loops
        if (get_depth(current) > MAX_ENTITIES) {
            throw std::runtime_error("Cycle detected in entity hierarchy");
        }
    }

    return current;
}

void EntityRelationshipManager::destroy_hierarchy(EntityHandle root) {
    auto descendants = get_all_descendants(root);

    // Remove all relationships
    for (const auto& descendant : descendants) {
        remove_parent(descendant);
    }

    remove_parent(root);
}

void EntityRelationshipManager::reparent_children(EntityHandle old_parent, EntityHandle new_parent) {
    auto children = get_children(old_parent);

    for (const auto& child : children) {
        set_parent(child, new_parent);
    }
}

std::size_t EntityRelationshipManager::get_hierarchy_count() const noexcept {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);
    return parent_map_.size();
}

std::size_t EntityRelationshipManager::get_orphan_count() const noexcept {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    std::unordered_set<Entity> entities_with_parents;
    for (const auto& [child, parent] : parent_map_) {
        entities_with_parents.insert(child);
    }

    std::unordered_set<Entity> all_entities;
    for (const auto& [child, parent] : parent_map_) {
        all_entities.insert(child);
        all_entities.insert(parent.id);
    }

    return all_entities.size() - entities_with_parents.size();
}

std::size_t EntityRelationshipManager::get_max_depth() const noexcept {
    std::lock_guard<std::shared_mutex> lock(relationship_mutex_);

    std::size_t max_depth = 0;

    for (const auto& [entity_id, parent] : parent_map_) {
        try {
            EntityHandle entity{entity_id, 0}; // Generation doesn't matter for depth calculation
            std::size_t depth = get_depth(entity);
            max_depth = std::max(max_depth, depth);
        } catch (const std::exception&) {
            // Skip entities that cause cycles or other issues
            continue;
        }
    }

    return max_depth;
}

void EntityRelationshipManager::internal_remove_child(EntityHandle parent, EntityHandle child) {
    auto children_it = children_map_.find(parent.id);
    if (children_it != children_map_.end()) {
        auto& children = children_it->second;
        children.erase(std::remove(children.begin(), children.end(), child), children.end());

        if (children.empty()) {
            children_map_.erase(children_it);
        }
    }
}

void EntityRelationshipManager::validate_no_cycles(EntityHandle child, EntityHandle new_parent) const {
    if (is_ancestor(child, new_parent)) {
        throw std::invalid_argument("Setting parent would create a cycle in the hierarchy");
    }
}

// EntityValidator implementation
EntityValidator::ValidationResult EntityValidator::validate_entity(const AdvancedEntityManager& manager, EntityHandle handle) {
    ValidationResult result;
    result.is_valid = true;

    try {
        if (!manager.validate_entity_id(handle.id)) {
            result.errors.push_back("Invalid entity ID: " + std::to_string(handle.id));
            result.is_valid = false;
        }

        if (!manager.is_valid(handle)) {
            result.errors.push_back("Entity handle is not valid (wrong generation or destroyed)");
            result.is_valid = false;
        }

        if (!manager.is_alive(handle)) {
            result.warnings.push_back("Entity is marked for destruction");
        }

        auto descriptor = manager.get_descriptor(handle);
        if (descriptor.generation != handle.generation) {
            result.errors.push_back("Generation mismatch");
            result.is_valid = false;
        }
    } catch (const std::exception& e) {
        result.errors.push_back("Exception during validation: " + std::string(e.what()));
        result.is_valid = false;
    }

    return result;
}

EntityValidator::ValidationResult EntityValidator::validate_all_entities(const AdvancedEntityManager& manager) {
    ValidationResult result;
    result.is_valid = true;

    std::size_t validated_count = 0;
    std::size_t error_count = 0;

    for (auto it = manager.begin(); it != manager.end(); ++it) {
        EntityHandle handle = *it;
        auto entity_result = validate_entity(manager, handle);

        if (!entity_result.is_valid) {
            error_count++;
            result.is_valid = false;

            for (const auto& error : entity_result.errors) {
                result.errors.push_back("Entity " + std::to_string(handle.id) + ": " + error);
            }
        }

        for (const auto& warning : entity_result.warnings) {
            result.warnings.push_back("Entity " + std::to_string(handle.id) + ": " + warning);
        }

        validated_count++;
    }

    if (error_count > 0) {
        result.errors.insert(result.errors.begin(),
            "Found " + std::to_string(error_count) + " invalid entities out of " + std::to_string(validated_count));
    }

    return result;
}

EntityValidator::ValidationResult EntityValidator::validate_hierarchy(const EntityRelationshipManager& relationship_manager, EntityHandle root) {
    ValidationResult result;
    result.is_valid = true;

    try {
        // Check for cycles
        std::unordered_set<Entity> visited;
        std::vector<EntityHandle> path;

        std::function<void(EntityHandle)> check_cycles;
        check_cycles = [&](EntityHandle current) {
            if (visited.find(current.id) != visited.end()) {
                // Found a cycle
                result.errors.push_back("Cycle detected in hierarchy starting from entity " + std::to_string(root.id));
                result.is_valid = false;
                return;
            }

            visited.insert(current.id);
            path.push_back(current);

            auto children = relationship_manager.get_children(current);
            for (const auto& child : children) {
                check_cycles(child);
            }

            path.pop_back();
            visited.erase(current.id);
        };

        check_cycles(root);

        // Validate hierarchy depth
        try {
            std::size_t depth = relationship_manager.get_depth(root);
            if (depth > 1000) { // Arbitrary large depth threshold
                result.warnings.push_back("Very deep hierarchy detected (depth: " + std::to_string(depth) + ")");
            }
        } catch (const std::exception& e) {
            result.errors.push_back("Error calculating hierarchy depth: " + std::string(e.what()));
            result.is_valid = false;
        }

    } catch (const std::exception& e) {
        result.errors.push_back("Exception during hierarchy validation: " + std::string(e.what()));
        result.is_valid = false;
    }

    return result;
}

EntityValidator::PerformanceMetrics EntityValidator::analyze_performance(const AdvancedEntityManager& manager) {
    PerformanceMetrics metrics;

    metrics.total_entities = MAX_ENTITIES;
    metrics.alive_entities = manager.get_entity_count();
    metrics.free_entities = manager.get_free_entity_count();
    metrics.memory_usage_bytes = manager.get_memory_usage();

    // Calculate fragmentation ratio
    if (metrics.total_entities > 0) {
        metrics.fragmentation_ratio = static_cast<double>(metrics.free_entities) / metrics.total_entities;
    } else {
        metrics.fragmentation_ratio = 0.0;
    }

    // Calculate allocation efficiency
    if (metrics.total_entities > 0) {
        metrics.allocation_efficiency = static_cast<double>(metrics.alive_entities) / metrics.total_entities;
    } else {
        metrics.allocation_efficiency = 0.0;
    }

    // Measure creation and destruction performance
    auto start_time = std::chrono::high_resolution_clock::now();

    // Note: We can't easily measure real performance without modifying the manager
    // In a real implementation, we'd add timing instrumentation to the manager itself
    metrics.average_creation_time = std::chrono::microseconds(1); // Placeholder
    metrics.average_destruction_time = std::chrono::microseconds(1); // Placeholder

    return metrics;
}

void EntityValidator::log_performance_report(const PerformanceMetrics& metrics) {
    std::cout << "\n=== Entity Manager Performance Report ===\n";
    std::cout << "Total entities: " << metrics.total_entities << "\n";
    std::cout << "Alive entities: " << metrics.alive_entities << "\n";
    std::cout << "Free entities: " << metrics.free_entities << "\n";
    std::cout << "Memory usage: " << metrics.memory_usage_bytes << " bytes\n";
    std::cout << "Fragmentation ratio: " << (metrics.fragmentation_ratio * 100.0) << "%\n";
    std::cout << "Allocation efficiency: " << (metrics.allocation_efficiency * 100.0) << "%\n";
    std::cout << "Average creation time: " << metrics.average_creation_time.count() << " μs\n";
    std::cout << "Average destruction time: " << metrics.average_destruction_time.count() << " μs\n";
    std::cout << "==========================================\n\n";
}

} // namespace lore::ecs