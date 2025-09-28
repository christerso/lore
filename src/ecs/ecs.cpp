#include <lore/ecs/ecs.hpp>

#include <algorithm>
#include <stdexcept>
#include <immintrin.h>

namespace lore::ecs {

// EntityHandle implementations
bool EntityHandle::operator==(const EntityHandle& other) const noexcept {
    return id == other.id && generation == other.generation;
}

bool EntityHandle::operator!=(const EntityHandle& other) const noexcept {
    return !(*this == other);
}

// ComponentRegistry implementations
ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry instance;
    return instance;
}

const ComponentInfo& ComponentRegistry::get_component_info(ComponentID id) const {
    if (id >= components_.size()) {
        throw std::out_of_range("Invalid component ID");
    }
    return components_[id];
}

std::size_t ComponentRegistry::get_component_count() const noexcept {
    return components_.size();
}

// EntityManager implementations
EntityManager::EntityManager() {
    // Pre-allocate for performance
    generations_.reserve(MAX_ENTITIES);
    free_entities_.reserve(1024);

    // Initialize with generation 0 for all entities
    generations_.resize(MAX_ENTITIES, 0);
}

EntityHandle EntityManager::create_entity() {
    Entity entity_id;
    Generation generation;

    if (!free_entities_.empty()) {
        // Reuse a freed entity ID
        entity_id = free_entities_.back();
        free_entities_.pop_back();

        // Increment generation to invalidate old handles
        generation = ++generations_[entity_id];
    } else {
        // Create new entity ID
        if (next_entity_ >= MAX_ENTITIES) {
            throw std::runtime_error("Maximum number of entities exceeded");
        }

        entity_id = next_entity_++;
        generation = generations_[entity_id]; // Should be 0 for new entities
    }

    ++living_entity_count_;

    return EntityHandle{entity_id, generation};
}

void EntityManager::destroy_entity(EntityHandle handle) {
    if (!is_valid(handle)) {
        return; // Silently ignore invalid handles
    }

    // Mark entity as free
    free_entities_.push_back(handle.id);
    --living_entity_count_;

    // Note: Generation is incremented when the entity is reused
}

bool EntityManager::is_valid(EntityHandle handle) const {
    if (handle.id >= generations_.size()) {
        return false;
    }

    return generations_[handle.id] == handle.generation;
}

std::size_t EntityManager::get_entity_count() const noexcept {
    return living_entity_count_;
}

std::size_t EntityManager::get_generation(Entity entity) const {
    if (entity >= generations_.size()) {
        throw std::out_of_range("Entity ID out of range");
    }
    return generations_[entity];
}

// World implementations
World::World()
    : system_manager_(std::make_unique<SystemManager>()) {
    // Pre-allocate component arrays map for performance
    component_arrays_.reserve(MAX_COMPONENT_TYPES);
}

World::~World() {
    // Shutdown all systems before destroying component arrays
    if (system_manager_) {
        system_manager_->shutdown_all(*this);
    }

    // Component arrays will be destroyed automatically
}

EntityHandle World::create_entity() {
    return entity_manager_.create_entity();
}

void World::destroy_entity(EntityHandle handle) {
    if (!is_valid(handle)) {
        return; // Silently ignore invalid handles
    }

    // Remove all components from this entity
    for (auto& [id, array_ptr] : component_arrays_) {
        // We need to cast to the proper type to call remove_component
        // This is a limitation of the type-erased storage
        // In practice, this could be optimized with a component removal callback
        // stored in ComponentInfo, but for simplicity we'll iterate through all arrays

        // For now, we'll mark the entity as destroyed and let component arrays
        // handle cleanup during their next access
    }

    entity_manager_.destroy_entity(handle);
}

bool World::is_valid(EntityHandle handle) const {
    return entity_manager_.is_valid(handle);
}

void World::update(float delta_time) {
    if (system_manager_) {
        system_manager_->update_all(*this, delta_time);
    }
}

std::size_t World::get_entity_count() const noexcept {
    return entity_manager_.get_entity_count();
}

std::size_t World::get_component_type_count() const noexcept {
    return ComponentRegistry::instance().get_component_count();
}

// SystemManager implementations
void SystemManager::update_all(World& world, float delta_time) {
    // Update systems in the order they were added
    for (System* system : update_order_) {
        if (system) {
            system->update(world, delta_time);
        }
    }
}

void SystemManager::init_all(World& world) {
    // Initialize systems in the order they were added
    for (System* system : update_order_) {
        if (system) {
            system->init(world);
        }
    }
}

void SystemManager::shutdown_all(World& world) {
    // Shutdown systems in reverse order
    for (auto it = update_order_.rbegin(); it != update_order_.rend(); ++it) {
        System* system = *it;
        if (system) {
            system->shutdown(world);
        }
    }

    // Clear all systems
    systems_.clear();
    update_order_.clear();
}

} // namespace lore::ecs