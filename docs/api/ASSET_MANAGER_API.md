# Asset Manager API Reference

## Overview

The Asset Manager API provides the primary interface for loading, managing, and accessing game assets in the Lore engine. This document provides comprehensive API reference with examples for working with the native Lore Package System and .lore files.

## Core Classes

### AssetManager

The central asset management system.

#### Constructor

```cpp
AssetManager();
AssetManager(const AssetManagerConfig& config);
```

#### Package Management

```cpp
// Load a .lore package
Result<void> load_package(const std::filesystem::path& package_path);

// Load package with specific name
Result<void> load_package(const std::filesystem::path& package_path,
                         const std::string& package_name);

// Unload a package by name
void unload_package(const std::string& package_name);

// Check if package is loaded
bool is_package_loaded(const std::string& package_name) const;

// Get list of loaded packages
std::vector<std::string> get_loaded_packages() const;
```

#### Asset Loading (Synchronous)

```cpp
// Load asset by path
template<typename T>
Result<AssetHandle<T>> load_asset(const std::string& asset_path);

// Load asset with specific loading options
template<typename T>
Result<AssetHandle<T>> load_asset(const std::string& asset_path,
                                  const LoadOptions& options);

// Preload asset without returning handle
Result<void> preload_asset(const std::string& asset_path);

// Check if asset is loaded
bool is_asset_loaded(const std::string& asset_path) const;
```

#### Asset Loading (Asynchronous)

```cpp
// Load asset asynchronously
template<typename T>
std::future<Result<AssetHandle<T>>> load_asset_async(const std::string& asset_path);

// Load asset with callback
template<typename T>
void load_asset_async(const std::string& asset_path,
                      std::function<void(Result<AssetHandle<T>>)> callback);

// Load multiple assets with progress tracking
std::future<std::vector<Result<AssetHandle<void>>>>
load_assets_async(const std::vector<std::string>& asset_paths,
                  std::function<void(float progress)> progress_callback = nullptr);
```

#### Asset Management

```cpp
// Unload specific asset
void unload_asset(const std::string& asset_path);

// Unload all assets of a specific type
template<typename T>
void unload_assets_of_type();

// Get asset metadata without loading
Result<AssetMetadata> get_asset_metadata(const std::string& asset_path) const;

// Find assets by type
std::vector<std::string> find_assets_by_type(AssetType type) const;

// Find assets by pattern (glob-style)
std::vector<std::string> find_assets(const std::string& pattern) const;
```

#### Memory Management

```cpp
// Set memory budget in bytes
void set_memory_budget(std::size_t bytes);

// Get current memory usage
std::size_t get_memory_usage() const;

// Get memory budget
std::size_t get_memory_budget() const;

// Force garbage collection
void garbage_collect();

// Set cache eviction policy
void set_cache_policy(CachePolicy policy);

// Clear all cached assets
void clear_cache();
```

#### Hot Reload

```cpp
// Enable/disable hot reload
void enable_hot_reload(bool enabled);

// Set file watch interval
void set_watch_interval(std::chrono::milliseconds interval);

// Force reload of specific asset
Result<void> reload_asset(const std::string& asset_path);

// Reload all modified assets
std::size_t reload_modified_assets();
```

#### Statistics and Profiling

```cpp
// Get performance statistics
AssetManagerStatistics get_statistics() const;

// Reset statistics
void reset_statistics();

// Enable/disable detailed profiling
void enable_profiling(bool enabled);

// Get profiling data
std::vector<AssetLoadProfile> get_load_profiles() const;
```

#### Error Handling

```cpp
// Set error handler for async operations
void set_error_handler(std::function<void(const LoreError&)> handler);

// Get last error
std::optional<LoreError> get_last_error() const;

// Clear error state
void clear_errors();
```

### AssetHandle<T>

Smart pointer-style handle for accessing loaded assets.

#### Access Operations

```cpp
// Get raw pointer to asset (may be nullptr)
const T* get() const noexcept;

// Dereference operators
const T& operator*() const noexcept;
const T* operator->() const noexcept;

// Boolean conversion
explicit operator bool() const noexcept;

// Comparison operators
bool operator==(const AssetHandle& other) const noexcept;
bool operator!=(const AssetHandle& other) const noexcept;
```

#### State Queries

```cpp
// Check if handle is valid
bool is_valid() const noexcept;

// Check if asset is loaded
bool is_loaded() const noexcept;

// Get current load state
LoadState get_load_state() const noexcept;

// Get loading progress (0.0 to 1.0)
float get_load_progress() const noexcept;

// Check if loading failed
bool has_load_error() const noexcept;

// Get load error
std::optional<LoreError> get_load_error() const noexcept;
```

#### Metadata Access

```cpp
// Get asset metadata
const AssetMetadata& get_metadata() const noexcept;

// Get asset path
const std::string& get_path() const noexcept;

// Get asset type
AssetType get_type() const noexcept;

// Get asset ID
std::uint32_t get_id() const noexcept;
```

#### Reference Management

