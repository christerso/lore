#pragma once

// Main ECS integration header - includes all advanced ECS functionality

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/entity_manager.hpp>
#include <lore/ecs/world_manager.hpp>
#include <lore/ecs/component_tracking.hpp>
#include <lore/ecs/serialization.hpp>

namespace lore::ecs {

// Complete Entity Management System for Lore Engine
// Provides 1M+ entity support with advanced features:
// - Generation-based entity ID recycling
// - World state management with LOD and streaming
// - Component relationship tracking and dependencies
// - Advanced query system with SIMD optimization
// - Thread-safe concurrent access patterns
// - Memory pooling for optimal performance
// - World state serialization for save/load
// - Debug visualization and profiling tools

class LoreECS {
public:
    LoreECS();
    ~LoreECS();

    // World management
    AdvancedWorld& get_world() { return *world_; }
    const AdvancedWorld& get_world() const { return *world_; }

    // Entity lifecycle with advanced features
    EntityHandle create_entity();
    EntityHandle create_entity_in_region(int32_t x, int32_t y, int32_t z);
    void destroy_entity(EntityHandle entity);
    bool is_valid(EntityHandle entity) const;

    // Component management with dependency tracking
    template<typename T>
    void add_component(EntityHandle entity, T component);

    template<typename T>
    void remove_component(EntityHandle entity);

    template<typename T>
    T& get_component(EntityHandle entity);

    template<typename T>
    const T& get_component(EntityHandle entity) const;

    template<typename T>
    bool has_component(EntityHandle entity) const;

    // Batch operations for performance
    template<typename T>
    void add_components_batch(const std::vector<EntityHandle>& entities, const std::vector<T>& components);

    template<typename T>
    void remove_components_batch(const std::vector<EntityHandle>& entities);

    // Advanced queries
    template<typename... ComponentTypes>
    auto create_query() -> TypedQuery<ComponentTypes...>;

    template<typename... ComponentTypes>
    void for_each(std::function<void(EntityHandle, ComponentTypes&...)> callback);

    // System management with dependencies
    template<typename T, typename... Args>
    T& add_system(Args&&... args);

    template<typename T>
    T& get_system();

    template<typename T>
    void remove_system();

    template<typename Before, typename After>
    void add_system_dependency();

    // Update with performance options
    void update(float delta_time);
    void update_parallel(float delta_time, std::size_t thread_count = 0);

    // World streaming and LOD
    void set_observer_position(const float* position);
    void set_active_region_bounds(const float* min_bounds, const float* max_bounds);
    void set_lod_distances(float high, float medium, float low);

    // Serialization
    bool save_world(const std::string& filename, WorldSerializer::Format format = WorldSerializer::Format::Binary);
    bool load_world(const std::string& filename);
    bool save_entities(const std::vector<EntityHandle>& entities, const std::string& filename);
    bool load_entities(const std::string& filename);

    // Memory management
    void compact_storage();
    void set_memory_budget(std::size_t bytes);
    std::size_t get_memory_usage() const;

    // Performance monitoring
    AdvancedWorld::PerformanceStats get_performance_stats() const;
    std::vector<SystemScheduler::SystemPerformance> get_system_performance() const;

    // Component registration for serialization
    template<typename T>
    void register_serializable_component();

    template<typename T>
    void register_component_dependency();

    // Entity relationships
    void set_parent(EntityHandle child, EntityHandle parent);
    void remove_parent(EntityHandle child);
    EntityHandle get_parent(EntityHandle child) const;
    std::vector<EntityHandle> get_children(EntityHandle parent) const;

    // Statistics
    std::size_t get_entity_count() const;
    std::size_t get_component_type_count() const;
    std::size_t get_active_region_count() const;

    // Configuration
    void enable_thread_safety(bool enable = true);
    void enable_change_tracking(bool enable = true);
    void enable_serialization_profiling(bool enable = true);

    // Debug and validation
    bool validate_world_state() const;
    void log_performance_report() const;

private:
    std::unique_ptr<AdvancedWorld> world_;
    std::unique_ptr<WorldSerializer> serializer_;
    std::unique_ptr<ComponentDependencyManager> dependency_manager_;
    std::unique_ptr<ComponentChangeTracker> change_tracker_;

    bool thread_safety_enabled_ = true;
    bool change_tracking_enabled_ = true;
    bool profiling_enabled_ = false;

    void initialize_systems();
    void setup_component_dependencies();
};

// Utility classes for common ECS patterns

// Transform hierarchy system
struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion
    float scale[3] = {1.0f, 1.0f, 1.0f};

    void serialize(BinaryArchive& archive) const;
    void deserialize(BinaryArchive& archive);
    void serialize(JsonArchive& archive) const;
    void deserialize(JsonArchive& archive);
};

class TransformSystem : public System {
public:
    TransformSystem();
    void update(World& world, float delta_time) override;

private:
    TypedQuery<Transform> transform_query_;
};

// Lifetime management
struct Lifetime {
    float remaining_time;
    bool destroy_on_expire = true;

    void serialize(BinaryArchive& archive) const;
    void deserialize(BinaryArchive& archive);
    void serialize(JsonArchive& archive) const;
    void deserialize(JsonArchive& archive);
};

class LifetimeSystem : public System {
public:
    LifetimeSystem();
    void update(World& world, float delta_time) override;

private:
    TypedQuery<Lifetime> lifetime_query_;
};

// Tag components for common entity types
struct StaticGeometry {};
struct DynamicGeometry {};
struct PlayerController {};
struct AIController {};
struct Renderable {};
struct Collidable {};
struct Audible {};

// Example usage system that demonstrates performance patterns
class ExampleUsageSystem : public System {
public:
    ExampleUsageSystem();
    void update(World& world, float delta_time) override;

private:
    // Multiple typed queries for different entity combinations
    TypedQuery<Transform, Renderable> renderable_query_;
    TypedQuery<Transform, Collidable> physics_query_;
    TypedQuery<Transform, PlayerController> player_query_;

    void update_renderables(World& world, float delta_time);
    void update_physics(World& world, float delta_time);
    void update_players(World& world, float delta_time);
};

// Performance benchmarking utilities
class ECSBenchmark {
public:
    struct BenchmarkResults {
        std::chrono::microseconds entity_creation_time;
        std::chrono::microseconds component_addition_time;
        std::chrono::microseconds query_execution_time;
        std::chrono::microseconds system_update_time;
        std::chrono::microseconds serialization_time;
        std::size_t entities_per_second;
        std::size_t components_per_second;
        float memory_efficiency;
    };

    static BenchmarkResults run_benchmark(std::size_t entity_count = 100000);
    static void log_benchmark_results(const BenchmarkResults& results);

private:
    static BenchmarkResults benchmark_entity_operations(std::size_t count);
    static BenchmarkResults benchmark_component_operations(std::size_t count);
    static BenchmarkResults benchmark_query_performance(std::size_t count);
    static BenchmarkResults benchmark_serialization(std::size_t count);
};

} // namespace lore::ecs

#include "advanced_ecs.inl"