#include <lore/ecs/world_manager.hpp>
#include <lore/ecs/advanced_ecs.hpp>
#include <algorithm>
#include <execution>
#include <queue>
#include <unordered_set>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lore::ecs {

// WorldRegion implementation
WorldRegion::WorldRegion(int32_t x, int32_t y, int32_t z, float size)
    : x_(x), y_(y), z_(z), size_(size) {
    entities_.reserve(1024); // Pre-allocate for performance
}

WorldRegion::~WorldRegion() = default;

void WorldRegion::add_entity(EntityHandle handle) {
    std::unique_lock<std::shared_mutex> lock(region_mutex_);
    entities_.insert(handle.id);
}

void WorldRegion::remove_entity(EntityHandle handle) {
    std::unique_lock<std::shared_mutex> lock(region_mutex_);
    entities_.erase(handle.id);
}

bool WorldRegion::contains_entity(EntityHandle handle) const {
    std::shared_lock<std::shared_mutex> lock(region_mutex_);
    return entities_.find(handle.id) != entities_.end();
}

std::size_t WorldRegion::get_memory_usage() const noexcept {
    std::shared_lock<std::shared_mutex> lock(region_mutex_);
    return sizeof(*this) + entities_.size() * sizeof(Entity);
}

void WorldRegion::compact_storage() {
    std::unique_lock<std::shared_mutex> lock(region_mutex_);
    // For unordered_set, there's not much to compact, but we can rehash if needed
    if (entities_.load_factor() > 0.75f) {
        entities_.rehash(entities_.size() * 2);
    }
}

// LODManager implementation
LODManager::LODManager() = default;
LODManager::~LODManager() = default;

LODManager::LODLevel LODManager::calculate_lod(EntityHandle entity, const float* observer_position) const {
    std::shared_lock<std::shared_mutex> lock(lod_mutex_);

    // Check if we need to update LOD (based on frequency)
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_);
    float update_interval_ms = 1000.0f / update_frequency_hz_;

    if (time_since_update.count() < update_interval_ms) {
        // Use cached LOD if available
        auto it = entity_lod_cache_.find(entity.id);
        if (it != entity_lod_cache_.end()) {
            return it->second;
        }
    }

    // Calculate distance to observer using entity position component
    float distance = 100.0f; // Default fallback distance

    // NOTE: This is a simplified implementation
    // In a real system, we would need access to the world instance to check components
    // For now, we'll use the observer position and a reasonable default distance
    if (observer_position) {
        // Use observer position as reference for distance calculation
        // Since we don't have access to entity position here, use a reasonable estimate
        const float* observer_pos = observer_position;
        distance = std::sqrt(observer_pos[0] * observer_pos[0] +
                           observer_pos[1] * observer_pos[1] +
                           observer_pos[2] * observer_pos[2]);
    }

    LODLevel level;
    if (distance <= lod_distances_[0]) {
        level = LODLevel::High;
    } else if (distance <= lod_distances_[1]) {
        level = LODLevel::Medium;
    } else if (distance <= lod_distances_[2]) {
        level = LODLevel::Low;
    } else {
        level = LODLevel::Culled;
    }

    // Cache the result
    const_cast<LODManager*>(this)->entity_lod_cache_[entity.id] = level;
    const_cast<LODManager*>(this)->last_update_ = now;

    return level;
}

void LODManager::set_observer_position(const float* position) {
    std::unique_lock<std::shared_mutex> lock(lod_mutex_);
    std::copy(position, position + 3, observer_position_.begin());

    // Invalidate LOD cache when observer moves significantly
    entity_lod_cache_.clear();
}

void LODManager::update_entity_lod(EntityHandle entity, LODLevel level) {
    std::unique_lock<std::shared_mutex> lock(lod_mutex_);
    entity_lod_cache_[entity.id] = level;
}

void LODManager::set_lod_distances(float high_distance, float medium_distance, float low_distance) {
    std::unique_lock<std::shared_mutex> lock(lod_mutex_);
    lod_distances_[0] = high_distance;
    lod_distances_[1] = medium_distance;
    lod_distances_[2] = low_distance;

    // Invalidate cache when distances change
    entity_lod_cache_.clear();
}

