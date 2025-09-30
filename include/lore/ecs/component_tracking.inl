#pragma once

#include <algorithm>
#include <execution>
#include <immintrin.h>

// Include for AdvancedWorld definition (needed for template implementations)
#include <lore/ecs/world_manager.hpp>

namespace lore::ecs {

// ComponentDependencyManager template implementations
template<typename Dependent, typename Dependency>
void ComponentDependencyManager::add_dependency() {
    ComponentID dependent_id = ComponentRegistry::instance().register_component<Dependent>();
    ComponentID dependency_id = ComponentRegistry::instance().register_component<Dependency>();

    internal_add_dependency(dependent_id, dependency_id);
}

template<typename Dependent>
void ComponentDependencyManager::remove_dependencies() {
    ComponentID dependent_id = ComponentRegistry::instance().get_component_id<Dependent>();
    internal_remove_dependencies(dependent_id);
}

template<typename T>
std::vector<ComponentID> ComponentDependencyManager::get_dependencies() const {
    ComponentID component_id = ComponentRegistry::instance().get_component_id<T>();

    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);
    auto it = dependencies_.find(component_id);
    if (it != dependencies_.end()) {
        return std::vector<ComponentID>(it->second.begin(), it->second.end());
    }
    return {};
}

template<typename T>
std::vector<ComponentID> ComponentDependencyManager::get_dependents() const {
    ComponentID component_id = ComponentRegistry::instance().get_component_id<T>();

    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);
    auto it = dependents_.find(component_id);
    if (it != dependents_.end()) {
        return std::vector<ComponentID>(it->second.begin(), it->second.end());
    }
    return {};
}

template<typename T>
bool ComponentDependencyManager::has_dependencies() const {
    ComponentID component_id = ComponentRegistry::instance().get_component_id<T>();

    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);
    auto it = dependencies_.find(component_id);
    return it != dependencies_.end() && !it->second.empty();
}

template<typename T>
bool ComponentDependencyManager::is_dependency_of(ComponentID component_id) const {
    ComponentID dependency_id = ComponentRegistry::instance().get_component_id<T>();

    std::shared_lock<std::shared_mutex> lock(dependency_mutex_);
    auto it = dependencies_.find(component_id);
    return it != dependencies_.end() && it->second.find(dependency_id) != it->second.end();
}

// TypedQuery template implementations
template<typename... RequiredComponents>
TypedQuery<RequiredComponents...>::TypedQuery() {
    build_component_lists();
}

template<typename... RequiredComponents>
TypedQuery<RequiredComponents...>::~TypedQuery() = default;

template<typename... RequiredComponents>
template<typename... ExcludedComponents>
TypedQuery<RequiredComponents...>& TypedQuery<RequiredComponents...>::without() {
    auto add_excluded = [this]<typename T>() {
        ComponentID component_id = ComponentRegistry::instance().register_component<T>();
        if (std::find(excluded_components_.begin(), excluded_components_.end(), component_id) == excluded_components_.end()) {
            excluded_components_.push_back(component_id);
        }
    };

    (add_excluded.template operator()<ExcludedComponents>(), ...);
    invalidate_cache();
    return *this;
}

template<typename... RequiredComponents>
TypedQuery<RequiredComponents...>& TypedQuery<RequiredComponents...>::in_archetype(std::shared_ptr<ComponentArchetype> archetype) {
    archetype_filter_ = archetype;
    invalidate_cache();
    return *this;
}

template<typename... RequiredComponents>
TypedQuery<RequiredComponents...>& TypedQuery<RequiredComponents...>::with_relationship(EntityHandle target, bool is_parent) {
    relationship_filter_ = std::make_pair(target, is_parent);
    invalidate_cache();
    return *this;
}

template<typename... RequiredComponents>
template<typename Func>
void TypedQuery<RequiredComponents...>::for_each(const AdvancedWorld& world, Func&& func) const {
    if (caching_enabled_ && cache_valid_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        for (EntityHandle entity : cached_results_) {
            if (world.is_valid(entity)) {
                func(entity, const_cast<RequiredComponents&>(world.get_component<RequiredComponents>(entity))...);
            }
        }
        return;
    }

    // Direct iteration without caching
    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;

        // Check if entity has all required components
        bool has_all_components = (world.has_component<RequiredComponents>(entity) && ...);
        if (!has_all_components) {
            continue;
        }

        // Check excluded components
        bool has_excluded = false;
        for ([[maybe_unused]] ComponentID excluded_id : excluded_components_) {
            // We'd need a way to check by ComponentID - simplified for now
        }
        if (has_excluded) {
            continue;
        }

        if (matches_filters(world, entity)) {
            func(entity, const_cast<RequiredComponents&>(world.get_component<RequiredComponents>(entity))...);
        }
    }
}

