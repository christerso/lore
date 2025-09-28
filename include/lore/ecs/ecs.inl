#pragma once

#include <stdexcept>
#include <algorithm>
#include <immintrin.h>
#include <limits>

namespace lore::ecs {

// ComponentRegistry template implementations
template<typename T>
ComponentID ComponentRegistry::register_component() {
    const std::type_index type = std::type_index(typeid(T));

    auto it = type_to_id_.find(type);
    if (it != type_to_id_.end()) {
        return it->second;
    }

    if (next_id_ >= MAX_COMPONENT_TYPES) {
        throw std::runtime_error("Maximum number of component types exceeded");
    }

    ComponentID id = next_id_++;

    ComponentInfo info;
    info.id = id;
    info.size = sizeof(T);
    info.alignment = alignof(T);
    info.name = typeid(T).name();
    info.destructor = [](void* data) {
        (void)data; // Suppress unused parameter warning
        if constexpr (!std::is_trivially_destructible_v<T>) {
            static_cast<T*>(data)->~T();
        }
    };

    type_to_id_[type] = id;

    if (components_.size() <= id) {
        components_.resize(id + 1);
    }
    components_[id] = std::move(info);

    return id;
}

template<typename T>
ComponentID ComponentRegistry::get_component_id() const {
    std::type_index type = std::type_index(typeid(T));
    auto it = type_to_id_.find(type);
    if (it == type_to_id_.end()) {
        throw std::runtime_error("Component type not registered");
    }
    return it->second;
}

// ComponentArray template implementations
template<typename T>
ComponentArray<T>::ComponentArray() {
    // Pre-allocate for performance with cache-friendly sizes
    sparse_.reserve(MAX_ENTITIES);
    dense_entities_.reserve(1024);
    dense_components_.reserve(1024);

    // Initialize sparse array with invalid indices
    sparse_.resize(MAX_ENTITIES, std::numeric_limits<std::uint32_t>::max());
}

template<typename T>
ComponentArray<T>::~ComponentArray() {
    clear();
}

template<typename T>
void ComponentArray<T>::add_component(Entity entity, T component) {
    if (entity >= MAX_ENTITIES) {
        throw std::out_of_range("Entity ID exceeds maximum allowed entities");
    }

    if (has_component(entity)) {
        throw std::runtime_error("Entity already has this component type");
    }

    std::uint32_t dense_index = static_cast<std::uint32_t>(dense_entities_.size());

    // Resize sparse array if needed
    if (sparse_.size() <= entity) {
        sparse_.resize(entity + 1, std::numeric_limits<std::uint32_t>::max());
    }

    sparse_[entity] = dense_index;
    dense_entities_.push_back(entity);
    dense_components_.emplace_back(std::move(component));
}

template<typename T>
void ComponentArray<T>::remove_component(Entity entity) {
    if (!has_component(entity)) {
        return; // Silently ignore removal of non-existent component
    }

    std::uint32_t dense_index = sparse_[entity];
    std::uint32_t last_index = static_cast<std::uint32_t>(dense_entities_.size() - 1);

    // Swap with last element for O(1) removal
    if (dense_index != last_index) {
        Entity last_entity = dense_entities_[last_index];

        // Move last element to the removed position
        dense_entities_[dense_index] = last_entity;
        dense_components_[dense_index] = std::move(dense_components_[last_index]);

        // Update sparse array for moved element
        sparse_[last_entity] = dense_index;
    }

    // Remove last element
    dense_entities_.pop_back();
    dense_components_.pop_back();
    sparse_[entity] = std::numeric_limits<std::uint32_t>::max();
}

template<typename T>
T& ComponentArray<T>::get_component(Entity entity) {
    if (!has_component(entity)) {
        throw std::runtime_error("Entity does not have this component type");
    }

    return dense_components_[sparse_[entity]];
}

template<typename T>
const T& ComponentArray<T>::get_component(Entity entity) const {
    if (!has_component(entity)) {
        throw std::runtime_error("Entity does not have this component type");
    }

    return dense_components_[sparse_[entity]];
}

template<typename T>
bool ComponentArray<T>::has_component(Entity entity) const {
    if (entity >= sparse_.size()) {
        return false;
    }

    std::uint32_t dense_index = sparse_[entity];
    return dense_index != std::numeric_limits<std::uint32_t>::max() &&
           dense_index < dense_entities_.size() &&
           dense_entities_[dense_index] == entity;
}

template<typename T>
std::size_t ComponentArray<T>::size() const noexcept {
    return dense_components_.size();
}

template<typename T>
T* ComponentArray<T>::data() noexcept {
    return dense_components_.data();
}

template<typename T>
const T* ComponentArray<T>::data() const noexcept {
    return dense_components_.data();
}

template<typename T>
Entity* ComponentArray<T>::entities() noexcept {
    return dense_entities_.data();
}

template<typename T>
const Entity* ComponentArray<T>::entities() const noexcept {
    return dense_entities_.data();
}

template<typename T>
void ComponentArray<T>::clear() {
    // Call destructors for non-trivial types
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (auto& component : dense_components_) {
            component.~T();
        }
    }

