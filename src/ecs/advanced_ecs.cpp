#include <lore/ecs/advanced_ecs.hpp>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

namespace lore::ecs {

// LoreECS implementation
LoreECS::LoreECS()
    : world_(std::make_unique<AdvancedWorld>())
    , serializer_(std::make_unique<WorldSerializer>())
    , dependency_manager_(std::make_unique<ComponentDependencyManager>())
    , change_tracker_(std::make_unique<ComponentChangeTracker>())
{
    initialize_systems();
    setup_component_dependencies();

    // Enable default features
    enable_thread_safety(true);
    enable_change_tracking(true);
}

LoreECS::~LoreECS() = default;

EntityHandle LoreECS::create_entity() {
    return world_->create_entity();
}

EntityHandle LoreECS::create_entity_in_region(int32_t x, int32_t y, int32_t z) {
    return world_->create_entity_in_region(x, y, z);
}

void LoreECS::destroy_entity(EntityHandle entity) {
    world_->destroy_entity(entity);
}

bool LoreECS::is_valid(EntityHandle entity) const {
    return world_->is_valid(entity);
}

void LoreECS::update(float delta_time) {
    world_->update(delta_time);

    // Process change tracking
    if (change_tracking_enabled_) {
        change_tracker_->process_pending_changes();
    }
}

void LoreECS::update_parallel(float delta_time, std::size_t thread_count) {
    world_->update_parallel(delta_time, thread_count);

    // Process change tracking
    if (change_tracking_enabled_) {
        change_tracker_->process_pending_changes();
    }
}

void LoreECS::set_observer_position(const float* position) {
    world_->get_lod_manager().set_observer_position(position);
}

void LoreECS::set_active_region_bounds(const float* min_bounds, const float* max_bounds) {
    world_->set_active_region_bounds(min_bounds, max_bounds);
}

void LoreECS::set_lod_distances(float high, float medium, float low) {
    world_->get_lod_manager().set_lod_distances(high, medium, low);
}

bool LoreECS::save_world(const std::string& filename, WorldSerializer::Format format) {
    return serializer_->serialize_world(*world_, filename, format);
}

bool LoreECS::load_world(const std::string& filename) {
    return serializer_->deserialize_world(*world_, filename);
}

bool LoreECS::save_entities(const std::vector<EntityHandle>& entities, const std::string& filename) {
    return serializer_->serialize_entities(entities, *world_, filename);
}

bool LoreECS::load_entities(const std::string& filename) {
    return serializer_->deserialize_entities_into_world(*world_, filename);
}

void LoreECS::compact_storage() {
    world_->compact_all_storage();
}

void LoreECS::set_memory_budget(std::size_t bytes) {
    world_->set_memory_budget(bytes);
}

std::size_t LoreECS::get_memory_usage() const {
    return world_->get_memory_usage();
}

AdvancedWorld::PerformanceStats LoreECS::get_performance_stats() const {
    return world_->get_performance_stats();
}

std::vector<SystemScheduler::SystemPerformance> LoreECS::get_system_performance() const {
    return world_->system_scheduler_->get_system_performance();
}

void LoreECS::set_parent(EntityHandle child, EntityHandle parent) {
    world_->relationship_manager_->set_parent(child, parent);
}

void LoreECS::remove_parent(EntityHandle child) {
    world_->relationship_manager_->remove_parent(child);
}

EntityHandle LoreECS::get_parent(EntityHandle child) const {
    return world_->relationship_manager_->get_parent(child);
}

std::vector<EntityHandle> LoreECS::get_children(EntityHandle parent) const {
    return world_->relationship_manager_->get_children(parent);
}

std::size_t LoreECS::get_entity_count() const {
    return world_->get_entity_count();
}

std::size_t LoreECS::get_component_type_count() const {
    return world_->get_component_type_count();
}

std::size_t LoreECS::get_active_region_count() const {
    return world_->get_active_region_count();
}

void LoreECS::enable_thread_safety(bool enable) {
    thread_safety_enabled_ = enable;
    world_->entity_manager_->enable_thread_safety(enable);
}

void LoreECS::enable_change_tracking(bool enable) {
    change_tracking_enabled_ = enable;
    if (enable) {
        // Register change tracker with world
        // Implementation would connect the change notifier to the tracker
    }
}

void LoreECS::enable_serialization_profiling(bool enable) {
    profiling_enabled_ = enable;
    SerializationProfiler::instance().enable_profiling(enable);
}

bool LoreECS::validate_world_state() const {
    auto validation_result = EntityValidator::validate_all_entities(*world_->entity_manager_);
    return validation_result.is_valid;
}

void LoreECS::log_performance_report() const {
    auto stats = get_performance_stats();
    auto system_performance = get_system_performance();

    std::cout << "\n=== Lore ECS Performance Report ===\n";
    std::cout << "Total entities: " << stats.total_entities << "\n";
    std::cout << "Active entities: " << stats.active_entities << "\n";
    std::cout << "Systems count: " << stats.systems_count << "\n";
    std::cout << "Memory usage: " << stats.memory_usage_bytes << " bytes\n";
    std::cout << "Memory fragmentation: " << (stats.memory_fragmentation * 100.0f) << "%\n";
    std::cout << "Last update time: " << stats.last_update_time.count() << " μs\n";
    std::cout << "Average update time: " << stats.average_update_time.count() << " μs\n";

    std::cout << "\nSystem Performance:\n";
    for (const auto& perf : system_performance) {
        std::cout << "  " << perf.name << ":\n";
        std::cout << "    Last execution: " << perf.last_execution_time.count() << " μs\n";
        std::cout << "    Average execution: " << perf.average_execution_time.count() << " μs\n";
        std::cout << "    Execution count: " << perf.execution_count << "\n";
    }

    std::cout << "\nComponent Memory Pools:\n";
    auto total_pool_memory = ComponentPoolManager::instance().get_total_memory_usage();
    std::cout << "  Total pool memory: " << total_pool_memory << " bytes\n";

    std::cout << "====================================\n\n";
}

void LoreECS::initialize_systems() {
    // Register default systems that are commonly needed
    add_system<TransformSystem>();
    add_system<LifetimeSystem>();

    // Set up system dependencies
    // TransformSystem should run before other systems that depend on transforms
    // This would be configured based on actual system requirements
}

void LoreECS::setup_component_dependencies() {
    // Register common component serialization
    register_serializable_component<Transform>();
    register_serializable_component<Lifetime>();

    // Set up component dependencies
    // For example, certain components might depend on Transform being present
    // This would be configured based on actual component relationships
}

// Transform implementation
void Transform::serialize(BinaryArchive& archive) const {
    archive.write_bytes(position, sizeof(position));
    archive.write_bytes(rotation, sizeof(rotation));
    archive.write_bytes(scale, sizeof(scale));
}

void Transform::deserialize(BinaryArchive& archive) {
    archive.read_bytes(position, sizeof(position));
    archive.read_bytes(rotation, sizeof(rotation));
    archive.read_bytes(scale, sizeof(scale));
}

void Transform::serialize(JsonArchive& archive) const {
    archive.begin_object("transform");

    archive.begin_array("position");
    for (int i = 0; i < 3; ++i) {
        archive.write_value(std::to_string(i), position[i]);
    }
    archive.end_array();

    archive.begin_array("rotation");
    for (int i = 0; i < 4; ++i) {
        archive.write_value(std::to_string(i), rotation[i]);
    }
    archive.end_array();

    archive.begin_array("scale");
    for (int i = 0; i < 3; ++i) {
        archive.write_value(std::to_string(i), scale[i]);
    }
    archive.end_array();

    archive.end_object();
}

void Transform::deserialize(JsonArchive& archive) {
    try {
        // Parse JSON manually without external dependencies
        const std::string& json_data = archive.get_json_data();

        // Initialize with defaults
        std::fill(std::begin(position), std::end(position), 0.0f);
        std::fill(std::begin(scale), std::end(scale), 1.0f);
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        rotation[3] = 1.0f; // w component of quaternion

        if (json_data.empty() || json_data == "{}") {
            return;
        }

        // Simple JSON parsing for Transform component
        auto extract_array = [&json_data](const std::string& key, float* array, std::size_t count) {
            std::string search_key = "\"" + key + "\":[";
            auto pos = json_data.find(search_key);
            if (pos == std::string::npos) return;

            pos += search_key.length();
            std::string values_str;

            for (auto i = pos; i < json_data.length() && json_data[i] != ']'; ++i) {
                values_str += json_data[i];
            }

            // Parse comma-separated values
            std::istringstream ss(values_str);
            std::string value_str;
            std::size_t index = 0;

            while (std::getline(ss, value_str, ',') && index < count) {
                try {
                    array[index] = std::stof(value_str);
                } catch (...) {
                    // Keep default value on parse error
                }
                index++;
            }
        };

        extract_array("position", position, 3);
        extract_array("scale", scale, 3);
        extract_array("rotation", rotation, 4);

    } catch (...) {
        // Keep default values on error
        std::fill(std::begin(position), std::end(position), 0.0f);
        std::fill(std::begin(scale), std::end(scale), 1.0f);
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        rotation[3] = 1.0f;
    }
}

// Lifetime implementation
void Lifetime::serialize(BinaryArchive& archive) const {
    archive << remaining_time << destroy_on_expire;
}

void Lifetime::deserialize(BinaryArchive& archive) {
    archive >> remaining_time >> destroy_on_expire;
}

void Lifetime::serialize(JsonArchive& archive) const {
    archive.write_value("remaining_time", remaining_time);
    archive.write_value("destroy_on_expire", destroy_on_expire);
}

void Lifetime::deserialize(JsonArchive& archive) {
    archive.read_value("remaining_time", remaining_time);
    archive.read_value("destroy_on_expire", destroy_on_expire);
}

// TransformSystem implementation
TransformSystem::TransformSystem() = default;

void TransformSystem::update(World& world, [[maybe_unused]] float delta_time) {
    // Example transform system that maintains world matrices
    // In a real implementation, this would calculate world transforms
    // based on parent-child relationships

    transform_query_.for_each(static_cast<AdvancedWorld&>(world),
        []([[maybe_unused]] EntityHandle entity, [[maybe_unused]] Transform& transform) {
            // Update transform logic would go here
            // For example, updating world matrices from local transforms
        });
}

// LifetimeSystem implementation
LifetimeSystem::LifetimeSystem() = default;

void LifetimeSystem::update(World& world, float delta_time) {
    std::vector<EntityHandle> entities_to_destroy;

    lifetime_query_.for_each(static_cast<AdvancedWorld&>(world),
        [&](EntityHandle entity, Lifetime& lifetime) {
            lifetime.remaining_time -= delta_time;

            if (lifetime.remaining_time <= 0.0f && lifetime.destroy_on_expire) {
                entities_to_destroy.push_back(entity);
            }
        });

    // Destroy expired entities
    auto& advanced_world = static_cast<AdvancedWorld&>(world);
    for (EntityHandle entity : entities_to_destroy) {
        advanced_world.destroy_entity(entity);
    }
}

// ExampleUsageSystem implementation
ExampleUsageSystem::ExampleUsageSystem() = default;

void ExampleUsageSystem::update(World& world, float delta_time) {
    update_renderables(world, delta_time);
    update_physics(world, delta_time);
    update_players(world, delta_time);
}

void ExampleUsageSystem::update_renderables(World& world, [[maybe_unused]] float delta_time) {
    auto& advanced_world = static_cast<AdvancedWorld&>(world);

    // Process all renderable entities
    renderable_query_.for_each(advanced_world,
        []([[maybe_unused]] EntityHandle entity, [[maybe_unused]] Transform& transform, [[maybe_unused]] Renderable& renderable) {
            // Update rendering logic
            // For example, updating visibility, LOD level, etc.
        });
}

void ExampleUsageSystem::update_physics(World& world, float delta_time) {
    auto& advanced_world = static_cast<AdvancedWorld&>(world);

    // Process all physics entities
    physics_query_.for_each(advanced_world,
        [delta_time]([[maybe_unused]] EntityHandle entity, [[maybe_unused]] Transform& transform, [[maybe_unused]] Collidable& collidable) {
            // Update physics simulation
            // For example, applying forces, collision detection, etc.
        });
}

void ExampleUsageSystem::update_players(World& world, float delta_time) {
    auto& advanced_world = static_cast<AdvancedWorld&>(world);

    // Process all player entities
    player_query_.for_each(advanced_world,
        [delta_time]([[maybe_unused]] EntityHandle entity, [[maybe_unused]] Transform& transform, [[maybe_unused]] PlayerController& controller) {
            // Update player-specific logic
            // For example, handling input, camera control, etc.
        });
}

// ECSBenchmark implementation
ECSBenchmark::BenchmarkResults ECSBenchmark::run_benchmark(std::size_t entity_count) {
    std::cout << "Running ECS benchmark with " << entity_count << " entities...\n";

    BenchmarkResults results{};

    // Benchmark entity operations
    auto entity_results = benchmark_entity_operations(entity_count);
    results.entity_creation_time = entity_results.entity_creation_time;
    results.entities_per_second = entity_results.entities_per_second;

    // Benchmark component operations
    auto component_results = benchmark_component_operations(entity_count);
    results.component_addition_time = component_results.component_addition_time;
    results.components_per_second = component_results.components_per_second;

    // Benchmark query performance
    auto query_results = benchmark_query_performance(entity_count);
    results.query_execution_time = query_results.query_execution_time;
    results.system_update_time = query_results.system_update_time;

    // Benchmark serialization
    auto serialization_results = benchmark_serialization(entity_count / 10); // Use fewer entities for serialization
    results.serialization_time = serialization_results.serialization_time;

    // Calculate memory efficiency
    LoreECS ecs;
    for (std::size_t i = 0; i < entity_count; ++i) {
        auto entity = ecs.create_entity();
        ecs.add_component(entity, Transform{});
        ecs.add_component(entity, Lifetime{10.0f, true});
    }

    std::size_t memory_used = ecs.get_memory_usage();
    std::size_t theoretical_minimum = entity_count * (sizeof(Transform) + sizeof(Lifetime) + sizeof(EntityHandle));
    results.memory_efficiency = static_cast<float>(theoretical_minimum) / memory_used;

    return results;
}

void ECSBenchmark::log_benchmark_results(const BenchmarkResults& results) {
    std::cout << "\n=== ECS Benchmark Results ===\n";
    std::cout << "Entity creation time: " << results.entity_creation_time.count() << " μs\n";
    std::cout << "Component addition time: " << results.component_addition_time.count() << " μs\n";
    std::cout << "Query execution time: " << results.query_execution_time.count() << " μs\n";
    std::cout << "System update time: " << results.system_update_time.count() << " μs\n";
    std::cout << "Serialization time: " << results.serialization_time.count() << " μs\n";
    std::cout << "Entities per second: " << results.entities_per_second << "\n";
    std::cout << "Components per second: " << results.components_per_second << "\n";
    std::cout << "Memory efficiency: " << (results.memory_efficiency * 100.0f) << "%\n";
    std::cout << "=============================\n\n";
}

ECSBenchmark::BenchmarkResults ECSBenchmark::benchmark_entity_operations(std::size_t count) {
    BenchmarkResults results{};

    LoreECS ecs;
    std::vector<EntityHandle> entities;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < count; ++i) {
        entities.push_back(ecs.create_entity());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    results.entity_creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    if (results.entity_creation_time.count() > 0) {
        results.entities_per_second = (count * 1000000) / results.entity_creation_time.count();
    }

    return results;
}

ECSBenchmark::BenchmarkResults ECSBenchmark::benchmark_component_operations(std::size_t count) {
    BenchmarkResults results{};

    LoreECS ecs;
    std::vector<EntityHandle> entities;

    // Create entities first
    for (std::size_t i = 0; i < count; ++i) {
        entities.push_back(ecs.create_entity());
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    for (auto entity : entities) {
        ecs.add_component(entity, Transform{});
        ecs.add_component(entity, Lifetime{10.0f, true});
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    results.component_addition_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::size_t total_components = count * 2; // Transform + Lifetime
    if (results.component_addition_time.count() > 0) {
        results.components_per_second = (total_components * 1000000) / results.component_addition_time.count();
    }

    return results;
}

ECSBenchmark::BenchmarkResults ECSBenchmark::benchmark_query_performance(std::size_t count) {
    BenchmarkResults results{};

    LoreECS ecs;

    // Create entities with components
    for (std::size_t i = 0; i < count; ++i) {
        auto entity = ecs.create_entity();
        ecs.add_component(entity, Transform{});
        ecs.add_component(entity, Lifetime{10.0f, true});
    }

    // Benchmark query execution
    auto start_time = std::chrono::high_resolution_clock::now();

    auto query = ecs.create_query<Transform, Lifetime>();
    std::size_t processed_entities = 0;

    query.for_each(ecs.get_world(),
        [&processed_entities]([[maybe_unused]] EntityHandle entity, Transform& transform, Lifetime& lifetime) {
            ++processed_entities;
            // Simulate some work
            transform.position[0] += 1.0f;
            lifetime.remaining_time -= 0.016f;
        });

    auto end_time = std::chrono::high_resolution_clock::now();
    results.query_execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Benchmark system update
    start_time = std::chrono::high_resolution_clock::now();
    ecs.update(0.016f); // Simulate 60 FPS
    end_time = std::chrono::high_resolution_clock::now();
    results.system_update_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    return results;
}

ECSBenchmark::BenchmarkResults ECSBenchmark::benchmark_serialization(std::size_t count) {
    BenchmarkResults results{};

    LoreECS ecs;

    // Create entities with components
    for (std::size_t i = 0; i < count; ++i) {
        auto entity = ecs.create_entity();
        ecs.add_component(entity, Transform{});
        ecs.add_component(entity, Lifetime{10.0f, true});
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    bool success = ecs.save_world("benchmark_world.dat");

    auto end_time = std::chrono::high_resolution_clock::now();
    results.serialization_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Clean up benchmark file
    if (success) {
        std::remove("benchmark_world.dat");
    }

    return results;
}

} // namespace lore::ecs