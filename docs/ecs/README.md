# Lore Engine - Complete Entity Management System

## Overview

The Lore Entity Management System is a high-performance, feature-complete Entity-Component-System (ECS) designed for massive game world simulation. It supports 1M+ entities with advanced features for modern game development.

## Key Features

### Core Architecture
- **Generation-based Entity IDs**: Safe entity recycling with generation counters prevents use-after-free bugs
- **Sparse Set Storage**: Optimal memory layout with O(1) component access and cache-friendly iteration
- **Type-safe Components**: Compile-time type checking with zero-overhead abstractions
- **SIMD Optimization**: Vectorized operations for bulk entity processing

### Advanced Features
- **World State Management**: Support for streaming worlds with Level-of-Detail (LOD) systems
- **Component Dependencies**: Automatic dependency tracking and update ordering
- **Reactive Systems**: Event-driven systems that respond to component changes
- **Entity Relationships**: Hierarchical parent-child relationships with efficient queries
- **Thread Safety**: Lock-free and concurrent-safe access patterns
- **Memory Pooling**: Custom allocators for optimal memory usage
- **Serialization**: Binary and JSON serialization for save/load functionality
- **Performance Profiling**: Built-in profiling and debugging tools

### Performance Characteristics
- **<1ms** typical frame operations
- **<1KB** average memory per entity
- **100K+** entities per frame processing
- **Linear scaling** with entity count
- **Cache-friendly** component iteration

## Quick Start

### Basic Usage

```cpp
#include <lore/ecs/advanced_ecs.hpp>

using namespace lore::ecs;

// Define components
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };

// Create ECS instance
LoreECS ecs;

// Create entities
auto player = ecs.create_entity();
ecs.add_component(player, Position{0.0f, 0.0f, 0.0f});
ecs.add_component(player, Velocity{1.0f, 0.0f, 0.0f});

// Query and iterate
ecs.for_each<Position, Velocity>([](EntityHandle entity, Position& pos, const Velocity& vel) {
    pos.x += vel.dx * 0.016f; // 60 FPS update
});

// Update systems
ecs.update(0.016f);
```

### System Development

```cpp
class MovementSystem : public System {
public:
    void update(World& world, float delta_time) override {
        auto& advanced_world = static_cast<AdvancedWorld&>(world);

        // Use typed queries for optimal performance
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

// Add system to ECS
ecs.add_system<MovementSystem>();
```

### Advanced Queries

```cpp
// Query with exclusions
auto query = ecs.create_query<Position, Health>()
    .without<Velocity>()  // Stationary entities only
    .in_lod_level(LODManager::LODLevel::High);

// Parallel execution
query.for_each_parallel(ecs.get_world(),
    [](EntityHandle entity, Position& pos, Health& health) {
        // Process entities in parallel
    });

// Cached queries for repeated use
query.enable_caching(true);
query.cache_results(ecs.get_world());
auto results = query.get_cached_results();
```

## Architecture Deep Dive

### Entity Management

#### Generation-Based IDs
```cpp
struct EntityHandle {
    Entity id;          // 32-bit entity ID
    Generation generation; // 16-bit generation counter
};
```

- **Safe Recycling**: When entities are destroyed, their ID is recycled with an incremented generation
- **Use-After-Free Protection**: Old handles become invalid when generation mismatches
- **Memory Efficiency**: Reuses entity slots instead of growing indefinitely

#### World Streaming
```cpp
// Set up streaming world
ecs.set_observer_position(camera_position);
ecs.set_active_region_bounds(min_bounds, max_bounds);
ecs.set_lod_distances(100.0f, 500.0f, 1000.0f);

// Create entities in specific regions
auto entity = ecs.create_entity_in_region(x, y, z);
```

### Component Storage

#### Sparse Set Architecture
```cpp
template<typename T>
class ComponentArray {
    std::vector<std::uint32_t> sparse_;     // entity -> dense index
    std::vector<Entity> dense_entities_;    // dense index -> entity
    std::vector<T> dense_components_;       // component data
};
```

**Benefits:**
- O(1) component access by entity
- Cache-friendly iteration over dense arrays
- Automatic memory compaction
- SIMD-friendly data layout

#### Archetype System
```cpp
class ComponentArchetype {
    ComponentBitSet component_mask_;        // Which components this archetype has
    std::vector<EntityHandle> entities_;    // Entities in this archetype
};
```