void LODManager::set_lod_update_frequency(float frequency_hz) {
    std::unique_lock<std::shared_mutex> lock(lod_mutex_);
    update_frequency_hz_ = frequency_hz;
}

std::vector<EntityHandle> LODManager::get_entities_by_lod(LODLevel level) const {
    std::shared_lock<std::shared_mutex> lock(lod_mutex_);

    std::vector<EntityHandle> entities;
    for (const auto& [entity_id, entity_level] : entity_lod_cache_) {
        if (entity_level == level) {
            entities.push_back({entity_id, 0}); // Generation would need to be looked up
        }
    }

    return entities;
}

std::size_t LODManager::get_entity_count_by_lod(LODLevel level) const {
    std::shared_lock<std::shared_mutex> lock(lod_mutex_);

    return std::count_if(entity_lod_cache_.begin(), entity_lod_cache_.end(),
        [level](const auto& pair) { return pair.second == level; });
}

// AdvancedWorld implementation
AdvancedWorld::AdvancedWorld()
    : World()
    , entity_manager_(std::make_unique<AdvancedEntityManager>())
    , relationship_manager_(std::make_unique<EntityRelationshipManager>())
    , system_scheduler_(std::make_unique<SystemScheduler>())
    , change_notifier_(std::make_unique<ComponentChangeNotifier>())
{
    component_arrays_.reserve(MAX_COMPONENT_TYPES);
    regions_.reserve(1024);
    update_times_.reserve(max_update_samples_);

    // Enable thread safety by default
    entity_manager_->enable_thread_safety(true);
}

AdvancedWorld::~AdvancedWorld() {
    // Systems are automatically shut down by the scheduler
}

EntityHandle AdvancedWorld::create_entity() {
    auto handle = entity_manager_->create_entity();
    update_memory_usage_cache();
    return handle;
}

EntityHandle AdvancedWorld::create_entity_in_region(int32_t x, int32_t y, int32_t z) {
    auto handle = create_entity();

    // Add entity to region
    auto* region = get_region(x, y, z);
    if (region) {
        region->add_entity(handle);
    }

    return handle;
}

void AdvancedWorld::destroy_entity(EntityHandle handle) {
    if (!is_valid(handle)) {
        return;
    }

    // Remove from all regions
    {
        std::shared_lock<std::shared_mutex> lock(regions_mutex_);
        for (auto& [key, region] : regions_) {
            region->remove_entity(handle);
        }
    }

    // Remove from relationship hierarchy
    relationship_manager_->remove_parent(handle);
    auto children = relationship_manager_->get_children(handle);
    for (const auto& child : children) {
        relationship_manager_->remove_parent(child);
    }

    // Remove all components (notifications will be sent automatically)
    // This is a simplification - in a real implementation, we'd iterate through
    // all component arrays and remove the entity from each

    entity_manager_->destroy_entity(handle);
    update_memory_usage_cache();
}

bool AdvancedWorld::is_valid(EntityHandle handle) const {
    return entity_manager_->is_valid(handle);
}

void AdvancedWorld::update(float delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Compact storage periodically
    static std::size_t update_counter = 0;
    if (++update_counter % 100 == 0) {
        compact_all_storage();
    }

    // Update systems
    system_scheduler_->update_all(*this, delta_time);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Track performance
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        update_times_.push_back(execution_time);
        if (update_times_.size() > max_update_samples_) {
            update_times_.erase(update_times_.begin());
        }
    }
}

void AdvancedWorld::update_parallel(float delta_time, std::size_t thread_count) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }

    system_scheduler_->update_parallel(*this, delta_time, thread_count);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Track performance
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        update_times_.push_back(execution_time);
        if (update_times_.size() > max_update_samples_) {
            update_times_.erase(update_times_.begin());
        }
    }
}

void AdvancedWorld::set_region_size(float size) {
    std::unique_lock<std::shared_mutex> lock(regions_mutex_);
    region_size_ = size;
}

void AdvancedWorld::set_active_region_bounds(const float* min_bounds, const float* max_bounds) {
    std::unique_lock<std::shared_mutex> lock(regions_mutex_);
    std::copy(min_bounds, min_bounds + 3, active_min_bounds_.begin());
    std::copy(max_bounds, max_bounds + 3, active_max_bounds_.begin());

    cleanup_inactive_regions();
}

