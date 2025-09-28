#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <memory>
#include <typeindex>

namespace lore::ecs {

// High-performance Entity-Component-System inspired by Aeon's sparse set design
// Designed for 1M+ entities with efficient iteration and random access

using Entity = std::uint32_t;
using ComponentID = std::uint32_t;
using Generation = std::uint16_t;

constexpr Entity INVALID_ENTITY = 0;
constexpr std::size_t MAX_ENTITIES = 1'000'000;
constexpr std::size_t MAX_COMPONENT_TYPES = 256;

// Component bit set for fast archetype matching
using ComponentBitSet = std::bitset<MAX_COMPONENT_TYPES>;

// Entity with generation for safe reuse
struct EntityHandle {
    Entity id;
    Generation generation;

    bool operator==(const EntityHandle& other) const noexcept;
    bool operator!=(const EntityHandle& other) const noexcept;
};

// Component registration information
struct ComponentInfo {
    ComponentID id;
    std::size_t size;
    std::size_t alignment;
    std::string name;
    void (*destructor)(void* data);
};

// Forward declarations
class World;
class SystemManager;

// Component registry for type management
class ComponentRegistry {
public:
    static ComponentRegistry& instance();

    template<typename T>
    ComponentID register_component();

    template<typename T>
    ComponentID get_component_id() const;

    const ComponentInfo& get_component_info(ComponentID id) const;
    std::size_t get_component_count() const noexcept;

private:
    ComponentRegistry() = default;

    std::unordered_map<std::type_index, ComponentID> type_to_id_;
    std::vector<ComponentInfo> components_;
    ComponentID next_id_ = 1;
};

// Sparse set for efficient entity-component storage
template<typename T>
class ComponentArray {
public:
    ComponentArray();
    ~ComponentArray();

    // Component management
    void add_component(Entity entity, T component);
    void remove_component(Entity entity);
    T& get_component(Entity entity);
    const T& get_component(Entity entity) const;
    bool has_component(Entity entity) const;

    // Iteration support
    std::size_t size() const noexcept;
    T* data() noexcept;
    const T* data() const noexcept;
    Entity* entities() noexcept;
    const Entity* entities() const noexcept;

    // Clear all components
    void clear();

private:
    std::vector<std::uint32_t> sparse_;     // entity -> dense index
    std::vector<Entity> dense_entities_;    // dense index -> entity
    std::vector<T> dense_components_;       // component data
};

// Entity manager for entity lifecycle
class EntityManager {
public:
    EntityManager();

    // Entity lifecycle
    EntityHandle create_entity();
    void destroy_entity(EntityHandle handle);
    bool is_valid(EntityHandle handle) const;

    // Statistics
    std::size_t get_entity_count() const noexcept;
    std::size_t get_generation(Entity entity) const;

private:
    std::vector<Generation> generations_;
    std::vector<Entity> free_entities_;
    Entity next_entity_ = 1;
    std::size_t living_entity_count_ = 0;
};

// Main World class - central ECS coordinator
class World {
public:
    World();
    ~World();

    // Entity management
    EntityHandle create_entity();
    void destroy_entity(EntityHandle handle);
    bool is_valid(EntityHandle handle) const;

    // Component management
    template<typename T>
    void add_component(EntityHandle handle, T component);

    template<typename T>
    void remove_component(EntityHandle handle);

    template<typename T>
    T& get_component(EntityHandle handle);

    template<typename T>
    const T& get_component(EntityHandle handle) const;

    template<typename T>
    bool has_component(EntityHandle handle) const;

    // Component array access for systems
    template<typename T>
    ComponentArray<T>& get_component_array();

    template<typename T>
    const ComponentArray<T>& get_component_array() const;

    // System management
    template<typename T, typename... Args>
    T& add_system(Args&&... args);

    template<typename T>
    T& get_system();

    template<typename T>
    void remove_system();

    // Update all systems
    void update(float delta_time);

    // Statistics
    std::size_t get_entity_count() const noexcept;
    std::size_t get_component_type_count() const noexcept;

private:
    EntityManager entity_manager_;
    std::unordered_map<ComponentID, std::unique_ptr<void, void(*)(void*)>> component_arrays_;
    std::unique_ptr<SystemManager> system_manager_;

    template<typename T>
    ComponentArray<T>& get_or_create_component_array();
};

// Base system class
class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, float delta_time) = 0;
    virtual void init(World& world) { (void)world; }
    virtual void shutdown(World& world) { (void)world; }
};

// System manager for system lifecycle
class SystemManager {
public:
    SystemManager() = default;
    ~SystemManager() = default;

    template<typename T, typename... Args>
    T& add_system(Args&&... args);

    template<typename T>
    T& get_system();

    template<typename T>
    void remove_system();

    void update_all(World& world, float delta_time);
    void init_all(World& world);
    void shutdown_all(World& world);

private:
    std::unordered_map<std::type_index, std::unique_ptr<System>> systems_;
    std::vector<System*> update_order_;
};

} // namespace lore::ecs

// Template implementations would go in a separate .inl file
#include "ecs.inl"