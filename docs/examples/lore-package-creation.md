# Lore Package Creation and Integration Examples

This document provides comprehensive examples for creating, building, and integrating .lore packages in your Lore engine games.

## Table of Contents

1. [Basic Package Creation](#basic-package-creation)
2. [Advanced Package Building](#advanced-package-building)
3. [Asset Organization Strategies](#asset-organization-strategies)
4. [Runtime Integration](#runtime-integration)
5. [Development Workflow](#development-workflow)
6. [Performance Optimization](#performance-optimization)
7. [Troubleshooting](#troubleshooting)

## Basic Package Creation

### Creating Your First .lore Package

```cpp
#include <lore/assets/package_builder.hpp>

int main() {
    // Create a package builder
    lore::assets::LorePackageBuilder builder("my_game_assets.lore");

    // Add individual files
    builder.add_file("assets/textures/player.png", "textures/player.png", lore::AssetType::Texture2D);
    builder.add_file("assets/audio/jump.wav", "audio/jump.wav", lore::AssetType::AudioClip);
    builder.add_file("assets/models/character.gltf", "models/character.gltf", lore::AssetType::StaticMesh);

    // Build the package
    auto result = builder.build();
    if (!result) {
        std::cerr << "Failed to build package: " << result.error().message() << std::endl;
        return 1;
    }

    std::cout << "Package created successfully!" << std::endl;
    return 0;
}
```

### Using the Command Line Tool

```bash
# Create a package from a directory
lore-pack create my_game_assets.lore assets/

# With compression
lore-pack create --compression=zstd my_game_assets.lore assets/

# With metadata
lore-pack create --metadata version=1.0.0 --metadata build=release my_game_assets.lore assets/

# Validate a package
lore-pack validate my_game_assets.lore

# Extract package contents
lore-pack extract my_game_assets.lore extracted_assets/
```

## Advanced Package Building

### Fluent Builder API

```cpp
#include <lore/assets/package_builder.hpp>

void create_game_package() {
    lore::assets::LorePackageBuilder builder("complete_game.lore");

    auto result = builder
        // Set package metadata
        .set_metadata("game_name", "My Awesome Game")
        .set_metadata("version", "1.2.3")
        .set_metadata("build_date", std::chrono::system_clock::now())
        .set_metadata("engine_version", LORE_VERSION_STRING)

        // Configure compression
        .set_default_compression(lore::CompressionType::ZSTD)
        .set_compression_level(6)
        .set_integrity_hash(lore::HashType::SHA256)

        // Add entire directories with type mapping
        .add_directory("assets/textures/", lore::AssetType::Texture2D)
        .add_directory("assets/models/", lore::AssetType::StaticMesh)
        .add_directory("assets/audio/", lore::AssetType::AudioClip)
        .add_directory("assets/shaders/", lore::AssetType::Shader)
        .add_directory("assets/fonts/", lore::AssetType::Font)

        // Add files with custom settings
        .add_file("assets/config/settings.json", "config/settings.json",
                  lore::AssetType::JSONData, {
                      .compression = lore::CompressionType::LZ4,
                      .priority = lore::LoadPriority::Critical
                  })

        // Add with dependencies
        .add_file("assets/materials/metal.mat", "materials/metal.mat",
                  lore::AssetType::Material, {
                      .dependencies = {"textures/metal_albedo.png", "textures/metal_normal.png"}
                  })

        // Custom asset types
        .add_file("assets/scripts/player_controller.lua", "scripts/player_controller.lua",
                  lore::AssetType::Custom)

        // Build with validation
        .enable_validation(true)
        .build();

    if (!result) {
        std::cerr << "Package creation failed: " << result.error().message() << std::endl;
        return;
    }

    std::cout << "Complete game package created successfully!" << std::endl;
}
```

### Custom Asset Types and Processors

```cpp
// Define custom asset type
enum class CustomAssetType : std::uint32_t {
    LuaScript = static_cast<std::uint32_t>(lore::AssetType::Custom) + 1,
    AIBehaviorTree = static_cast<std::uint32_t>(lore::AssetType::Custom) + 2,
    GameConfig = static_cast<std::uint32_t>(lore::AssetType::Custom) + 3
};

// Custom asset processor
class LuaScriptProcessor : public lore::assets::AssetProcessor {
public:
    lore::Result<std::vector<std::uint8_t>> process(
        const std::filesystem::path& input_path,
        const lore::assets::ProcessingOptions& options) override {

        // Read Lua script
        std::ifstream file(input_path, std::ios::binary);
        if (!file) {
            return lore::Error{"Failed to open Lua script file"};
        }

        // Compile to bytecode for faster loading
        std::vector<std::uint8_t> source_code{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        auto bytecode = compile_lua_to_bytecode(source_code);
        return bytecode;
    }

    lore::AssetType get_asset_type() const override {
        return static_cast<lore::AssetType>(CustomAssetType::LuaScript);
    }
};

// Register custom processor
void setup_custom_processors() {
    lore::assets::LorePackageBuilder::register_processor(
        std::make_unique<LuaScriptProcessor>()
    );
}
```

## Asset Organization Strategies

### Hierarchical Organization

```cpp
void create_organized_package() {
    lore::assets::LorePackageBuilder builder("organized_assets.lore");

    builder
        // Core game assets (always loaded)
        .add_directory("assets/core/ui/", lore::AssetType::Texture2D, {
            .group = "core",
            .priority = lore::LoadPriority::Critical,
            .keep_in_memory = true
        })
        .add_directory("assets/core/fonts/", lore::AssetType::Font, {
            .group = "core",
            .priority = lore::LoadPriority::Critical
        })

        // Level-specific assets (load on demand)
        .add_directory("assets/levels/level1/", lore::AssetType::Texture2D, {
            .group = "level1",
            .priority = lore::LoadPriority::Normal
        })
        .add_directory("assets/levels/level2/", lore::AssetType::Texture2D, {
            .group = "level2",
            .priority = lore::LoadPriority::Low
        })

        // Character assets (moderate priority)
        .add_directory("assets/characters/", lore::AssetType::StaticMesh, {
            .group = "characters",
            .priority = lore::LoadPriority::High,
            .compression = lore::CompressionType::LZ4HC
        })

        // Audio assets (background loading)
        .add_directory("assets/audio/music/", lore::AssetType::AudioClip, {
            .group = "music",
            .priority = lore::LoadPriority::Background,
            .compression = lore::CompressionType::LZMA
        })
        .add_directory("assets/audio/sfx/", lore::AssetType::AudioClip, {
            .group = "sfx",
            .priority = lore::LoadPriority::Normal,
            .compression = lore::CompressionType::ZSTD
        })

        .build();
}
```

### Multi-Package Strategy

```cpp
// Create separate packages for different content types
void create_modular_packages() {
    // Core engine assets - always loaded
    {
        lore::assets::LorePackageBuilder core_builder("core_assets.lore");
        core_builder
            .add_directory("assets/core/", lore::AssetType::Texture2D)
            .set_default_compression(lore::CompressionType::LZ4)  // Fast loading
            .set_metadata("package_type", "core")
            .build();
    }

    // Game content - loaded based on level
    {
        lore::assets::LorePackageBuilder content_builder("game_content.lore");
        content_builder
            .add_directory("assets/levels/", lore::AssetType::StaticMesh)
            .add_directory("assets/characters/", lore::AssetType::SkinnedMesh)
            .set_default_compression(lore::CompressionType::ZSTD)  // Balanced
            .set_metadata("package_type", "content")
            .build();
    }

    // Audio content - streamed
    {
        lore::assets::LorePackageBuilder audio_builder("audio_assets.lore");
        audio_builder
            .add_directory("assets/audio/", lore::AssetType::AudioClip)
            .set_default_compression(lore::CompressionType::LZMA)  // Maximum compression
            .set_metadata("package_type", "audio")
            .build();
    }

    // DLC content - optional
    {
        lore::assets::LorePackageBuilder dlc_builder("dlc_pack1.lore");
        dlc_builder
            .add_directory("dlc/pack1/", lore::AssetType::StaticMesh)
            .set_metadata("package_type", "dlc")
            .set_metadata("dlc_id", "pack1")
            .set_metadata("requires_core", "1.0.0")
            .build();
    }
}
```

## Runtime Integration

### Basic Asset Loading

```cpp
#include <lore/assets/asset_manager.hpp>

class GameApplication {
private:
    std::unique_ptr<lore::assets::AssetManager> asset_manager_;

public:
    bool initialize() {
        // Create asset manager
        asset_manager_ = std::make_unique<lore::assets::AssetManager>();

        // Load core package
        auto core_result = asset_manager_->load_package("core_assets.lore");
        if (!core_result) {
            std::cerr << "Failed to load core assets: "
                      << core_result.error().message() << std::endl;
            return false;
        }

        // Load game content package
        auto content_result = asset_manager_->load_package("game_content.lore");
        if (!content_result) {
            std::cerr << "Failed to load game content: "
                      << content_result.error().message() << std::endl;
            return false;
        }

        return true;
    }

    void load_level(const std::string& level_name) {
        // Load level-specific assets
        auto texture = asset_manager_->load_asset<lore::Texture2D>(
            "levels/" + level_name + "/background.png"
        );

        auto model = asset_manager_->load_asset<lore::StaticMesh>(
            "levels/" + level_name + "/terrain.gltf"
        );

        // Use assets...
        if (texture && model) {
            // Assets loaded successfully
            setup_level_rendering(*texture, *model);
        }
    }
};
```

### Async Loading with Progress

```cpp
class AssetLoader {
public:
    struct LoadProgress {
        std::size_t total_assets = 0;
        std::size_t loaded_assets = 0;
        std::string current_asset;
        float percentage = 0.0f;
    };

    using ProgressCallback = std::function<void(const LoadProgress&)>;

public:
    std::future<bool> load_level_async(
        const std::string& level_name,
        ProgressCallback progress_callback = nullptr) {

        return std::async(std::launch::async, [=]() {
            LoadProgress progress;
            std::vector<std::string> level_assets = {
                "levels/" + level_name + "/background.png",
                "levels/" + level_name + "/terrain.gltf",
                "levels/" + level_name + "/music.ogg",
                "levels/" + level_name + "/lighting.json"
            };

            progress.total_assets = level_assets.size();

            for (const auto& asset_path : level_assets) {
                progress.current_asset = asset_path;

                if (progress_callback) {
                    progress_callback(progress);
                }

                // Load asset
                auto result = asset_manager_->load_asset<void>(asset_path);
                if (!result) {
                    std::cerr << "Failed to load " << asset_path << std::endl;
                    return false;
                }

                ++progress.loaded_assets;
                progress.percentage = static_cast<float>(progress.loaded_assets) /
                                    static_cast<float>(progress.total_assets);
            }

            return true;
        });
    }
};

// Usage example
void show_loading_screen() {
    AssetLoader loader;

    auto future = loader.load_level_async("forest_level",
        [](const AssetLoader::LoadProgress& progress) {
            std::cout << "Loading: " << progress.current_asset
                      << " (" << (progress.percentage * 100.0f) << "%)" << std::endl;

            // Update loading screen UI
            update_progress_bar(progress.percentage);
        });

    // Wait for completion
    bool success = future.get();
    if (success) {
        std::cout << "Level loaded successfully!" << std::endl;
    }
}
```

### Memory Management

```cpp
class OptimizedAssetManager {
public:
    void setup_memory_management() {
        // Set memory budget based on platform
        std::size_t memory_budget = get_available_memory() * 0.6; // 60% of available RAM
        asset_manager_->set_memory_budget(memory_budget);

        // Configure cache policies
        asset_manager_->set_cache_policy(lore::CachePolicy::LRU);
        asset_manager_->set_cache_eviction_threshold(0.8f); // Evict at 80% full

        // Enable background garbage collection
        asset_manager_->enable_background_gc(true);
        asset_manager_->set_gc_interval(std::chrono::seconds(30));
    }

    void optimize_for_level_transition() {
        // Preload next level assets
        std::vector<std::string> next_level_assets = get_next_level_assets();

        auto preload_future = asset_manager_->preload_assets_async(
            next_level_assets,
            lore::LoadPriority::Background
        );

        // Unload previous level assets that aren't needed
        std::vector<std::string> old_assets = get_previous_level_assets();
        for (const auto& asset_path : old_assets) {
            if (!is_core_asset(asset_path)) {
                asset_manager_->unload_asset(asset_path);
            }
        }

        // Force garbage collection to free memory
        asset_manager_->garbage_collect();
    }

    void handle_memory_pressure() {
        auto stats = asset_manager_->get_statistics();

        if (stats.memory_usage_ratio > 0.9f) { // Over 90% memory usage
            // Emergency memory cleanup

            // 1. Unload non-critical assets
            asset_manager_->unload_assets_by_priority(lore::LoadPriority::Low);
            asset_manager_->unload_assets_by_priority(lore::LoadPriority::Background);

            // 2. Force aggressive garbage collection
            asset_manager_->garbage_collect();

            // 3. Temporarily reduce memory budget
            auto current_budget = asset_manager_->get_memory_budget();
            asset_manager_->set_memory_budget(current_budget * 0.8f);

            std::cout << "Memory pressure handled, usage reduced to "
                      << asset_manager_->get_memory_usage_ratio() * 100.0f << "%" << std::endl;
        }
    }
};
```

## Development Workflow

### Hot Reload Development Setup

```cpp
class DevelopmentAssetManager {
public:
    void setup_hot_reload() {
        // Enable hot reload for development builds only
        #ifdef LORE_DEBUG
        asset_manager_->enable_hot_reload(true);
        asset_manager_->set_watch_interval(std::chrono::milliseconds(250));

        // Set up hot reload callbacks
        asset_manager_->set_hot_reload_callback([this](const std::string& asset_path) {
            std::cout << "Hot reloading: " << asset_path << std::endl;
            on_asset_reloaded(asset_path);
        });

        // Watch specific directories
        asset_manager_->add_watch_directory("assets/textures/");
        asset_manager_->add_watch_directory("assets/shaders/");
        asset_manager_->add_watch_directory("assets/scripts/");
        #endif
    }

private:
    void on_asset_reloaded(const std::string& asset_path) {
        // Update any systems that depend on this asset
        if (asset_path.find("shaders/") != std::string::npos) {
            // Reload shader programs
            graphics_system_->reload_shaders();
        } else if (asset_path.find("textures/") != std::string::npos) {
            // Update texture references
            rendering_system_->update_texture(asset_path);
        } else if (asset_path.find("scripts/") != std::string::npos) {
            // Reload script
            script_system_->reload_script(asset_path);
        }
    }
};
```

### Build Pipeline Integration

```cmake
# CMake function to create .lore packages during build
function(create_lore_package)
    set(options "")
    set(oneValueArgs PACKAGE_NAME OUTPUT_DIR COMPRESSION)
    set(multiValueArgs SOURCE_DIRS DEPENDENCIES)

    cmake_parse_arguments(LORE_PKG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Create custom target for package creation
    add_custom_target(${LORE_PKG_PACKAGE_NAME}_package
        COMMAND lore-pack create
            --compression=${LORE_PKG_COMPRESSION}
            --output=${LORE_PKG_OUTPUT_DIR}/${LORE_PKG_PACKAGE_NAME}.lore
            ${LORE_PKG_SOURCE_DIRS}
        DEPENDS ${LORE_PKG_DEPENDENCIES}
        COMMENT "Creating ${LORE_PKG_PACKAGE_NAME}.lore package"
    )

    # Add to main target dependencies
    add_dependencies(${PROJECT_NAME} ${LORE_PKG_PACKAGE_NAME}_package)
endfunction()

# Usage in CMakeLists.txt
create_lore_package(
    PACKAGE_NAME game_assets
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/assets
    COMPRESSION zstd
    SOURCE_DIRS
        ${CMAKE_SOURCE_DIR}/assets/textures
        ${CMAKE_SOURCE_DIR}/assets/models
        ${CMAKE_SOURCE_DIR}/assets/audio
    DEPENDENCIES
        texture_processing
        model_optimization
)
```

### Package Validation Tools

```cpp
class PackageValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::size_t total_assets = 0;
        std::size_t corrupted_assets = 0;
        std::chrono::milliseconds validation_time{0};
    };

public:
    ValidationResult validate_package(const std::filesystem::path& package_path) {
        auto start_time = std::chrono::steady_clock::now();
        ValidationResult result;

        // Open package
        auto package = lore::assets::LorePackage::open(package_path);
        if (!package) {
            result.errors.push_back("Failed to open package: " + package.error().message());
            return result;
        }

        // Validate header
        if (!validate_header((*package)->get_header(), result)) {
            return result;
        }

        // Validate all assets
        auto asset_ids = (*package)->get_asset_ids();
        result.total_assets = asset_ids.size();

        for (auto asset_id : asset_ids) {
            if (!validate_asset(**package, asset_id, result)) {
                ++result.corrupted_assets;
            }
        }

        // Calculate validation time
        auto end_time = std::chrono::steady_clock::now();
        result.validation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );

        result.is_valid = result.errors.empty() && result.corrupted_assets == 0;
        return result;
    }

private:
    bool validate_header(const lore::assets::PackageHeader& header, ValidationResult& result) {
        // Check magic number
        if (header.magic != 0x45524F4C) { // "LORE"
            result.errors.push_back("Invalid magic number in package header");
            return false;
        }

        // Check version compatibility
        if (header.major_version > LORE_PACKAGE_MAJOR_VERSION) {
            result.errors.push_back("Package version too new for this engine version");
            return false;
        }

        // Warn about old versions
        if (header.major_version < LORE_PACKAGE_MAJOR_VERSION) {
            result.warnings.push_back("Package uses older format version");
        }

        return true;
    }

    bool validate_asset(const lore::assets::LorePackage& package,
                       std::uint32_t asset_id,
                       ValidationResult& result) {

        auto entry_result = package.get_asset_entry(asset_id);
        if (!entry_result) {
            result.errors.push_back("Failed to get asset entry for ID: " + std::to_string(asset_id));
            return false;
        }

        auto& entry = *entry_result;

        // Validate asset data integrity
        auto stream_result = package.get_asset_stream(asset_id);
        if (!stream_result) {
            result.errors.push_back("Failed to get asset stream for: " +
                                  std::string(entry.name_offset, entry.name_offset + 64));
            return false;
        }

        // Verify hash
        if (!verify_asset_hash(*stream_result, entry)) {
            result.errors.push_back("Hash mismatch for asset: " +
                                  std::string(entry.name_offset, entry.name_offset + 64));
            return false;
        }

        return true;
    }

    bool verify_asset_hash(const lore::assets::AssetStream& stream,
                          const lore::assets::AssetEntry& entry) {
        // Implement hash verification based on entry.hash_type
        // This is a simplified example
        return true; // Placeholder
    }
};
```

## Performance Optimization

### Compression Strategy

```cpp
void optimize_package_compression() {
    lore::assets::LorePackageBuilder builder("optimized_game.lore");

    builder
        // UI textures: Fast loading, accessed frequently
        .add_directory("assets/ui/", lore::AssetType::Texture2D, {
            .compression = lore::CompressionType::LZ4,
            .priority = lore::LoadPriority::High
        })

        // Environment textures: Balanced compression
        .add_directory("assets/environments/", lore::AssetType::Texture2D, {
            .compression = lore::CompressionType::ZSTD,
            .compression_level = 6
        })

        // Audio files: Maximum compression for distribution
        .add_directory("assets/audio/music/", lore::AssetType::AudioClip, {
            .compression = lore::CompressionType::LZMA,
            .compression_level = 9
        })

        // Frequently accessed audio: Fast decompression
        .add_directory("assets/audio/sfx/", lore::AssetType::AudioClip, {
            .compression = lore::CompressionType::LZ4HC,
            .compression_level = 9
        })

        // Critical game data: No compression for instant access
        .add_file("assets/config/critical_settings.json", "config/critical.json",
                  lore::AssetType::JSONData, {
                      .compression = lore::CompressionType::None,
                      .priority = lore::LoadPriority::Critical
                  })

        .build();
}
```

### Asset Streaming

```cpp
class StreamingAssetManager {
public:
    void setup_streaming() {
        // Configure streaming buffer sizes
        asset_manager_->set_streaming_buffer_size(64 * 1024 * 1024); // 64 MB buffer
        asset_manager_->set_max_concurrent_streams(4);

        // Set up distance-based loading
        asset_manager_->enable_distance_based_loading(true);
        asset_manager_->set_load_distance(100.0f);   // Load within 100 units
        asset_manager_->set_unload_distance(150.0f); // Unload beyond 150 units
    }

    void update_streaming(const lore::Vector3& player_position) {
        // Update streaming based on player position
        asset_manager_->update_streaming_position(player_position);

        // Check for assets that should be loaded/unloaded
        auto nearby_assets = world_->get_assets_near_position(player_position, 100.0f);

        for (const auto& asset_path : nearby_assets) {
            if (!asset_manager_->is_asset_loaded(asset_path)) {
                // Start async loading
                asset_manager_->load_asset_async<void>(asset_path, lore::LoadPriority::Normal);
            }
        }

        // Unload distant assets
        auto distant_assets = asset_manager_->get_loaded_assets_beyond_distance(
            player_position, 150.0f
        );

        for (const auto& asset_path : distant_assets) {
            if (!is_critical_asset(asset_path)) {
                asset_manager_->unload_asset(asset_path);
            }
        }
    }
};
```

## Troubleshooting

### Common Issues and Solutions

#### Package Creation Fails

```cpp
// Issue: Package creation fails with "File not found"
// Solution: Verify all file paths exist before building

bool verify_assets_exist(const std::vector<std::string>& asset_paths) {
    for (const auto& path : asset_paths) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Asset not found: " << path << std::endl;
            return false;
        }
    }
    return true;
}

void create_package_with_verification() {
    std::vector<std::string> assets = {
        "assets/textures/player.png",
        "assets/models/character.gltf",
        "assets/audio/jump.wav"
    };

    if (!verify_assets_exist(assets)) {
        std::cerr << "Cannot create package: missing assets" << std::endl;
        return;
    }

    // Proceed with package creation...
}
```

#### Loading Performance Issues

```cpp
// Issue: Slow asset loading
// Solution: Profile and optimize loading pipeline

class LoadingProfiler {
public:
    void profile_loading_performance() {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Load test assets
        std::vector<std::string> test_assets = {
            "textures/test1.png", "textures/test2.png", "textures/test3.png"
        };

        for (const auto& asset_path : test_assets) {
            auto asset_start = std::chrono::high_resolution_clock::now();

            auto result = asset_manager_->load_asset<lore::Texture2D>(asset_path);

            auto asset_end = std::chrono::high_resolution_clock::now();
            auto asset_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                asset_end - asset_start
            );

            std::cout << asset_path << ": " << asset_time.count() << "ms" << std::endl;
        }

        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time
        );

        std::cout << "Total loading time: " << total_time.count() << "ms" << std::endl;

        // Get detailed statistics
        auto stats = asset_manager_->get_statistics();
        std::cout << "Cache hit rate: " << (stats.cache_hit_rate * 100.0f) << "%" << std::endl;
        std::cout << "Average decompression time: " << stats.avg_decompression_time.count() << "ms" << std::endl;
    }
};
```

#### Memory Usage Issues

```cpp
// Issue: High memory usage
// Solution: Implement memory monitoring and cleanup

class MemoryMonitor {
public:
    void monitor_memory_usage() {
        auto stats = asset_manager_->get_statistics();

        std::cout << "Memory Usage Report:" << std::endl;
        std::cout << "  Total allocated: " << (stats.total_memory_bytes / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Assets in memory: " << stats.loaded_asset_count << std::endl;
        std::cout << "  Largest asset: " << (stats.largest_asset_bytes / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Cache efficiency: " << (stats.cache_hit_rate * 100.0f) << "%" << std::endl;

        // Check for memory leaks
        if (stats.total_memory_bytes > expected_memory_usage_ * 1.5f) {
            std::cout << "WARNING: Memory usage is 50% higher than expected!" << std::endl;
            diagnose_memory_issues();
        }
    }

private:
    void diagnose_memory_issues() {
        // Get list of all loaded assets
        auto loaded_assets = asset_manager_->get_loaded_asset_list();

        // Sort by memory usage
        std::sort(loaded_assets.begin(), loaded_assets.end(),
            [](const auto& a, const auto& b) {
                return a.memory_usage > b.memory_usage;
            });

        std::cout << "Top memory-consuming assets:" << std::endl;
        for (int i = 0; i < std::min(10, static_cast<int>(loaded_assets.size())); ++i) {
            const auto& asset = loaded_assets[i];
            std::cout << "  " << asset.path << ": "
                      << (asset.memory_usage / 1024 / 1024) << " MB" << std::endl;
        }
    }

    std::size_t expected_memory_usage_ = 512 * 1024 * 1024; // 512 MB
};
```

This comprehensive example documentation provides developers with everything they need to create, optimize, and integrate .lore packages into their Lore engine games. The examples progress from basic usage to advanced optimization techniques, covering real-world scenarios and common issues.