```cpp
// Get reference count
std::size_t get_reference_count() const noexcept;

// Create weak reference
AssetWeakHandle<T> weak_ref() const noexcept;

// Reset handle (release reference)
void reset() noexcept;
```

### AssetMetadata

Contains information about an asset.

#### Basic Properties

```cpp
// Asset identification
std::uint32_t get_id() const noexcept;
const std::string& get_name() const noexcept;
const std::string& get_path() const noexcept;
AssetType get_type() const noexcept;

// Size information
std::size_t get_compressed_size() const noexcept;
std::size_t get_uncompressed_size() const noexcept;
float get_compression_ratio() const noexcept;

// Timestamps
std::chrono::system_clock::time_point get_creation_time() const noexcept;
std::chrono::system_clock::time_point get_modification_time() const noexcept;
std::chrono::system_clock::time_point get_load_time() const noexcept;
```

#### Compression and Integrity

```cpp
// Compression information
CompressionType get_compression_type() const noexcept;
HashType get_hash_type() const noexcept;
std::span<const std::uint8_t> get_hash() const noexcept;

// Validation
bool verify_integrity() const noexcept;
Result<void> validate() const noexcept;
```

#### Dependencies

```cpp
// Dependency information
std::size_t get_dependency_count() const noexcept;
std::vector<std::uint32_t> get_dependencies() const noexcept;
std::vector<std::string> get_dependency_paths() const noexcept;

// Check if asset depends on another
bool depends_on(std::uint32_t asset_id) const noexcept;
bool depends_on(const std::string& asset_path) const noexcept;
```

#### Custom Properties

```cpp
// Get custom property
template<typename T>
std::optional<T> get_property(const std::string& key) const;

// Set custom property
template<typename T>
void set_property(const std::string& key, const T& value);

// Check if property exists
bool has_property(const std::string& key) const noexcept;

// Get all property keys
std::vector<std::string> get_property_keys() const;
```

## Configuration

### AssetManagerConfig

Configuration options for the asset manager.

```cpp
struct AssetManagerConfig {
    // Memory management
    std::size_t memory_budget = 512 * 1024 * 1024;  // 512 MB
    CachePolicy cache_policy = CachePolicy::LRU;
    float cache_eviction_threshold = 0.9f;

    // Threading
    std::size_t worker_thread_count = std::thread::hardware_concurrency();
    std::size_t max_concurrent_loads = 8;
    LoadPriority default_priority = LoadPriority::Normal;

    // Hot reload
    bool enable_hot_reload = false;
    std::chrono::milliseconds watch_interval{500};

    // Compression
    CompressionType default_compression = CompressionType::LZ4;
    bool enable_compression_cache = true;

    // Error handling
    bool throw_on_error = false;
    LogLevel log_level = LogLevel::Warning;
};
```

### LoadOptions

Options for asset loading operations.

```cpp
struct LoadOptions {
    LoadPriority priority = LoadPriority::Normal;
    bool keep_in_memory = false;
    bool validate_integrity = true;
    std::chrono::milliseconds timeout{30000};  // 30 seconds

    // Compression options
    bool prefer_compressed = true;
    CompressionType preferred_compression = CompressionType::Auto;

    // Dependency handling
    bool load_dependencies = true;
    bool fail_on_missing_dependencies = true;
};
```

## Enumerations

### AssetType

Supported asset types:

```cpp
enum class AssetType : std::uint32_t {
    Unknown = 0,

    // Textures
    Texture2D,
    Texture3D,
    TextureCube,
    TextureArray,

    // Audio
    AudioClip,
    AudioStream,

    // 3D Assets
    StaticMesh,
    SkinnedMesh,
    Animation,
    Skeleton,

    // Materials and Shaders
    Material,
    Shader,
    ShaderProgram,

    // Fonts and UI
    Font,
    UILayout,

    // Data
    TextFile,
    BinaryData,
    JSONData,
    XMLData,

    // Scenes and Prefabs
    Scene,
    Prefab,

    // Custom
    Custom = 0xFFFF0000
};
```

### LoadState

Asset loading states:

```cpp
enum class LoadState {
    Unloaded,    // Asset not loaded
    Loading,     // Asset is being loaded
    Loaded,      // Asset loaded successfully
    Failed,      // Asset loading failed
    Unloading    // Asset is being unloaded
};
```

### LoadPriority

Loading priority levels:

```cpp
enum class LoadPriority {
    Critical = 0,  // Must be loaded immediately
    High = 1,      // High priority loading
    Normal = 2,    // Normal priority (default)
    Low = 3,       // Low priority, load when convenient
    Background = 4 // Background loading only
};
```

### CompressionType

Supported compression algorithms:

```cpp
enum class CompressionType {
    None,     // No compression
    LZ4,      // Fast compression/decompression
    LZ4HC,    // High compression LZ4
    ZSTD,     // Balanced compression
    LZMA      // Maximum compression
};
```

## Usage Examples

### Basic Asset Loading