template<typename... RequiredComponents>
template<typename Func>
void TypedQuery<RequiredComponents...>::for_each_parallel(const AdvancedWorld& world, Func&& func, std::size_t thread_count) const {
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }

    auto entities = collect(world);

    // Parallel execution using std::execution
    std::for_each(std::execution::par_unseq, entities.begin(), entities.end(),
        [&world, &func](EntityHandle entity) {
            if (world.is_valid(entity)) {
                func(entity, const_cast<RequiredComponents&>(world.get_component<RequiredComponents>(entity))...);
            }
        });
}

template<typename... RequiredComponents>
std::vector<EntityHandle> TypedQuery<RequiredComponents...>::collect(const AdvancedWorld& world) const {
    if (caching_enabled_ && cache_valid_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return cached_results_;
    }

    std::vector<EntityHandle> results;

    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;

        bool has_all_components = (world.has_component<RequiredComponents>(entity) && ...);
        if (has_all_components && matches_filters(world, entity)) {
            results.push_back(entity);
        }
    }

    if (caching_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cached_results_ = results;
        cache_valid_ = true;
    }

    return results;
}

template<typename... RequiredComponents>
std::size_t TypedQuery<RequiredComponents...>::count(const AdvancedWorld& world) const {
    return collect(world).size();
}

template<typename... RequiredComponents>
void TypedQuery<RequiredComponents...>::enable_caching(bool enable) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    caching_enabled_ = enable;
    if (!enable) {
        cache_valid_ = false;
        cached_results_.clear();
    }
}

template<typename... RequiredComponents>
void TypedQuery<RequiredComponents...>::invalidate_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_valid_ = false;
}

template<typename... RequiredComponents>
bool TypedQuery<RequiredComponents...>::is_cached() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return caching_enabled_ && cache_valid_;
}

template<typename... RequiredComponents>
void TypedQuery<RequiredComponents...>::build_component_lists() {
    auto add_required = [this]<typename T>() {
        ComponentID component_id = ComponentRegistry::instance().register_component<T>();
        required_components_.push_back(component_id);
    };

    (add_required.template operator()<RequiredComponents>(), ...);
}