**Benefits:**
- Groups entities with identical component sets
- Enables efficient batch processing
- Supports complex query optimization
- Reduces memory fragmentation

### Query System

#### Typed Queries
```cpp
template<typename... RequiredComponents>
class TypedQuery {
    // Compile-time component type checking
    // Optimized iteration patterns
    // Automatic SIMD utilization
};
```

#### SIMD Optimization
```cpp
class SIMDQuery {
    static constexpr std::size_t BATCH_SIZE = 8;    // Process 8 entities at once
    static constexpr std::size_t SIMD_ALIGNMENT = 32; // AVX2 alignment

    template<typename ComponentType>
    void process_components_vectorized(
        const AdvancedWorld& world,
        std::function<void(ComponentType*, std::size_t)> processor
    ) const;
};
```

### Memory Management

#### Component Memory Pools
```cpp
class ComponentMemoryPool {
    std::size_t component_size_;
    std::size_t alignment_;
    std::vector<std::unique_ptr<std::byte[]>> memory_chunks_;
    std::vector<void*> free_blocks_;
};
```

**Features:**
- Per-component-type memory pools
- Aligned allocation for SIMD operations
- Automatic garbage collection
- Memory fragmentation tracking

#### World Memory Budget
```cpp
// Set memory limits
ecs.set_memory_budget(512 * 1024 * 1024); // 512 MB

// Monitor usage
auto stats = ecs.get_performance_stats();
std::cout << "Memory usage: " << stats.memory_usage_bytes << " bytes\n";
std::cout << "Fragmentation: " << stats.memory_fragmentation << "%\n";
```

### Serialization System

#### Component Serialization
```cpp
// Register component for serialization
ecs.register_serializable_component<Position>();

// Components can provide custom serialization
struct CustomComponent {
    void serialize(BinaryArchive& archive) const;
    void deserialize(BinaryArchive& archive);
    void serialize(JsonArchive& archive) const;
    void deserialize(JsonArchive& archive);
};
```

#### World State Persistence
```cpp
// Save entire world state
ecs.save_world("game_save.dat", WorldSerializer::Format::Binary);

// Save specific entities
std::vector<EntityHandle> important_entities = {...};
ecs.save_entities(important_entities, "checkpoint.dat");

// Load world state
LoreECS new_ecs;
new_ecs.load_world("game_save.dat");
```

#### Streaming Serialization
```cpp
// For very large worlds
auto stream = serializer.create_serialization_stream("huge_world.dat");
stream->write_metadata(metadata);

for (auto entity : world.get_all_entities()) {
    stream->write_entity(serialize_entity(entity));
}

stream->finalize();
```

### Performance Monitoring

#### Built-in Profiling
```cpp
// Enable performance tracking
ecs.enable_serialization_profiling(true);

// Get detailed performance stats
auto stats = ecs.get_performance_stats();
auto system_perf = ecs.get_system_performance();

// Log comprehensive report
ecs.log_performance_report();
```

#### Benchmarking
```cpp
// Run comprehensive benchmark
auto results = ECSBenchmark::run_benchmark(100000);
ECSBenchmark::log_benchmark_results(results);

// Example output:
// Entity creation time: 1234 μs
// Component addition time: 2345 μs
// Query execution time: 456 μs
// Entities per second: 123456
// Memory efficiency: 89.5%
```

## Advanced Features

### Entity Relationships

```cpp
// Create hierarchy
auto parent = ecs.create_entity();
auto child = ecs.create_entity();
ecs.set_parent(child, parent);

// Query relationships
auto children = ecs.get_children(parent);
auto parent_entity = ecs.get_parent(child);

// Query by relationship
auto query = ecs.create_query<Transform>()
    .with_relationship(parent, false); // All children of parent
```

### Reactive Systems

```cpp
class HealthSystem : public ReactiveSystem {
public:
    HealthSystem() {
        watch_component_modified<Health>();
        set_update_frequency(10.0f); // 10 Hz
    }

    void on_component_modified(EntityHandle entity, ComponentID component_id) override {
        // React to health changes
        auto& health = world.get_component<Health>(entity);
        if (health.current_hp <= 0) {
            // Handle death
        }
    }
};
```

### Component Dependencies

```cpp
// Register dependencies
dependency_manager.add_dependency<Physics, Transform>(); // Physics depends on Transform

// Automatic update ordering
auto update_order = dependency_manager.get_update_order();
// Result: [Transform, Physics, Rendering, ...]
```

