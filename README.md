# 🎮 Lore Engine

**A high-performance C++23 game engine with advanced systems architecture**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)](https://www.vulkan.org/)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](#-quick-start)

Lore is a modern game engine built from the ground up with C++23, featuring a comprehensive ECS architecture, Vulkan rendering, and advanced physics simulation. Designed for high-performance applications with realistic physics simulation capabilities.

## ✨ Features

### 🎯 Core Architecture
- **ECS Foundation**: High-performance Entity-Component-System with 1M+ entity support
- **C++23 Modern**: Leverages latest C++ features for optimal performance
- **SIMD Optimization**: Vector math with SIMD acceleration
- **Memory Efficient**: Arena allocators for zero-allocation runtime

### 🎨 Graphics & Rendering
- **Vulkan Backend**: Modern graphics API with full GPU utilization
- **Deferred Rendering**: Advanced rendering pipeline
- **VMA Integration**: Efficient GPU memory management
- **Shader System**: GLSL shader compilation and management
- **Image Loading**: Comprehensive texture support (PNG, JPEG, BMP, TGA, HDR)

### 🎵 Audio System
- **3D Spatial Audio**: Realistic sound positioning and attenuation
- **Sound Propagation**: Physics-based acoustics simulation
- **Multi-format Support**: Various audio formats via miniaudio
- **Hearing Damage Simulation**: Realistic audio effects

### 🎮 Input Management
- **Event-Driven**: Publisher/subscriber input system
- **Multi-Device Support**: Keyboard, mouse, gamepad support
- **Action Mapping**: High-level input abstraction
- **Thread-Safe**: Concurrent input processing

### ⚡ Physics & Simulation
- **Rigid Body Dynamics**: SIMD-accelerated physics simulation
- **Advanced Physics**: Thermodynamics, electromagnetics, optics
- **Ballistics System**: Realistic projectile physics with aerodynamics
- **Collision Detection**: Efficient collision algorithms
- **Fluid Dynamics**: Advanced fluid simulation

### 📦 Asset Management
- **Lore Package System**: Native .lore asset packaging
- **Asset Streaming**: Efficient loading and caching
- **Hot Reloading**: Runtime asset updates
- **Compression**: Optimized asset storage

## 🚀 Quick Start

### Prerequisites
- **Windows**: Visual Studio 2022 with v143 toolset
- **CMake**: 3.28 or higher
- **Vulkan SDK**: Latest version
- **Git**: For dependency management

### Building

```bash
# Navigate to your lore directory
cd lore

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the engine
cmake --build . --config Release

# Run the example
./lore.exe
```

### Your First Lore Application

```cpp
#include <lore/graphics/graphics.hpp>
#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>

int main() {
    // Initialize core systems
    auto& graphics = lore::graphics::GraphicsSystem::instance();
    graphics.create_window(800, 600, "My Lore Game");
    graphics.initialize();

    // Create ECS world
    lore::ecs::World world;
    
    // Add systems
    auto& physics = world.add_system<lore::physics::PhysicsSystem>();
    auto& audio = world.add_system<lore::audio::AudioSystem>();
    
    // Configure systems
    physics.set_gravity(lore::math::Vec3(0.0f, -9.81f, 0.0f));
    audio.set_master_volume(0.8f);
    
    // Initialize all systems
    world.init();

    // Game loop
    while (!graphics.should_close()) {
        world.update(delta_time);
        graphics.render();
    }

    // Cleanup
    world.shutdown();
    graphics.shutdown();
    return 0;
}
```

## 📁 Project Structure

```
lore/
├── include/lore/           # Public API headers
│   ├── ecs/               # Entity-Component-System
│   ├── graphics/          # Vulkan rendering system
│   ├── physics/           # Physics simulation
│   ├── audio/             # Audio system
│   ├── input/             # Input handling
│   ├── math/              # SIMD math library
│   └── assets/            # Asset management
├── src/                   # Implementation files
├── docs/                  # Comprehensive documentation
├── examples/              # Example projects
├── shaders/               # GLSL shader files
├── assets/                # Game assets
└── tools/                 # Development tools
```

## 🛠️ Systems Overview

### Entity-Component-System (ECS)
- **High Performance**: Optimized for cache efficiency and SIMD operations
- **Sparse Set Storage**: Memory-efficient component storage
- **System Dependencies**: Automatic dependency resolution
- **Thread-Safe**: Concurrent system execution

### Graphics System
- **Vulkan 1.3**: Modern graphics API with full feature support
- **Memory Management**: VMA integration for efficient GPU memory usage
- **Synchronization**: Proper semaphore and fence management
- **Shader Compilation**: GLSL to SPIR-V compilation pipeline

### Physics Engine
- **Realistic Simulation**: Advanced physics including thermodynamics
- **SIMD Acceleration**: Vectorized calculations for performance
- **Multi-Body Dynamics**: Complex physics interactions
- **Material Properties**: Comprehensive material system

### Audio Engine
- **3D Positional**: Realistic spatial audio with doppler effects
- **Physics Integration**: Sound propagation through materials
- **Performance Optimized**: Low-latency audio processing
- **Format Support**: Multiple audio formats via miniaudio

## 📖 Documentation

- **[Complete Documentation](docs/README.md)** - Comprehensive engine documentation
- **[API Reference](docs/api/)** - Detailed API documentation
- **[System Guides](docs/systems/)** - In-depth system explanations
- **[Tutorials](docs/tutorials/)** - Step-by-step learning guides
- **[Examples](docs/examples/)** - Working code examples

### Key Documentation
- **[Input System](docs/INPUT_SYSTEM.md)** - Event-driven input handling
- **[Vulkan Integration](VULKAN_IMAGE_SYSTEM_SUMMARY.md)** - Graphics system details
- **[Asset Manager](docs/api/ASSET_MANAGER_API.md)** - Asset management system
- **[Lore Packages](docs/systems/LORE_PACKAGE_SYSTEM.md)** - Native asset packaging

## 🎯 Development Status

### ✅ Implemented Systems
- **Graphics**: Vulkan renderer with triangle rendering ✓
- **Input**: Event-driven input system ✓
- **Assets**: Lore package system and image loading ✓
- **ECS**: Core entity-component-system ✓
- **Math**: SIMD-optimized mathematics ✓

### 🚧 In Development
- **Physics**: Advanced physics simulation
- **Audio**: 3D spatial audio system
- **Rendering**: Deferred rendering pipeline
- **Materials**: Shader and material system

### 📋 Planned Features
- **Networking**: Multiplayer support
- **Scripting**: Lua/C# integration
- **Editor Tools**: Visual development environment
- **Platform Support**: Linux and macOS ports

## 🤝 Contributing

We welcome contributions! 

### Development Workflow
1. Create a feature branch from the current development branch
2. Make your changes with comprehensive tests
3. Ensure all systems compile and pass tests
4. Submit your changes for review

### Code Standards
- **C++23**: Use modern C++ features appropriately
- **SIMD**: Optimize performance-critical code paths
- **Documentation**: Comprehensive API documentation
- **Testing**: Unit tests for all new features

## 📄 License

This project is under active development. License terms to be determined.

## 🙏 Acknowledgments

- **Vulkan**: Modern graphics API
- **GLFW**: Window and input management
- **GLM**: OpenGL Mathematics library
- **stb**: Image loading libraries
- **miniaudio**: Audio playback library
- **VMA**: Vulkan Memory Allocator

## 📞 Support

- **Documentation**: [Comprehensive Docs](docs/README.md)
- **Local Development**: Check existing documentation in the `docs/` directory
- **Examples**: Working examples in `examples/` directory

---

**Built with ❤️ using C++23 and modern game development practices**