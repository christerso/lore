# Lore Engine Documentation

Welcome to the comprehensive documentation for the Lore C++23 game engine. This documentation covers all aspects of the engine, from core systems to advanced features and development workflows.

## Documentation Structure

The Lore engine documentation is organized into several key sections:

### 📚 Systems Documentation (`/systems/`)

Core engine systems and their technical specifications:

- **[Lore Package System](systems/LORE_PACKAGE_SYSTEM.md)** - Native asset packaging using .lore files
- **Asset Management** - Asset loading, caching, and memory management
- **ECS Architecture** - Entity-Component-System implementation
- **Rendering Pipeline** - Graphics rendering and GPU resource management
- **Audio System** - Sound processing and audio resource management
- **Physics Integration** - Physics simulation and collision detection
- **Input Handling** - Input processing and event management

### 🔧 API Reference (`/api/`)

Detailed API documentation for developers:

- **[Asset Manager API](api/ASSET_MANAGER_API.md)** - Complete API reference for asset management
- **ECS API** - Entity, Component, and System APIs
- **Graphics API** - Rendering and GPU resource APIs
- **Audio API** - Audio processing and playback APIs
- **Input API** - Input handling and event APIs
- **Core Utilities** - Math, memory, and utility APIs

### 📖 Tutorials (`/tutorials/`)

Step-by-step guides for common tasks:

- **Getting Started** - Initial setup and first game
- **Asset Creation** - Creating and packaging game assets
- **ECS Basics** - Working with entities, components, and systems
- **Rendering Guide** - Setting up graphics and shaders
- **Audio Integration** - Adding sound to your game
- **Input Handling** - Implementing player controls
- **Performance Optimization** - Profiling and optimization techniques

### 💡 Examples (`/examples/`)

Complete code examples and sample projects:

- **Minimal Game** - Simplest possible Lore game
- **2D Platformer** - 2D game with physics and animation
- **3D Demo** - 3D rendering showcase
- **Asset Pipeline** - Complete asset creation workflow
- **Multi-threaded Game** - Advanced threading examples
- **Performance Benchmarks** - Performance testing and profiling

## Quick Start Guide

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 19.34+)
- CMake 3.25 or later
- Git for version control

### Installation

```bash
# Clone the Lore engine
git clone https://github.com/lore-engine/lore.git
cd lore

# Configure with CMake
cmake -B build -DLORE_BUILD_EXAMPLES=ON

# Build the engine
cmake --build build --config Release
```

### Your First Lore Game

```cpp
#include <lore/lore.hpp>

int main() {
    // Initialize Lore engine
    lore::Engine engine;

    // Create a basic game world
    auto world = engine.create_world();

    // Add a simple entity
    auto entity = world->create_entity();
    world->add_component<lore::Transform>(entity, {.position = {0, 0, 0}});
    world->add_component<lore::Renderable>(entity, {.model_path = "models/cube.gltf"});

    // Run the game loop
    return engine.run();
}
```

## Core Concepts

### Lore Package System

The heart of Lore's asset management is the native **.lore package format**:

- **High Performance**: Optimized for fast loading and minimal memory usage
- **Compression**: Multiple compression algorithms for different use cases
- **Integrity**: Built-in validation and error recovery
- **Streaming**: Efficient asset streaming for large worlds
- **Hot Reload**: Real-time asset updates during development

```cpp
// Loading a .lore package
auto asset_manager = lore::assets::AssetManager();
asset_manager.load_package("game_assets.lore");

// Loading individual assets
auto texture = asset_manager.load_asset<lore::Texture>("textures/player.png");
auto model = asset_manager.load_asset<lore::Model>("models/character.gltf");
```

### Entity-Component-System (ECS)

Lore uses a high-performance ECS architecture:

```cpp
// Create entities and add components
auto player = world->create_entity();
world->add_component<Transform>(player, {.position = {0, 0, 0}});
world->add_component<Velocity>(player, {.velocity = {0, 0, 0}});
world->add_component<Health>(player, {.current = 100, .max = 100});

// Systems process components
class MovementSystem : public lore::System {
public:
    void update(lore::World& world, float delta_time) override {
        world.view<Transform, Velocity>().each([delta_time](auto& transform, const auto& velocity) {
            transform.position += velocity.velocity * delta_time;
        });
    }
};
```

### Modern C++23 Features

Lore leverages the latest C++23 features for safety and performance:

- **Modules**: Fast compilation and better dependency management
- **Concepts**: Type-safe templates and better error messages
- **Coroutines**: Async asset loading and smooth frame pacing
- **Ranges**: Efficient data processing pipelines
- **Safety**: RAII, smart pointers, and bounds checking

## Development Workflow

### Asset Pipeline

1. **Create Assets**: Use your favorite tools (Blender, Photoshop, etc.)
2. **Package Assets**: Convert to .lore format using LorePackageBuilder
3. **Load Assets**: Use AssetManager to load packages in your game
4. **Hot Reload**: Enable live updates during development

```cpp
// Package creation
LorePackageBuilder builder("game_assets.lore");
builder
    .add_directory("assets/textures/", AssetType::Texture2D)
    .add_directory("assets/models/", AssetType::StaticMesh)
    .add_directory("assets/audio/", AssetType::AudioClip)
    .set_compression(CompressionType::ZSTD)
    .build();
```

### Performance Profiling

Lore includes comprehensive profiling tools:

```cpp
// Enable profiling
engine.enable_profiling(true);

// Get performance statistics
auto stats = engine.get_performance_stats();
std::cout << "Frame time: " << stats.frame_time.count() << "ms" << std::endl;
std::cout << "Memory usage: " << stats.memory_usage_mb << "MB" << std::endl;
```

### Testing and Debugging

- **Unit Tests**: Comprehensive test suite for all engine systems
- **Integration Tests**: End-to-end testing of complete workflows
- **Debug Tools**: Visual debugging, performance overlays, and asset inspection
- **Memory Safety**: Built-in leak detection and bounds checking

## Architecture Overview

```
┌─────────────────────────────────────────┐
│              Game Layer                 │
├─────────────────────────────────────────┤
│            Lore Engine API              │
├─────────────────────────────────────────┤
│  ECS   │ Assets │ Graphics │ Audio │ ... │
├─────────────────────────────────────────┤
│         Core Systems Layer              │
│    (Memory, Threading, File I/O)        │
├─────────────────────────────────────────┤
│           Platform Layer                │
│      (Windows, Linux, macOS)            │
└─────────────────────────────────────────┘
```

### Key Design Principles

1. **Performance First**: Every system is designed for maximum performance
2. **Memory Safety**: RAII and smart pointers prevent memory leaks
3. **Thread Safety**: Full concurrent access support where needed
4. **Modularity**: Use only the systems you need
5. **Developer Experience**: Clear APIs and comprehensive error messages

## Platform Support

Lore engine supports multiple platforms:

- **Windows 10/11** (Primary development platform)
- **Linux** (Ubuntu 20.04+, Arch Linux, Fedora)
- **macOS** (macOS 12+, Apple Silicon support)

### Graphics APIs

- **Vulkan** (Primary renderer, all platforms)
- **DirectX 12** (Windows alternative)
- **Metal** (macOS, experimental)

## Performance Characteristics

### Memory Management

- **Custom Allocators**: Optimized for game workloads
- **Object Pooling**: Reduce allocation overhead
- **Memory Budgets**: Configurable limits for different systems
- **Garbage Collection**: Optional for managed resources

### Threading

- **Job System**: Task-based parallelism for all engine systems
- **Thread Pool**: Configurable worker threads
- **Lock-Free**: Where possible, using atomic operations
- **SIMD**: Vectorized operations for math and data processing

### Asset Loading

- **Async Loading**: Non-blocking asset operations
- **Streaming**: Load assets on-demand
- **Compression**: Multiple algorithms optimized for different content
- **Caching**: Intelligent memory management

## Contributing

We welcome contributions to the Lore engine! Please see our contribution guidelines:

1. **Read the Documentation**: Understand the systems you're working with
2. **Follow Coding Standards**: Use modern C++23 practices
3. **Write Tests**: All new features must have comprehensive tests
4. **Update Documentation**: Keep docs in sync with code changes

### Development Setup

```bash
# Fork and clone
git clone https://github.com/your-username/lore.git
cd lore

# Install development dependencies
./scripts/install-deps.sh

# Build in debug mode
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLORE_ENABLE_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## Support and Community

- **Documentation**: You're reading it! Check the links above for specific topics
- **GitHub Issues**: Report bugs and request features
- **Discussions**: Ask questions and share ideas
- **Discord**: Real-time chat with the community
- **Examples**: Complete sample projects to learn from

## License

The Lore engine is licensed under the MIT License. See [LICENSE](../LICENSE) for full details.

---

**Ready to build amazing games with Lore?** Start with the [Getting Started Tutorial](tutorials/getting-started.md) or explore the [Examples](examples/) directory!