```cpp
#include <lore/assets/assets.hpp>

// Create asset manager
auto asset_manager = std::make_unique<AssetManager>();

// Load a package
if (auto result = asset_manager->load_package("game_assets.lore"); !result) {
    std::cerr << "Failed to load package: " << result.error().message() << std::endl;
    return;
}

// Load a texture
auto texture_handle = asset_manager->load_asset<Texture2D>("textures/player.png");
if (texture_handle) {
    const auto* texture = texture_handle->get();
    // Use texture...
} else {
    std::cerr << "Failed to load texture" << std::endl;
}
```

### Async Loading with Callback

```cpp
// Load asset asynchronously with callback
asset_manager->load_asset_async<AudioClip>("audio/music.ogg",
    [](Result<AssetHandle<AudioClip>> result) {
        if (result) {
            // Asset loaded successfully
            auto audio_handle = std::move(*result);
            // Use audio...
        } else {
            // Handle error
            std::cerr << "Audio load failed: " << result.error().message() << std::endl;
        }
    });
```

### Batch Loading with Progress

```cpp
std::vector<std::string> asset_paths = {
    "textures/ui_background.png",
    "textures/ui_button.png",
    "audio/ui_click.wav",
    "fonts/ui_font.ttf"
};

auto future = asset_manager->load_assets_async(asset_paths,
    [](float progress) {
        std::cout << "Loading progress: " << (progress * 100.0f) << "%" << std::endl;
    });

// Wait for completion
auto results = future.get();
for (const auto& result : results) {
    if (!result) {
        std::cerr << "Asset load failed: " << result.error().message() << std::endl;
    }
}
```

### Memory Management

```cpp
// Set memory budget to 256 MB
asset_manager->set_memory_budget(256 * 1024 * 1024);

// Check memory usage
std::cout << "Memory usage: " << asset_manager->get_memory_usage()
          << " / " << asset_manager->get_memory_budget() << " bytes" << std::endl;

// Force garbage collection when needed
if (asset_manager->get_memory_usage() > asset_manager->get_memory_budget() * 0.8f) {
    asset_manager->garbage_collect();
}
```

### Hot Reload Setup

```cpp
// Enable hot reload for development
asset_manager->enable_hot_reload(true);
asset_manager->set_watch_interval(std::chrono::milliseconds(250));

// Check for modified assets periodically
auto modified_count = asset_manager->reload_modified_assets();
if (modified_count > 0) {
    std::cout << "Reloaded " << modified_count << " modified assets" << std::endl;
}
```

### Error Handling

```cpp
// Set global error handler
asset_manager->set_error_handler([](const LoreError& error) {
    std::cerr << "Asset error: " << error.message() << std::endl;
    // Log error, show notification, etc.
});

// Check for specific errors
auto texture_result = asset_manager->load_asset<Texture2D>("missing_texture.png");
if (!texture_result) {
    switch (texture_result.error().code()) {
        case LoreError::FileNotFound:
            // Use fallback texture
            break;
        case LoreError::InvalidFormat:
            // Show format error
            break;
        default:
            // Generic error handling
            break;
    }
}
```

### Statistics and Profiling

```cpp
// Get performance statistics
auto stats = asset_manager->get_statistics();
std::cout << "Cache hit rate: " << (stats.cache_hits / static_cast<double>(stats.cache_requests) * 100.0) << "%" << std::endl;
std::cout << "Average load time: " << stats.average_load_time.count() << " ms" << std::endl;
std::cout << "Total assets loaded: " << stats.total_assets_loaded << std::endl;

// Enable detailed profiling
asset_manager->enable_profiling(true);

// Get load profiles after some time
auto profiles = asset_manager->get_load_profiles();
for (const auto& profile : profiles) {
    std::cout << profile.asset_path << ": " << profile.load_time.count() << " ms" << std::endl;
}
```

## Thread Safety

The Asset Manager API is fully thread-safe and designed specifically for the Lore engine architecture. All operations can be called from any thread without external synchronization. However, note the following:

1. **AssetHandle objects are thread-safe** for reading but not for assignment
2. **Callback functions** may be called from background threads
3. **Error handlers** may be called from any thread
4. **Statistics** are updated atomically and can be read from any thread
5. **Lore package operations** are optimized for concurrent access

## Performance Considerations

1. **Prefer async loading** for large assets to avoid blocking the main thread
2. **Use preloading** for assets that will be needed soon
3. **Set appropriate memory budgets** based on your target platform
4. **Use compression** for assets that aren't accessed frequently
5. **Enable hot reload only in development** due to file system watching overhead
6. **Batch asset operations** when possible to reduce overhead

## Error Recovery

The Lore Package System provides several mechanisms for error recovery:

1. **Fallback assets**: Automatically load default assets when primary assets fail
2. **Retry logic**: Automatic retry for transient errors like network timeouts
3. **Graceful degradation**: Continue operation even when some assets fail to load
4. **Error callbacks**: Custom error handling for application-specific recovery
5. **Package validation**: Comprehensive integrity checking for .lore files
6. **Hot reload**: Automatic recovery from corrupted assets during development