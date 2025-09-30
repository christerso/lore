#pragma once

namespace lore::ecs {

// LoreECS template implementations
template<typename T>
void LoreECS::add_component(EntityHandle entity, T component) {
    world_->add_component(entity, std::move(component));

    // Register component for serialization if not already done
    if (!ComponentSerializerRegistry::instance().is_component_serializable(
            ComponentRegistry::instance().get_component_id<T>())) {
        register_serializable_component<T>();
    }
}

template<typename T>
void LoreECS::remove_component(EntityHandle entity) {
    world_->remove_component<T>(entity);
}

template<typename T>
T& LoreECS::get_component(EntityHandle entity) {
    return world_->get_component<T>(entity);
}

template<typename T>
const T& LoreECS::get_component(EntityHandle entity) const {
    return world_->get_component<T>(entity);
}

template<typename T>
bool LoreECS::has_component(EntityHandle entity) const {
    return world_->has_component<T>(entity);
}

template<typename T>
void LoreECS::add_components_batch(const std::vector<EntityHandle>& entities, const std::vector<T>& components) {
    world_->add_components_batch(entities, components);

    // Register component for serialization if not already done
    if (!ComponentSerializerRegistry::instance().is_component_serializable(
            ComponentRegistry::instance().get_component_id<T>())) {
        register_serializable_component<T>();
    }
}

template<typename T>
void LoreECS::remove_components_batch(const std::vector<EntityHandle>& entities) {
    world_->remove_components_batch<T>(entities);
}

template<typename... ComponentTypes>
auto LoreECS::create_query() -> TypedQuery<ComponentTypes...> {
    return TypedQuery<ComponentTypes...>{};
}

template<typename... ComponentTypes>
void LoreECS::for_each(std::function<void(EntityHandle, ComponentTypes&...)> callback) {
    auto query = create_query<ComponentTypes...>();
    query.for_each(*world_, callback);
}

template<typename T, typename... Args>
T& LoreECS::add_system(Args&&... args) {
    return world_->add_system<T>(std::forward<Args>(args)...);
}

template<typename T>
T& LoreECS::get_system() {
    return world_->get_system<T>();
}

template<typename T>
void LoreECS::remove_system() {
    world_->remove_system<T>();
}

template<typename Before, typename After>
void LoreECS::add_system_dependency() {
    world_->system_scheduler_->add_dependency<Before, After>();
}

template<typename T>
void LoreECS::register_serializable_component() {
    ComponentSerializerRegistry::instance().register_component<T>();
}

template<typename T>
void LoreECS::register_component_dependency() {
    // Component dependencies would be registered with the dependency manager
    // This is a placeholder for the specific dependency registration
}

} // namespace lore::ecs