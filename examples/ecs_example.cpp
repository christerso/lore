#include <lore/ecs/advanced_ecs.hpp>
#include <iostream>
#include <chrono>
#include <random>

using namespace lore::ecs;

// Example components for demonstration
struct Position {
    float x, y, z;

    Position(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

    void serialize(BinaryArchive& archive) const {
        archive << x << y << z;
    }

    void deserialize(BinaryArchive& archive) {
        archive >> x >> y >> z;
    }

    void serialize(JsonArchive& archive) const {
        archive.write_value("x", x);
        archive.write_value("y", y);
        archive.write_value("z", z);
    }

    void deserialize(JsonArchive& archive) {
        archive.read_value("x", x);
        archive.read_value("y", y);
        archive.read_value("z", z);
    }
};

struct Velocity {
    float dx, dy, dz;

    Velocity(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f) : dx(dx), dy(dy), dz(dz) {}

    void serialize(BinaryArchive& archive) const {
        archive << dx << dy << dz;
    }

    void deserialize(BinaryArchive& archive) {
        archive >> dx >> dy >> dz;
    }

    void serialize(JsonArchive& archive) const {
        archive.write_value("dx", dx);
        archive.write_value("dy", dy);
        archive.write_value("dz", dz);
    }

    void deserialize(JsonArchive& archive) {
        archive.read_value("dx", dx);
        archive.read_value("dy", dy);
        archive.read_value("dz", dz);
    }
};

struct Health {
    float current_hp;
    float max_hp;

    Health(float hp = 100.0f) : current_hp(hp), max_hp(hp) {}

    void serialize(BinaryArchive& archive) const {
        archive << current_hp << max_hp;
    }

    void deserialize(BinaryArchive& archive) {
        archive >> current_hp >> max_hp;
    }

    void serialize(JsonArchive& archive) const {
        archive.write_value("current_hp", current_hp);
        archive.write_value("max_hp", max_hp);
    }

    void deserialize(JsonArchive& archive) {
        archive.read_value("current_hp", current_hp);
        archive.read_value("max_hp", max_hp);
    }
};

// Example systems
class MovementSystem : public System {
public:
    void update(World& world, float delta_time) override {
        auto& advanced_world = static_cast<AdvancedWorld&>(world);

        // Use typed query for optimal performance
        movement_query_.for_each(advanced_world,
            [delta_time](EntityHandle entity, Position& pos, const Velocity& vel) {
                pos.x += vel.dx * delta_time;
                pos.y += vel.dy * delta_time;
                pos.z += vel.dz * delta_time;
            });
    }

private:
    TypedQuery<Position, Velocity> movement_query_;
};

class HealthRegenerationSystem : public ReactiveSystem {
public:
    HealthRegenerationSystem() {
        watch_component_modified<Health>();
        set_update_frequency(10.0f); // Update 10 times per second
    }

    void reactive_update(World& world, float delta_time) override {
        auto& advanced_world = static_cast<AdvancedWorld&>(world);

        health_query_.for_each(advanced_world,
            [delta_time](EntityHandle entity, Health& health) {
                if (health.current_hp < health.max_hp) {
                    health.current_hp = std::min(health.max_hp,
                                               health.current_hp + 10.0f * delta_time);
                }
            });
    }