    dense_components_.clear();
    dense_entities_.clear();
    std::fill(sparse_.begin(), sparse_.end(), std::numeric_limits<std::uint32_t>::max());
}

// World template implementations
template<typename T>
void World::add_component(EntityHandle handle, T component) {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    ComponentRegistry::instance().register_component<T>();
    get_or_create_component_array<T>().add_component(handle.id, std::move(component));
}

template<typename T>
void World::remove_component(EntityHandle handle) {
    if (!is_valid(handle)) {
        return; // Silently ignore invalid handles
    }

    try {
        auto& array = get_component_array<T>();
        array.remove_component(handle.id);
    } catch (const std::exception&) {
        // Component array doesn't exist, nothing to remove
    }
}

template<typename T>
T& World::get_component(EntityHandle handle) {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    return get_component_array<T>().get_component(handle.id);
}

template<typename T>
const T& World::get_component(EntityHandle handle) const {
    if (!is_valid(handle)) {
        throw std::runtime_error("Invalid entity handle");
    }

    return get_component_array<T>().get_component(handle.id);
}

template<typename T>
bool World::has_component(EntityHandle handle) const {
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
ComponentArray<T>& World::get_component_array() {
    return get_or_create_component_array<T>();
}

template<typename T>
const ComponentArray<T>& World::get_component_array() const {
    ComponentID id = ComponentRegistry::instance().get_component_id<T>();

    auto it = component_arrays_.find(id);
    if (it == component_arrays_.end()) {
        throw std::runtime_error("Component array does not exist");
    }

    return *static_cast<const ComponentArray<T>*>(it->second.get());
}

template<typename T>
ComponentArray<T>& World::get_or_create_component_array() {
    ComponentID id = ComponentRegistry::instance().register_component<T>();

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
T& World::add_system(Args&&... args) {
    return system_manager_->add_system<T>(std::forward<Args>(args)...);
}

template<typename T>
T& World::get_system() {
    return system_manager_->get_system<T>();
}

template<typename T>
void World::remove_system() {
    system_manager_->remove_system<T>();
}

// SystemManager template implementations
template<typename T, typename... Args>
T& SystemManager::add_system(Args&&... args) {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it != systems_.end()) {
        throw std::runtime_error("System already exists");
    }

    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T& system_ref = *system;

    systems_[type] = std::move(system);
    update_order_.push_back(&system_ref);

    return system_ref;
}

template<typename T>
T& SystemManager::get_system() {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it == systems_.end()) {
        throw std::runtime_error("System not found");
    }

    return *static_cast<T*>(it->second.get());
}

template<typename T>
void SystemManager::remove_system() {
    static_assert(std::is_base_of_v<System, T>, "T must inherit from System");

    const std::type_index type = std::type_index(typeid(T));

    auto it = systems_.find(type);
    if (it == systems_.end()) {
        return; // System doesn't exist, nothing to remove
    }

    System* system_ptr = it->second.get();

    // Remove from update order
    auto order_it = std::find(update_order_.begin(), update_order_.end(), system_ptr);
    if (order_it != update_order_.end()) {
        update_order_.erase(order_it);
    }

    systems_.erase(it);
}

} // namespace lore::ecs