template<typename... RequiredComponents>
bool TypedQuery<RequiredComponents...>::matches_filters(const AdvancedWorld& world, EntityHandle entity) const {
    // Check archetype filter
    if (archetype_filter_ && !archetype_filter_->contains_entity(entity)) {
        return false;
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

// ComponentChangeTracker template implementations
template<typename ComponentType>
void ComponentChangeTracker::register_reactive_system(std::shared_ptr<ReactiveSystem> system) {
    ComponentID component_id = ComponentRegistry::instance().register_component<ComponentType>();

    std::unique_lock<std::shared_mutex> lock(tracker_mutex_);
    reactive_systems_[component_id].push_back(system);
}

// ReactiveSystem template implementations
template<typename ComponentType>
void ReactiveSystem::watch_component_added() {
    ComponentID component_id = ComponentRegistry::instance().register_component<ComponentType>();
    watched_added_.insert(component_id);
}

template<typename ComponentType>
void ReactiveSystem::watch_component_modified() {
    ComponentID component_id = ComponentRegistry::instance().register_component<ComponentType>();
    watched_modified_.insert(component_id);
}

template<typename ComponentType>
void ReactiveSystem::watch_component_removed() {
    ComponentID component_id = ComponentRegistry::instance().register_component<ComponentType>();
    watched_removed_.insert(component_id);
}

// SIMDQuery template implementations
template<typename... Components>
SIMDQuery& SIMDQuery::with() {
    auto add_component = [this]<typename T>() {
        ComponentID component_id = ComponentRegistry::instance().register_component<T>();
        if (std::find(required_components_.begin(), required_components_.end(), component_id) == required_components_.end()) {
            required_components_.push_back(component_id);
        }
    };

    (add_component.template operator()<Components>(), ...);
    return *this;
}

template<typename... Components>
SIMDQuery& SIMDQuery::without() {
    auto add_excluded = [this]<typename T>() {
        ComponentID component_id = ComponentRegistry::instance().register_component<T>();
        if (std::find(excluded_components_.begin(), excluded_components_.end(), component_id) == excluded_components_.end()) {
            excluded_components_.push_back(component_id);
        }
    };

    (add_excluded.template operator()<Components>(), ...);
    return *this;
}

template<typename Func>
void SIMDQuery::for_each_simd(const AdvancedWorld& world, Func&& func) const {
    if (!is_simd_compatible()) {
        // Fall back to regular iteration
        auto entities = get_matching_entities_simd(world);
        for (EntityHandle entity : entities) {
            func(entity);
        }
        return;
    }

    auto entities = get_matching_entities_simd(world);

    // Process in SIMD-friendly batches
    for (std::size_t i = 0; i < entities.size(); i += BATCH_SIZE) {
        std::size_t batch_end = std::min(i + BATCH_SIZE, entities.size());

        // Process batch with SIMD optimization
        for (std::size_t j = i; j < batch_end; ++j) {
            func(entities[j]);
        }
    }
}

template<typename ComponentType>
void SIMDQuery::process_components_vectorized(const AdvancedWorld& world,
                                            std::function<void(ComponentType*, std::size_t)> processor) const {
    static_assert(std::is_trivially_copyable_v<ComponentType>, "Component must be trivially copyable for SIMD processing");

    auto entities = get_matching_entities_simd(world);
    if (entities.empty()) {
        return;
    }

    // Get component array
    const auto& component_array = world.get_component_array<ComponentType>();

    // Process components in vectorized batches
    std::vector<ComponentType*> component_ptrs;
    component_ptrs.reserve(BATCH_SIZE);

    for (std::size_t i = 0; i < entities.size(); i += BATCH_SIZE) {
        component_ptrs.clear();

        std::size_t batch_end = std::min(i + BATCH_SIZE, entities.size());
        for (std::size_t j = i; j < batch_end; ++j) {
            if (world.has_component<ComponentType>(entities[j])) {
                component_ptrs.push_back(const_cast<ComponentType*>(&world.get_component<ComponentType>(entities[j])));
            }
        }

        if (!component_ptrs.empty()) {
            // Align components for SIMD processing
            if (reinterpret_cast<uintptr_t>(component_ptrs[0]) % SIMD_ALIGNMENT == 0) {
                processor(component_ptrs[0], component_ptrs.size());
            } else {
                // Process individually if not aligned
                for (ComponentType* comp : component_ptrs) {
                    processor(comp, 1);
                }
            }
        }
    }
}

template<typename ComponentType>
void SIMDQuery::iterate_aligned(const AdvancedWorld& world,
                               std::function<void(const ComponentType&, EntityHandle)> callback) const {
    auto entities = get_matching_entities_simd(world);

    for (EntityHandle entity : entities) {
        if (world.has_component<ComponentType>(entity)) {
            const auto& component = world.get_component<ComponentType>(entity);
            callback(component, entity);
        }
    }
}

// ComponentPoolManager template implementations
template<typename T>
ComponentMemoryPool& ComponentPoolManager::get_pool() {
    std::type_index type_id = std::type_index(typeid(T));

    std::lock_guard<std::mutex> lock(pools_mutex_);
    auto it = pools_.find(type_id);
    if (it == pools_.end()) {
        auto pool = std::make_unique<ComponentMemoryPool>(
            sizeof(T),
            alignof(T),
            1024  // Default initial capacity
        );
        auto* raw_ptr = pool.get();
        pools_[type_id] = std::move(pool);
        return *raw_ptr;
    }

    return *it->second;
}

template<typename T>
void ComponentPoolManager::configure_pool(std::size_t initial_capacity) {
    std::type_index type_id = std::type_index(typeid(T));

    std::lock_guard<std::mutex> lock(pools_mutex_);
    auto pool = std::make_unique<ComponentMemoryPool>(
        sizeof(T),
        alignof(T),
        initial_capacity
    );
    pools_[type_id] = std::move(pool);
}

} // namespace lore::ecs