### Thread Safety

```cpp
// Enable thread-safe operations
ecs.enable_thread_safety(true);

// Safe concurrent access
std::vector<std::thread> workers;
for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
    workers.emplace_back([&ecs]() {
        // Safe to access ECS from multiple threads
        auto entity = ecs.create_entity();
        ecs.add_component(entity, Position{});
    });
}

for (auto& worker : workers) {
    worker.join();
}
```

## Performance Guidelines

### Entity Creation
- **Batch Creation**: Use `create_entities(count)` for many entities
- **Pre-reserve**: Call `reserve_entities(count)` when entity count is known
- **Region Placement**: Use `create_entity_in_region()` for spatial optimization

### Component Management
- **Batch Operations**: Use `add_components_batch()` and `remove_components_batch()`
- **Component Pools**: Configure pools with `ComponentPoolManager::configure_pool<T>()`
- **Memory Alignment**: Ensure components are aligned for SIMD operations

### Query Optimization
- **Use Typed Queries**: `TypedQuery<T...>` is faster than generic queries
- **Enable Caching**: For frequently-used queries, enable result caching
- **SIMD Queries**: Use `SIMDQuery` for bulk operations on trivial components
- **Exclude Components**: Use `.without<T>()` to filter out unwanted entities

### System Design
- **Data Locality**: Access components in the order they're stored
- **Minimize Dependencies**: Reduce system dependencies for better parallelization
- **Batch Processing**: Process entities in groups rather than individually
- **LOD Systems**: Use Level-of-Detail to reduce processing for distant entities

### Memory Management
- **Set Budgets**: Use `set_memory_budget()` to control memory usage
- **Compact Regularly**: Call `compact_storage()` to defragment memory
- **Monitor Usage**: Track memory usage with performance stats
- **Pool Configuration**: Configure component pools based on usage patterns

## Integration Examples

### Physics Integration

```cpp
struct RigidBody {
    float mass;
    float drag;
    bool is_kinematic;
};

class PhysicsSystem : public System {
public:
    void update(World& world, float delta_time) override {
        // Update physics for all rigid bodies
        physics_query_.for_each_parallel(static_cast<AdvancedWorld&>(world),
            [delta_time](EntityHandle entity, Transform& transform, RigidBody& body, Velocity& velocity) {
                if (!body.is_kinematic) {
                    // Apply physics simulation
                    velocity.dx *= (1.0f - body.drag * delta_time);
                    velocity.dy *= (1.0f - body.drag * delta_time);
                    velocity.dz *= (1.0f - body.drag * delta_time);

                    transform.position[0] += velocity.dx * delta_time;
                    transform.position[1] += velocity.dy * delta_time;
                    transform.position[2] += velocity.dz * delta_time;
                }
            });
    }

private:
    TypedQuery<Transform, RigidBody, Velocity> physics_query_;
};
```

### Graphics Integration

```cpp
struct Renderable {
    uint32_t mesh_id;
    uint32_t material_id;
    bool visible;
    float lod_bias;
};

class RenderSystem : public System {
public:
    void update(World& world, float delta_time) override {
        auto& advanced_world = static_cast<AdvancedWorld&>(world);

        // Only render high-LOD visible entities
        auto render_query = advanced_world.create_query<Transform, Renderable>()
            .in_lod_level(LODManager::LODLevel::High);

        render_query.for_each(advanced_world,
            [this](EntityHandle entity, const Transform& transform, const Renderable& renderable) {
                if (renderable.visible) {
                    // Submit to graphics pipeline
                    submit_for_rendering(entity, transform, renderable);
                }
            });
    }

private:
    void submit_for_rendering(EntityHandle entity, const Transform& transform, const Renderable& renderable);
};
```

### Audio Integration

```cpp
struct AudioSource {
    uint32_t sound_id;
    float volume;
    float pitch;
    bool is_3d;
    float min_distance;
    float max_distance;
};

class AudioSystem : public System {
public:
    void update(World& world, float delta_time) override {
        auto& advanced_world = static_cast<AdvancedWorld&>(world);

        // Process audio sources with spatial audio
        audio_query_.for_each(advanced_world,
            [this](EntityHandle entity, const Transform& transform, AudioSource& audio) {
                if (audio.is_3d) {
                    float distance = calculate_distance_to_listener(transform.position);
                    float attenuation = calculate_attenuation(distance, audio.min_distance, audio.max_distance);
                    update_3d_audio(entity, audio, attenuation);
                }
            });
    }

private:
    TypedQuery<Transform, AudioSource> audio_query_;

    float calculate_distance_to_listener(const float* position);
    float calculate_attenuation(float distance, float min_dist, float max_dist);
    void update_3d_audio(EntityHandle entity, AudioSource& audio, float attenuation);
};
```

