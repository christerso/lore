# Lore Package System Documentation

## Overview

The Lore Package System is the native asset management solution for the Lore C++23 game engine. It provides high-performance asset packaging, compression, streaming, and management capabilities using the native ".lore" file format. The system is designed with a focus on safety, performance, and ease of use, specifically tailored for the Lore engine's architecture.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Components](#core-components)
3. [Package Format Specification](#package-format-specification)
4. [API Reference](#api-reference)
5. [ECS Integration](#ecs-integration)
6. [Performance Features](#performance-features)
7. [Development Workflow](#development-workflow)
8. [Best Practices](#best-practices)

## Architecture Overview

The Lore Package System follows a layered architecture integrated seamlessly with the Lore engine:

```
┌─────────────────────────────────────────┐
│           Lore Game Application         │
├─────────────────────────────────────────┤
│         ECS Integration Layer           │
│   (AssetComponent, AssetSystem)         │
├─────────────────────────────────────────┤
│          Asset Manager API              │
│    (AssetManager, AssetHandle)          │
├─────────────────────────────────────────┤
│        Lore Package Layer               │
│  (LorePackage, LorePackageBuilder)      │
├─────────────────────────────────────────┤
│         Compression Layer               │
│      (LZ4, ZSTD, Integrity)            │
├─────────────────────────────────────────┤
│          File System Layer              │
│    (Memory Mapping, Hot Reload)         │
└─────────────────────────────────────────┘
```

### Key Design Principles

1. **Safety First**: All operations are bounds-checked with comprehensive error handling
2. **Zero-Copy Design**: Minimal memory copying through span-based interfaces
3. **Thread Safety**: Full concurrent access support with reader-writer locks
4. **Performance**: SIMD optimizations, memory-mapped files, and cache-friendly data structures
5. **Native Integration**: Deep integration with Lore engine systems and ECS architecture

## Core Components

### AssetManager

The central hub for all asset operations in the Lore engine:

```cpp
class AssetManager {
public:
    // Package management
    Result<void> load_package(const std::filesystem::path& package_path);
    void unload_package(const std::string& package_name);

    // Asset loading
    template<typename T>
    Result<AssetHandle<T>> load_asset(const std::string& asset_path);

    // Async operations
    template<typename T>
    std::future<Result<AssetHandle<T>>> load_asset_async(const std::string& asset_path);

    // Memory management
    void set_memory_budget(std::size_t bytes);
    void garbage_collect();
};
```

### AssetHandle

Smart pointer-style asset access with automatic lifetime management:

```cpp
template<typename T>
class AssetHandle {
public:
    // Access operations
    const T* get() const noexcept;
    const T& operator*() const noexcept;
    const T* operator->() const noexcept;

    // State queries
    bool is_valid() const noexcept;
    bool is_loaded() const noexcept;
    LoadState get_load_state() const noexcept;

    // Metadata access
    const AssetMetadata& get_metadata() const noexcept;
};
```

### LorePackage

Low-level package file interface for .lore files:

```cpp
class LorePackage {
public:
    // Package operations
    static Result<std::unique_ptr<LorePackage>> open(const std::filesystem::path& path);

    // Asset queries
    std::vector<std::uint32_t> get_asset_ids() const;
    Result<AssetEntry> get_asset_entry(std::uint32_t asset_id) const;
    Result<AssetStream> get_asset_stream(std::uint32_t asset_id) const;

    // Package information
    const PackageHeader& get_header() const noexcept;
    std::size_t get_asset_count() const noexcept;
};
```

## Package Format Specification

### Binary Layout

Lore packages (.lore files) use a binary format optimized for random access and streaming:

```
┌─────────────────────────────────────────┐
│           Package Header                │  256 bytes
│         (magic, version, etc.)          │
├─────────────────────────────────────────┤
│           Asset Table                   │  N × 128 bytes
│       (one entry per asset)             │
├─────────────────────────────────────────┤
│         String Table                    │  Variable size
│      (asset names, paths)               │
├─────────────────────────────────────────┤
│        Dependency Table                 │  Variable size
│     (asset dependencies)                │
├─────────────────────────────────────────┤
│         Metadata Table                  │  Variable size
│    (key-value metadata)                 │
├─────────────────────────────────────────┤
│           Asset Data                    │  Variable size
│      (compressed blobs)                 │
└─────────────────────────────────────────┘
```

### Header Structure

```cpp
struct PackageHeader {
    std::uint32_t magic = 0x45524F4C;  // "LORE"
    std::uint16_t major_version = 1;
    std::uint16_t minor_version = 0;
    std::uint16_t patch_version = 0;

    std::uint32_t asset_count;
    std::uint64_t asset_table_offset;
    std::uint64_t string_table_offset;
    std::uint64_t dependency_table_offset;
    std::uint64_t metadata_table_offset;
    std::uint64_t data_section_offset;

    CompressionType default_compression;
    HashType integrity_hash_type;
    std::uint8_t package_hash[32];  // SHA256

    std::chrono::system_clock::time_point creation_time;
    std::array<char, 64> creator_info;
};
```

### Asset Entry Structure

```cpp
struct AssetEntry {
    std::uint32_t asset_id;
    AssetType type;
    std::uint32_t name_offset;      // Into string table
    std::uint32_t path_offset;      // Into string table

    std::uint64_t data_offset;      // Into data section
    std::uint64_t compressed_size;
    std::uint64_t uncompressed_size;

    CompressionType compression;
    HashType hash_type;
    std::uint8_t hash[32];          // Asset integrity hash

    std::uint32_t dependency_count;
    std::uint32_t dependency_offset; // Into dependency table

    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;
};
```

## API Reference

### Basic Usage

```cpp
#include <lore/assets/assets.hpp>

// Initialize asset manager
auto asset_manager = std::make_unique<lore::assets::AssetManager>();

// Load a .lore package
auto result = asset_manager->load_package("assets/game_assets.lore");
if (!result) {
    // Handle error
}

// Load an asset
auto texture_handle = asset_manager->load_asset<Texture>("textures/player.png");
if (texture_handle) {
    const Texture* texture = texture_handle->get();
    // Use texture...
}
```

### Async Loading

```cpp
// Start async load
auto future = asset_manager->load_asset_async<AudioClip>("audio/music.ogg");

// Do other work...

// Get result when ready
auto audio_handle = future.get();
if (audio_handle) {
    // Asset is loaded and ready to use
}
```

### ECS Integration

```cpp
// Add asset component to entity
world.add_component<AssetComponent>(entity, {
    .asset_path = "models/character.gltf",
    .auto_load = true,
    .priority = LoadPriority::High
});

// Asset system will automatically load the asset
// Access loaded asset
if (auto* asset_comp = world.get_component<AssetComponent>(entity)) {
    if (asset_comp->is_loaded()) {
        auto* model = asset_comp->get_asset<Model>();
        // Use model...
    }
}
```

## ECS Integration

### AssetComponent

Attach assets to entities for automatic management:

```cpp
struct AssetComponent {
    std::string asset_path;
    LoadPriority priority = LoadPriority::Normal;
    bool auto_load = true;
    bool keep_loaded = false;

    // Runtime state
    LoadState load_state = LoadState::Unloaded;
    std::chrono::steady_clock::time_point load_start_time;
    std::string last_error;
};
```

### AssetSystem

Manages asset lifecycle within the ECS update loop:

```cpp
class AssetSystem : public ecs::System {
public:
    void update(ecs::World& world, float delta_time) override;

    // Configuration
    void set_max_loads_per_frame(std::size_t count);
    void set_background_loading(bool enabled);
    void set_load_timeout(std::chrono::milliseconds timeout);
};
```

## Performance Features

### Memory Management

- **LRU Cache**: Automatically evicts least recently used assets
- **Memory Budgets**: Configurable memory limits with pressure handling
- **Reference Counting**: Assets are automatically unloaded when no longer referenced
- **Garbage Collection**: Periodic cleanup of unreferenced assets

### Compression

Multiple compression algorithms optimized for different use cases:

- **LZ4**: Fast compression/decompression for assets accessed frequently
- **LZ4HC**: Higher compression ratio for assets loaded once
- **ZSTD**: Balanced compression for general use
- **LZMA**: Maximum compression for distribution packages

### Threading

- **Shared Mutexes**: Optimized for read-heavy workloads
- **Async Loading**: Background asset loading with priority queues
- **Thread Pool**: Configurable worker threads for decompression
- **Lock-Free Operations**: Performance counters and statistics

## Development Workflow

### Hot Reload

Assets can be automatically reloaded during development:

```cpp
// Enable hot reload
asset_manager->enable_hot_reload(true);
asset_manager->set_watch_interval(std::chrono::milliseconds(500));

// Assets will automatically reload when files change
```

### Asset Validation

Comprehensive validation ensures asset integrity:

```cpp
// Validate package
auto validation_result = LorePackage::validate("assets/game.lore");
if (!validation_result) {
    std::cout << "Validation failed: " << validation_result.error().message() << std::endl;
}

// Validate individual assets
auto asset_result = asset_manager->validate_asset("textures/broken.png");
```

### Package Creation

Build .lore packages using the fluent builder API:

```cpp
LorePackageBuilder builder("game_assets.lore");

builder
    .set_compression(CompressionType::ZSTD)
    .set_integrity_hash(HashType::SHA256)
    .add_directory("assets/textures/", AssetType::Texture2D)
    .add_directory("assets/audio/", AssetType::AudioClip)
    .add_directory("assets/models/", AssetType::StaticMesh)
    .set_metadata("version", "1.0.0")
    .set_metadata("build_date", std::chrono::system_clock::now());

auto result = builder.build();
```

## Best Practices

### Asset Organization

1. **Group Related Assets**: Keep textures, models, and audio for the same feature together
2. **Use Descriptive Names**: Asset paths should clearly indicate content and usage
3. **Version Assets**: Include version information in package metadata
4. **Optimize for Access Patterns**: Place frequently accessed assets at the beginning of packages

### Memory Management

1. **Set Appropriate Budgets**: Configure memory limits based on target platform
2. **Use Reference Counting**: Let the system automatically manage asset lifetime
3. **Preload Critical Assets**: Load essential assets during initialization
4. **Unload Unused Assets**: Explicitly unload large assets when no longer needed

### Performance Optimization

1. **Choose Appropriate Compression**: Balance file size vs. decompression time
2. **Use Async Loading**: Load assets in background to avoid frame hitches
3. **Batch Operations**: Group asset loads to minimize overhead
4. **Monitor Performance**: Use built-in statistics to identify bottlenecks

### Error Handling

1. **Check All Results**: Always verify asset loading success
2. **Provide Fallbacks**: Have default assets for when loading fails
3. **Log Errors**: Use comprehensive error logging for debugging
4. **Validate Early**: Verify asset integrity during package creation

## Error Codes

The system provides detailed error information:

```cpp
enum class LoreError {
    Success = 0,
    FileNotFound,
    InvalidFormat,
    CorruptedData,
    CompressionError,
    DecompressionError,
    HashMismatch,
    DependencyNotFound,
    CircularDependency,
    OutOfMemory,
    AssetNotFound,
    InvalidAssetType,
    LoadTimeout,
    // ... more error codes
};
```

## Statistics and Profiling

Monitor system performance with built-in statistics:

```cpp
auto stats = asset_manager->get_statistics();
std::cout << "Cache hits: " << stats.cache_hits << std::endl;
std::cout << "Memory usage: " << stats.memory_usage_bytes << " bytes" << std::endl;
std::cout << "Average load time: " << stats.average_load_time.count() << " ms" << std::endl;
```

## Integration with Other Lore Systems

The Lore Package System is designed to work seamlessly with other Lore engine systems:

- **Graphics System**: Automatic texture and shader loading
- **Audio System**: Direct integration with audio file loading
- **Physics System**: Mesh and collision data loading
- **ECS System**: Component-based asset management
- **Rendering System**: Optimized asset streaming for rendering pipelines

## Conclusion

The Lore Package System provides a robust, high-performance foundation for asset management in the Lore game engine. Its combination of safety, performance, and ease of use makes it suitable for projects ranging from small indie games to AAA productions. The native .lore format is specifically designed to work optimally with the Lore engine's architecture and systems.

For more detailed examples and tutorials, see the [examples](../examples/) directory.