WorldRegion* AdvancedWorld::get_region(int32_t x, int32_t y, int32_t z) {
    uint64_t key = region_key(x, y, z);

    std::unique_lock<std::shared_mutex> lock(regions_mutex_);
    auto it = regions_.find(key);
    if (it == regions_.end()) {
        auto region = std::make_unique<WorldRegion>(x, y, z, region_size_);
        auto* raw_ptr = region.get();
        regions_[key] = std::move(region);
        return raw_ptr;
    }

    return it->second.get();
}

std::vector<WorldRegion*> AdvancedWorld::get_active_regions() const {
    std::shared_lock<std::shared_mutex> lock(regions_mutex_);

    std::vector<WorldRegion*> active_regions;
    for (const auto& [key, region] : regions_) {
        if (region->is_active()) {
            active_regions.push_back(region.get());
        }
    }

    return active_regions;
}

void AdvancedWorld::compact_all_storage() {
    entity_manager_->compact_storage();

    std::shared_lock<std::shared_mutex> lock(regions_mutex_);
    for (auto& [key, region] : regions_) {
        region->compact_storage();
    }

    update_memory_usage_cache();
}

void AdvancedWorld::set_memory_budget(std::size_t bytes) {
    memory_budget_.store(bytes, std::memory_order_relaxed);
}

std::size_t AdvancedWorld::get_memory_usage() const {
    return memory_usage_cache_.load(std::memory_order_relaxed);
}

AdvancedWorld::PerformanceStats AdvancedWorld::get_performance_stats() const {
    PerformanceStats stats;

    stats.total_entities = MAX_ENTITIES;
    stats.active_entities = entity_manager_->get_entity_count();
    stats.systems_count = system_scheduler_->get_system_performance().size();
    stats.memory_usage_bytes = get_memory_usage();

    // Calculate memory fragmentation
    std::size_t free_entities = entity_manager_->get_free_entity_count();
    if (stats.total_entities > 0) {
        stats.memory_fragmentation = static_cast<float>(free_entities) / stats.total_entities;
    } else {
        stats.memory_fragmentation = 0.0f;
    }

    // Calculate timing statistics
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        if (!update_times_.empty()) {
            stats.last_update_time = update_times_.back();

            auto sum = std::accumulate(update_times_.begin(), update_times_.end(),
                std::chrono::microseconds(0));
            stats.average_update_time = sum / update_times_.size();
        } else {
            stats.last_update_time = std::chrono::microseconds(0);
            stats.average_update_time = std::chrono::microseconds(0);
        }
    }

    return stats;
}

std::size_t AdvancedWorld::get_entity_count() const noexcept {
    return entity_manager_->get_entity_count();
}

std::size_t AdvancedWorld::get_component_type_count() const noexcept {
    return ComponentRegistry::instance().get_component_count();
}

std::size_t AdvancedWorld::get_active_region_count() const noexcept {
    return get_active_regions().size();
}

uint64_t AdvancedWorld::region_key(int32_t x, int32_t y, int32_t z) const {
    // Pack 3D coordinates into a 64-bit key
    uint64_t key = 0;
    key |= (static_cast<uint64_t>(static_cast<uint32_t>(x)) & 0x1FFFFF); // 21 bits
    key |= (static_cast<uint64_t>(static_cast<uint32_t>(y)) & 0x1FFFFF) << 21; // 21 bits
    key |= (static_cast<uint64_t>(static_cast<uint32_t>(z)) & 0x3FFFFF) << 42; // 22 bits
    return key;
}

