#pragma once

#include <algorithm>
#include <execution>

namespace lore::ecs {

// AdvancedWorld template implementations
template<typename T>
void AdvancedWorld::add_component(EntityHandle handle, T component) {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    ComponentID component_id = ComponentRegistry::instance().register_component<T>();
    auto& array = get_or_create_component_array<T>();

    array.add_component(handle.id, std::move(component));

    // Notify change listeners
    change_notifier_->notify_component_added(handle, component_id);
}

template<typename T>
void AdvancedWorld::remove_component(EntityHandle handle) {
    if (!is_valid(handle)) {
        return;
    }

    ComponentID component_id = ComponentRegistry::instance().get_component_id<T>();

    try {
        auto& array = get_component_array<T>();
        array.remove_component(handle.id);

        // Notify change listeners
        change_notifier_->notify_component_removed(handle, component_id);
    } catch (const std::exception&) {
        // Component array doesn't exist or entity doesn't have component
    }
}

template<typename T>
T& AdvancedWorld::get_component(EntityHandle handle) {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    return get_component_array<T>().get_component(handle.id);
}

template<typename T>
const T& AdvancedWorld::get_component(EntityHandle handle) const {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    return get_component_array<T>().get_component(handle.id);
}

template<typename T>
bool AdvancedWorld::has_component(EntityHandle handle) const {
    if (!is_valid(handle)) {
        return false;
    }

    try {
        const auto& array = get_component_array<T>();
        return array.has_component(handle.id);
    } catch (const std::exception&) {
        return false;
    }
}

template<typename T>
void AdvancedWorld::add_components_batch(const std::vector<EntityHandle>& entities, const std::vector<T>& components) {
    if (entities.size() != components.size()) {
        throw std::invalid_argument("Entity and component vectors must have the same size");
    }

    ComponentID component_id = ComponentRegistry::instance().register_component<T>();
    auto& array = get_or_create_component_array<T>();

    change_notifier_->begin_batch();

    for (std::size_t i = 0; i < entities.size(); ++i) {
        if (is_valid(entities[i])) {
            array.add_component(entities[i].id, components[i]);
            change_notifier_->notify_component_added(entities[i], component_id);
        }
    }

    change_notifier_->end_batch();
}

template<typename T>
void AdvancedWorld::remove_components_batch(const std::vector<EntityHandle>& entities) {
    ComponentID component_id = ComponentRegistry::instance().get_component_id<T>();

    try {
        auto& array = get_component_array<T>();
        change_notifier_->begin_batch();

        for (const auto& entity : entities) {
            if (is_valid(entity) && array.has_component(entity.id)) {
                array.remove_component(entity.id);
                change_notifier_->notify_component_removed(entity, component_id);
            }
        }

        change_notifier_->end_batch();
    } catch (const std::exception&) {
        // Component array doesn't exist
    }
}

template<typename T>
ComponentArray<T>& AdvancedWorld::get_component_array() {
    return get_or_create_component_array<T>();
}

template<typename T>
const ComponentArray<T>& AdvancedWorld::get_component_array() const {
    ComponentID id = ComponentRegistry::instance().get_component_id<T>();

    std::shared_lock<std::shared_mutex> lock(world_mutex_);
    auto it = component_arrays_.find(id);
    if (it == component_arrays_.end()) {
        throw std::runtime_error("Component array does not exist");
    }

    return *static_cast<const ComponentArray<T>*>(it->second.get());
}

template<typename T>
ComponentArray<T>& AdvancedWorld::get_or_create_component_array() {
    ComponentID id = ComponentRegistry::instance().register_component<T>();

    std::unique_lock<std::shared_mutex> lock(world_mutex_);
    auto it = component_arrays_.find(id);
    if (it == component_arrays_.end()) {
        auto array = std::make_unique<ComponentArray<T>>();
        auto* raw_ptr = array.get();

        auto deleter = [](void* ptr) noexcept {
            delete static_cast<ComponentArray<T>*>(ptr);
        };

        component_arrays_.emplace(std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(array.release(), deleter));

        return *raw_ptr;
    }

    return *static_cast<ComponentArray<T>*>(it->second.get());
}

template<typename T, typename... Args>
T& AdvancedWorld::add_system(Args&&... args) {
    return system_scheduler_->add_system<T>(std::forward<Args>(args)...);
}

template<typename T>
T& AdvancedWorld::get_system() {
    return system_scheduler_->get_system<T>();
}

template<typename T>
void AdvancedWorld::remove_system() {
    system_scheduler_->remove_system<T>();
}

template<typename... ComponentTypes>
std::unique_ptr<EntityQuery> AdvancedWorld::create_query() {
    auto query = std::make_unique<EntityQuery>();

    // Use fold expression to register all component types
    (query->with<ComponentTypes>(), ...);

    return query;
}

// EntityQuery template implementations
template<typename T>
EntityQuery& EntityQuery::with() {
    ComponentID component_id = ComponentRegistry::instance().register_component<T>();

    if (std::find(required_components_.begin(), required_components_.end(), component_id) == required_components_.end()) {
        required_components_.push_back(component_id);
        invalidate_cache();
    }

    return *this;
}

template<typename T>
EntityQuery& EntityQuery::without() {
    ComponentID component_id = ComponentRegistry::instance().register_component<T>();

    if (std::find(excluded_components_.begin(), excluded_components_.end(), component_id) == excluded_components_.end()) {
        excluded_components_.push_back(component_id);
        invalidate_cache();
    }

    return *this;
}

// SystemScheduler template implementations
template<typename T, typename... Args>
T& SystemScheduler::add_system(Args&&... args) {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it != systems_.end()) {
        throw std::runtime_error("System already exists");
    }

    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T& system_ref = *system;

    systems_[type] = std::move(system);

    // Initialize performance tracking
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        performance_data_[type] = SystemPerformance{
            typeid(T).name(),
            std::chrono::microseconds(0),
            std::chrono::microseconds(0),
            0
        };
    }

    compute_execution_order();

    return system_ref;
}

template<typename T>
T& SystemScheduler::get_system() {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it == systems_.end()) {
        throw std::runtime_error("System not found");
    }

    return *static_cast<T*>(it->second.get());
}

template<typename T>
void SystemScheduler::remove_system() {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it == systems_.end()) {
        return;
    }

    systems_.erase(it);

    // Remove from performance tracking
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        performance_data_.erase(type);
    }

    // Remove dependencies
    dependencies_.erase(type);
    for (auto& [system_type, deps] : dependencies_) {
        deps.erase(std::remove(deps.begin(), deps.end(), type), deps.end());
    }

    compute_execution_order();
}

template<typename Before, typename After>
void SystemScheduler::add_dependency() {
    static_assert(std::is_base_of_v<System, Before>, "Before must inherit from System");
    static_assert(std::is_base_of_v<System, After>, "After must inherit from System");

    const std::type_index before_type = std::type_index(typeid(Before));
    const std::type_index after_type = std::type_index(typeid(After));

    // After depends on Before (Before must run before After)
    dependencies_[after_type].push_back(before_type);

    compute_execution_order();
}

} // namespace lore::ecs