    void on_component_modified(EntityHandle entity, ComponentID component_id) override {
        std::cout << "Health component modified for entity " << entity.id << std::endl;
    }

private:
    TypedQuery<Health> health_query_;
};

void demonstrate_basic_usage() {
    std::cout << "\n=== Basic Usage Demonstration ===\n";

    LoreECS ecs;

    // Create entities
    auto player = ecs.create_entity();
    auto enemy1 = ecs.create_entity();
    auto enemy2 = ecs.create_entity();

    // Add components
    ecs.add_component(player, Position{0.0f, 0.0f, 0.0f});
    ecs.add_component(player, Velocity{1.0f, 0.0f, 0.0f});
    ecs.add_component(player, Health{100.0f});

    ecs.add_component(enemy1, Position{10.0f, 0.0f, 0.0f});
    ecs.add_component(enemy1, Velocity{-0.5f, 0.0f, 0.0f});
    ecs.add_component(enemy1, Health{50.0f});

    ecs.add_component(enemy2, Position{-5.0f, 5.0f, 0.0f});
    ecs.add_component(enemy2, Health{75.0f});

    // Add systems
    ecs.add_system<MovementSystem>();
    ecs.add_system<HealthRegenerationSystem>();

    // Update simulation
    for (int i = 0; i < 5; ++i) {
        ecs.update(0.016f); // 60 FPS

        auto& player_pos = ecs.get_component<Position>(player);
        std::cout << "Frame " << i << ": Player at ("
                  << player_pos.x << ", " << player_pos.y << ", " << player_pos.z << ")\n";
    }

    std::cout << "Entities created: " << ecs.get_entity_count() << std::endl;
}

void demonstrate_queries() {
    std::cout << "\n=== Query System Demonstration ===\n";

    LoreECS ecs;

    // Create many entities
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> vel_dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> health_dist(25.0f, 150.0f);

    std::vector<EntityHandle> entities;
    for (int i = 0; i < 1000; ++i) {
        auto entity = ecs.create_entity();
        entities.push_back(entity);

        ecs.add_component(entity, Position{pos_dist(gen), pos_dist(gen), pos_dist(gen)});

        if (i % 2 == 0) {
            ecs.add_component(entity, Velocity{vel_dist(gen), vel_dist(gen), vel_dist(gen)});
        }

        if (i % 3 == 0) {
            ecs.add_component(entity, Health{health_dist(gen)});
        }
    }

    // Query entities with Position and Velocity
    std::cout << "Entities with Position and Velocity: ";
    std::size_t moving_entities = 0;
    ecs.for_each<Position, Velocity>([&moving_entities](EntityHandle entity, Position& pos, Velocity& vel) {
        moving_entities++;
    });
    std::cout << moving_entities << std::endl;

    // Query entities with Health but without Velocity (stationary objects with health)
    auto stationary_health_query = ecs.create_query<Position, Health>().without<Velocity>();
    std::size_t stationary_count = stationary_health_query.count(ecs.get_world());
    std::cout << "Stationary entities with health: " << stationary_count << std::endl;

    // Enable query caching for performance
    stationary_health_query.enable_caching(true);
    stationary_health_query.cache_results(ecs.get_world());

    auto cached_results = stationary_health_query.get_cached_results();
    std::cout << "Cached query results: " << cached_results.size() << " entities\n";
}

void demonstrate_relationships() {
    std::cout << "\n=== Entity Relationships Demonstration ===\n";

    LoreECS ecs;

    // Create a hierarchy: World -> Player -> Weapon -> Scope
    auto world_entity = ecs.create_entity();
    auto player = ecs.create_entity();
    auto weapon = ecs.create_entity();
    auto scope = ecs.create_entity();

    ecs.add_component(world_entity, Position{0.0f, 0.0f, 0.0f});
    ecs.add_component(player, Position{5.0f, 0.0f, 0.0f});
    ecs.add_component(weapon, Position{0.5f, 0.0f, 0.0f}); // Relative to player
    ecs.add_component(scope, Position{0.1f, 0.1f, 0.0f}); // Relative to weapon

    // Set up hierarchy
    ecs.set_parent(player, world_entity);
    ecs.set_parent(weapon, player);
    ecs.set_parent(scope, weapon);

    // Query parent-child relationships
    auto children = ecs.get_children(player);
    std::cout << "Player has " << children.size() << " children\n";

    auto weapon_parent = ecs.get_parent(weapon);
    std::cout << "Weapon's parent ID: " << weapon_parent.id << std::endl;

    // Query entities that have a specific parent
    auto player_query = ecs.create_query<Position>().with_relationship(player, false);
    std::cout << "Entities that are children of player: " << player_query.count(ecs.get_world()) << std::endl;
}

void demonstrate_serialization() {
    std::cout << "\n=== Serialization Demonstration ===\n";

    LoreECS ecs;

    // Register serializable components
    ecs.register_serializable_component<Position>();
    ecs.register_serializable_component<Velocity>();
    ecs.register_serializable_component<Health>();

    // Create and populate world
    for (int i = 0; i < 100; ++i) {
        auto entity = ecs.create_entity();
        ecs.add_component(entity, Position{static_cast<float>(i), static_cast<float>(i * 2), 0.0f});
        ecs.add_component(entity, Velocity{1.0f, -1.0f, 0.0f});
        ecs.add_component(entity, Health{100.0f - i});
    }

    // Save world state
    std::cout << "Saving world with " << ecs.get_entity_count() << " entities...\n";
    bool saved = ecs.save_world("demo_world.dat", WorldSerializer::Format::Binary);
    std::cout << "Save result: " << (saved ? "Success" : "Failed") << std::endl;

    // Also save as JSON for inspection
    bool saved_json = ecs.save_world("demo_world.json", WorldSerializer::Format::Json);
    std::cout << "JSON save result: " << (saved_json ? "Success" : "Failed") << std::endl;

    // Create new ECS instance and load
    LoreECS ecs2;
    ecs2.register_serializable_component<Position>();
    ecs2.register_serializable_component<Velocity>();
    ecs2.register_serializable_component<Health>();

    bool loaded = ecs2.load_world("demo_world.dat");
    std::cout << "Load result: " << (loaded ? "Success" : "Failed") << std::endl;
    std::cout << "Loaded world has " << ecs2.get_entity_count() << " entities\n";
}

void demonstrate_performance() {
    std::cout << "\n=== Performance Demonstration ===\n";

    // Run comprehensive benchmark
    auto results = ECSBenchmark::run_benchmark(10000);
    ECSBenchmark::log_benchmark_results(results);

    // Demonstrate streaming world for large scale
    LoreECS ecs;

    // Set up world streaming
    float observer_pos[3] = {0.0f, 0.0f, 0.0f};
    float min_bounds[3] = {-1000.0f, -1000.0f, -1000.0f};
    float max_bounds[3] = {1000.0f, 1000.0f, 1000.0f};

    ecs.set_observer_position(observer_pos);
    ecs.set_active_region_bounds(min_bounds, max_bounds);
    ecs.set_lod_distances(100.0f, 500.0f, 1000.0f);

    // Create entities in different regions
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            for (int z = -2; z <= 2; ++z) {
                auto entity = ecs.create_entity_in_region(x, y, z);
                ecs.add_component(entity, Position{x * 100.0f, y * 100.0f, z * 100.0f});
            }
        }
    }

    std::cout << "Created entities across " << ecs.get_active_region_count() << " regions\n";

    // Enable performance monitoring
    ecs.enable_serialization_profiling(true);

    // Update and get performance stats
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        ecs.update(0.016f);
    }
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "100 updates took " << duration.count() << " μs\n";
    std::cout << "Average: " << (duration.count() / 100) << " μs per update\n";

    // Log detailed performance report
    ecs.log_performance_report();
}