void AdvancedWorld::update_memory_usage_cache() const {
    std::size_t usage = sizeof(*this);
    usage += entity_manager_->get_memory_usage();

    // Add component arrays memory
    std::shared_lock<std::shared_mutex> lock(world_mutex_);
    for (const auto& [id, array_ptr] : component_arrays_) {
        if (array_ptr) {
            // Calculate actual memory usage for component arrays
            const auto& component_info = ComponentRegistry::instance().get_component_info(id);

            // Since array_ptr is void*, we need to cast it to a known type to get size
            // This is a simplified approach - in a real implementation, we'd store size separately
            std::size_t component_count = 0;

            // Try to estimate component count based on memory allocation
            // This is an approximation since we can't directly access the size from void*
            if (component_info.size > 0) {
                // Estimate based on typical component array growth patterns
                component_count = 100; // Default estimate
            }

            // Memory for component data
            usage += component_count * component_info.size;

            // Memory for sparse array (entity ID mappings)
            usage += component_count * sizeof(std::uint32_t);

            // Memory for dense arrays (entities and components)
            usage += component_count * sizeof(Entity);

            // Add overhead for vector capacity vs size
            usage += (component_count * component_info.size) / 4; // ~25% overhead estimate
        }
    }

    // Add regions memory
    {
        std::shared_lock<std::shared_mutex> regions_lock(regions_mutex_);
        for (const auto& [key, region] : regions_) {
            usage += region->get_memory_usage();
        }
    }

    memory_usage_cache_.store(usage, std::memory_order_relaxed);
}

void AdvancedWorld::cleanup_inactive_regions() {
    // Remove regions outside active bounds
    auto it = regions_.begin();
    while (it != regions_.end()) {
        const auto& region = it->second;
        float region_x = region->get_x() * region_size_;
        float region_y = region->get_y() * region_size_;
        float region_z = region->get_z() * region_size_;

        bool is_active = (region_x >= active_min_bounds_[0] && region_x <= active_max_bounds_[0] &&
                         region_y >= active_min_bounds_[1] && region_y <= active_max_bounds_[1] &&
                         region_z >= active_min_bounds_[2] && region_z <= active_max_bounds_[2]);

        if (!is_active && region->get_entities().empty()) {
            it = regions_.erase(it);
        } else {
            region->set_active(is_active);
            ++it;
        }
    }
}

// ComponentChangeNotifier implementation
ComponentChangeNotifier::ComponentChangeNotifier() {
    callbacks_.reserve(256);
}

ComponentChangeNotifier::~ComponentChangeNotifier() = default;

std::size_t ComponentChangeNotifier::register_callback(ComponentID component_id, ChangeCallback callback) {
    std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);

    std::size_t id = next_callback_id_.fetch_add(1, std::memory_order_relaxed);
    callbacks_.push_back({id, component_id, std::move(callback)});

    return id;
}

std::size_t ComponentChangeNotifier::register_global_callback(ChangeCallback callback) {
    return register_callback(INVALID_ENTITY, std::move(callback));
}

void ComponentChangeNotifier::unregister_callback(std::size_t callback_id) {
    std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);

    callbacks_.erase(
        std::remove_if(callbacks_.begin(), callbacks_.end(),
            [callback_id](const CallbackInfo& info) { return info.id == callback_id; }),
        callbacks_.end());
}

void ComponentChangeNotifier::notify_component_added(EntityHandle entity, ComponentID component_id) {
    internal_notify(entity, component_id, ChangeType::Added);
}

void ComponentChangeNotifier::notify_component_modified(EntityHandle entity, ComponentID component_id) {
    internal_notify(entity, component_id, ChangeType::Modified);
}

void ComponentChangeNotifier::notify_component_removed(EntityHandle entity, ComponentID component_id) {
    internal_notify(entity, component_id, ChangeType::Removed);
}

void ComponentChangeNotifier::begin_batch() {
    batch_mode_.store(true, std::memory_order_relaxed);
}