## Best Practices

### Component Design
1. **Keep Components Data-Only**: Components should be plain data structures
2. **Use Composition**: Prefer multiple small components over large complex ones
3. **Align for SIMD**: Ensure components are properly aligned for vectorization
4. **Implement Serialization**: Always provide serialization for persistent components

### System Design
1. **Single Responsibility**: Each system should have one clear purpose
2. **Minimize State**: Systems should be stateless when possible
3. **Use Reactive Patterns**: Respond to events rather than polling
4. **Optimize Hot Paths**: Profile and optimize frequently-executed code

### Performance Optimization
1. **Profile First**: Use built-in profiling to identify bottlenecks
2. **Batch Operations**: Group similar operations together
3. **Cache Query Results**: Enable caching for expensive queries
4. **Use Parallel Execution**: Leverage multi-threading for independent systems

### Memory Management
1. **Monitor Usage**: Track memory consumption and fragmentation
2. **Set Reasonable Budgets**: Configure memory limits based on target hardware
3. **Compact Regularly**: Defragment memory during loading screens or pauses
4. **Pool Resources**: Use memory pools for frequently allocated objects

## Troubleshooting

### Common Issues

#### Entity Handle Invalidation
```cpp
// WRONG: Using handle after entity destruction
auto entity = ecs.create_entity();
ecs.destroy_entity(entity);
ecs.add_component(entity, Position{}); // ERROR: Invalid handle

// CORRECT: Check validity
if (ecs.is_valid(entity)) {
    ecs.add_component(entity, Position{});
}
```

#### Memory Leaks
```cpp
// WRONG: Not cleaning up resources
class ResourceComponent {
    void* raw_pointer; // Not automatically cleaned up
};

// CORRECT: Use RAII
class ResourceComponent {
    std::unique_ptr<Resource> resource; // Automatically cleaned up
};
```

#### Performance Issues
```cpp
// WRONG: Iterating over all entities
for (auto entity : all_entities) {
    if (ecs.has_component<Transform>(entity)) {
        // Slow individual checks
    }
}

// CORRECT: Use queries
ecs.for_each<Transform>([](EntityHandle entity, Transform& transform) {
    // Fast bulk iteration
});
```

### Debugging Tools

#### World State Validation
```cpp
bool valid = ecs.validate_world_state();
if (!valid) {
    std::cout << "World state corruption detected!\n";
    // Check logs for specific issues
}
```

#### Performance Analysis
```cpp
// Enable detailed profiling
ecs.enable_serialization_profiling(true);

// Get system performance data
auto system_perf = ecs.get_system_performance();
for (const auto& perf : system_perf) {
    if (perf.average_execution_time.count() > 1000) { // > 1ms
        std::cout << "Slow system: " << perf.name << "\n";
    }
}
```

#### Memory Analysis
```cpp
auto stats = ecs.get_performance_stats();
if (stats.memory_fragmentation > 0.3f) { // > 30% fragmentation
    std::cout << "High memory fragmentation, consider compacting\n";
    ecs.compact_storage();
}
```

## API Reference

For complete API documentation, see the header files:
- `lore/ecs/advanced_ecs.hpp` - Main ECS interface
- `lore/ecs/entity_manager.hpp` - Entity lifecycle management
- `lore/ecs/world_manager.hpp` - World state and streaming
- `lore/ecs/component_tracking.hpp` - Component relationships and queries
- `lore/ecs/serialization.hpp` - Save/load functionality

## Contributing

When contributing to the ECS system:

1. **Maintain Performance**: All changes must maintain or improve performance
2. **Add Tests**: Include comprehensive tests for new features
3. **Update Documentation**: Keep documentation current with changes
4. **Follow Conventions**: Use existing naming and coding conventions
5. **Profile Changes**: Benchmark performance impact of modifications

## License

The Lore Engine ECS system is part of the Lore Engine project. See the main project license for details.