void demonstrate_memory_management() {
    std::cout << "\n=== Memory Management Demonstration ===\n";

    LoreECS ecs;

    // Set memory budget (16 MB)
    ecs.set_memory_budget(16 * 1024 * 1024);

    std::cout << "Initial memory usage: " << ecs.get_memory_usage() << " bytes\n";

    // Create many entities
    std::vector<EntityHandle> entities;
    for (int i = 0; i < 10000; ++i) {
        auto entity = ecs.create_entity();
        entities.push_back(entity);
        ecs.add_component(entity, Position{});
        ecs.add_component(entity, Velocity{});
        ecs.add_component(entity, Health{});
    }

    std::cout << "After creating 10,000 entities: " << ecs.get_memory_usage() << " bytes\n";

    // Destroy half the entities
    for (std::size_t i = 0; i < entities.size() / 2; ++i) {
        ecs.destroy_entity(entities[i]);
    }

    std::cout << "After destroying 5,000 entities: " << ecs.get_memory_usage() << " bytes\n";

    // Compact storage
    ecs.compact_storage();
    std::cout << "After compacting storage: " << ecs.get_memory_usage() << " bytes\n";

    // Validate world state
    bool valid = ecs.validate_world_state();
    std::cout << "World state validation: " << (valid ? "PASSED" : "FAILED") << std::endl;
}

int main() {
    std::cout << "Lore Engine - Complete Entity Management System Demo\n";
    std::cout << "==================================================\n";

    try {
        demonstrate_basic_usage();
        demonstrate_queries();
        demonstrate_relationships();
        demonstrate_serialization();
        demonstrate_performance();
        demonstrate_memory_management();

        std::cout << "\n=== All Demonstrations Completed Successfully ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}