void ComponentChangeNotifier::end_batch() {
    batch_mode_.store(false, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(batch_mutex_);
    for (const auto& [entity, component_id, type] : batched_changes_) {
        internal_notify(entity, component_id, type);
    }
    batched_changes_.clear();
}

void ComponentChangeNotifier::internal_notify(EntityHandle entity, ComponentID component_id, ChangeType type) {
    if (batch_mode_.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batched_changes_.emplace_back(entity, component_id, type);
        return;
    }

    std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
    for (const auto& callback_info : callbacks_) {
        // Call if it's a global callback or matches the component type
        if (callback_info.component_id == INVALID_ENTITY || callback_info.component_id == component_id) {
            try {
                callback_info.callback(entity, component_id, type);
            } catch (const std::exception& e) {
                std::cerr << "Exception in component change callback: " << e.what() << std::endl;
            }
        }
    }
}

// EntityQuery implementation
EntityQuery::EntityQuery() = default;
EntityQuery::~EntityQuery() = default;

EntityQuery& EntityQuery::in_region(int32_t x, int32_t y, int32_t z) {
    region_filter_ = std::make_tuple(x, y, z);
    invalidate_cache();
    return *this;
}

EntityQuery& EntityQuery::in_lod_level(LODManager::LODLevel level) {
    lod_filter_ = level;
    invalidate_cache();
    return *this;
}

EntityQuery& EntityQuery::with_relationship(EntityHandle target, bool is_parent) {
    relationship_filter_ = std::make_pair(target, is_parent);
    invalidate_cache();
    return *this;
}

std::vector<EntityHandle> EntityQuery::execute(const AdvancedWorld& world) const {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<EntityHandle> results;

    // Iterate through all entities
    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;
        if (matches_entity(world, entity)) {
            results.push_back(entity);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    last_execution_time_ = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    return results;
}

void EntityQuery::execute_foreach(const AdvancedWorld& world, std::function<void(EntityHandle)> callback) const {
    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;
        if (matches_entity(world, entity)) {
            callback(entity);
        }
    }
}

void EntityQuery::cache_results(const AdvancedWorld& world) {
    std::lock_guard<std::mutex> lock(query_mutex_);
    cached_results_ = execute(world);
    cache_valid_ = true;
}

const std::vector<EntityHandle>& EntityQuery::get_cached_results() const {
    std::lock_guard<std::mutex> lock(query_mutex_);
    if (!cache_valid_) {
        throw std::runtime_error("Query cache is not valid. Call cache_results() first.");
    }
    return cached_results_;
}

void EntityQuery::invalidate_cache() {
    std::lock_guard<std::mutex> lock(query_mutex_);
    cache_valid_ = false;
}

bool EntityQuery::is_cache_valid() const {
    std::lock_guard<std::mutex> lock(query_mutex_);
    return cache_valid_;
}

std::size_t EntityQuery::get_result_count(const AdvancedWorld& world) const {
    return execute(world).size();
}

std::chrono::microseconds EntityQuery::get_last_execution_time() const {
    return last_execution_time_;
}

bool EntityQuery::matches_entity(const AdvancedWorld& world, EntityHandle entity) const {
    // Check required components
    for ([[maybe_unused]] ComponentID component_id : required_components_) {
        // This is a simplification - we'd need to check each component type
        // In a real implementation, we'd have a way to check if an entity has a component by ID
    }

    // Check excluded components
    for ([[maybe_unused]] ComponentID component_id : excluded_components_) {
        // Similar to above
    }

    // Check region filter
    if (region_filter_.has_value()) {
        auto [x, y, z] = region_filter_.value();
        auto* region = const_cast<AdvancedWorld&>(world).get_region(x, y, z);
        if (!region || !region->contains_entity(entity)) {
            return false;
        }
    }

    // Check LOD filter
    if (lod_filter_.has_value()) {
        auto current_lod = world.lod_manager_.calculate_lod(entity, nullptr);
        if (current_lod != lod_filter_.value()) {
            return false;
        }
    }

    // Check relationship filter
    if (relationship_filter_.has_value()) {
        auto [target, is_parent] = relationship_filter_.value();
        if (is_parent) {
            auto parent = world.relationship_manager_->get_parent(entity);
            if (parent != target) {
                return false;
            }
        } else {
            auto children = world.relationship_manager_->get_children(entity);
            if (std::find(children.begin(), children.end(), target) == children.end()) {
                return false;
            }
        }
    }

    return true;
}

// SystemScheduler implementation
SystemScheduler::SystemScheduler() {
    systems_.reserve(64);
    execution_order_.reserve(64);
}

SystemScheduler::~SystemScheduler() = default;

void SystemScheduler::clear_dependencies() {
    dependencies_.clear();
    compute_execution_order();
}

void SystemScheduler::update_all(AdvancedWorld& world, float delta_time) {
    for (const auto& system_type : execution_order_) {
        auto it = systems_.find(system_type);
        if (it != systems_.end()) {
            auto start_time = std::chrono::high_resolution_clock::now();

            it->second->update(world, delta_time);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

            update_system_performance(system_type, execution_time);
        }
    }
}

void SystemScheduler::update_parallel(AdvancedWorld& world, float delta_time, std::size_t thread_count) {
    // Simple parallel execution - in a real implementation, this would be more sophisticated
    // We'd group systems that don't have dependencies and run them in parallel

    std::vector<std::future<void>> futures;
    futures.reserve(thread_count);

    std::size_t systems_per_thread = execution_order_.size() / thread_count;
    if (systems_per_thread == 0) systems_per_thread = 1;

    for (std::size_t i = 0; i < thread_count && i * systems_per_thread < execution_order_.size(); ++i) {
        std::size_t start_idx = i * systems_per_thread;
        std::size_t end_idx = std::min((i + 1) * systems_per_thread, execution_order_.size());

        futures.push_back(std::async(std::launch::async, [this, &world, delta_time, start_idx, end_idx]() {
            for (std::size_t j = start_idx; j < end_idx; ++j) {
                const auto& system_type = execution_order_[j];
                auto it = systems_.find(system_type);
                if (it != systems_.end()) {
                    auto start_time = std::chrono::high_resolution_clock::now();

                    it->second->update(world, delta_time);

                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

                    update_system_performance(system_type, execution_time);
                }
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
}

void SystemScheduler::create_system_group(const std::string& name, const std::vector<std::type_index>& system_types) {
    system_groups_[name] = system_types;
}

void SystemScheduler::execute_system_group(const std::string& name, AdvancedWorld& world, float delta_time) {
    auto it = system_groups_.find(name);
    if (it == system_groups_.end()) {
        return;
    }

    for (const auto& system_type : it->second) {
        auto system_it = systems_.find(system_type);
        if (system_it != systems_.end()) {
            auto start_time = std::chrono::high_resolution_clock::now();

            system_it->second->update(world, delta_time);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

            update_system_performance(system_type, execution_time);
        }
    }
}

std::vector<SystemScheduler::SystemPerformance> SystemScheduler::get_system_performance() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);

    std::vector<SystemPerformance> performance_list;
    performance_list.reserve(performance_data_.size());

    for (const auto& [type, perf] : performance_data_) {
        performance_list.push_back(perf);
    }

    return performance_list;
}

void SystemScheduler::compute_execution_order() {
    execution_order_.clear();

    // Topological sort to determine execution order
    std::unordered_map<std::type_index, int> in_degree;
    std::unordered_set<std::type_index> all_systems;

    // Initialize in-degree
    for (const auto& [system_type, system] : systems_) {
        in_degree[system_type] = 0;
        all_systems.insert(system_type);
    }

    // Calculate in-degrees
    for (const auto& [system_type, deps] : dependencies_) {
        in_degree[system_type] = static_cast<int>(deps.size());
        for (const auto& dep : deps) {
            all_systems.insert(dep);
        }
    }

    // Kahn's algorithm for topological sorting
    std::queue<std::type_index> zero_in_degree;
    for (const auto& system_type : all_systems) {
        if (systems_.find(system_type) != systems_.end() && in_degree[system_type] == 0) {
            zero_in_degree.push(system_type);
        }
    }

    while (!zero_in_degree.empty()) {
        auto current = zero_in_degree.front();
        zero_in_degree.pop();

        execution_order_.push_back(current);

        // Update in-degrees of dependent systems
        for (const auto& [system_type, deps] : dependencies_) {
            if (std::find(deps.begin(), deps.end(), current) != deps.end()) {
                in_degree[system_type]--;
                if (in_degree[system_type] == 0 && systems_.find(system_type) != systems_.end()) {
                    zero_in_degree.push(system_type);
                }
            }
        }
    }

    // Check for cycles
    if (execution_order_.size() != systems_.size()) {
        throw std::runtime_error("Circular dependency detected in system dependencies");
    }
}

void SystemScheduler::update_system_performance(const std::type_index& type, std::chrono::microseconds execution_time) {
    std::lock_guard<std::mutex> lock(performance_mutex_);

    auto& perf = performance_data_[type];
    perf.last_execution_time = execution_time;
    perf.execution_count++;

    // Update average (simple moving average for now)
    if (perf.execution_count == 1) {
        perf.average_execution_time = execution_time;
    } else {
        auto total_time = perf.average_execution_time * (perf.execution_count - 1) + execution_time;
        perf.average_execution_time = total_time / perf.execution_count;
    }
}

} // namespace lore::ecs