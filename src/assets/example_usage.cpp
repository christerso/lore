// Example usage of the Lore package asset system
// This file demonstrates the comprehensive asset management capabilities

#include <lore/assets/assets.hpp>
#include <lore/ecs/ecs.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace lore::assets::examples {

// Example custom asset loader for text files
class TextAssetLoader : public AssetLoader {
public:
    AssetResult<std::shared_ptr<void>> load(std::span<const std::uint8_t> data,
                                           const AssetMetadata& metadata) override {
        auto text_data = std::make_shared<std::string>(
            reinterpret_cast<const char*>(data.data()),
            data.size()
        );
        return std::static_pointer_cast<void>(text_data);
    }

    AssetResult<void> unload(std::shared_ptr<void> asset_data) override {
        // Text data will be automatically cleaned up when shared_ptr is destroyed
        return {};
    }

    AssetResult<AssetMetadata> extract_metadata(std::span<const std::uint8_t> data,
                                               const std::string& path) const noexcept override {
        AssetMetadata metadata;
        metadata.id = utils::generate_asset_id(path);
        metadata.type = AssetType::Text;
        metadata.name = utils::get_filename(path);
        metadata.path = path;
        metadata.size_bytes = data.size();
        metadata.compressed_size_bytes = data.size();
        metadata.version = 1;
        metadata.compression = CompressionType::None;
        metadata.checksum = utils::calculate_crc32(data);
        metadata.hash_sha256 = utils::calculate_sha256(data);

        return metadata;
    }

    AssetType get_asset_type() const noexcept override { return AssetType::Text; }
    std::vector<std::string> get_supported_extensions() const noexcept override { return {"txt", "log"}; }
    std::string get_loader_name() const noexcept override { return "TextAssetLoader"; }
    std::string get_loader_version() const noexcept override { return "1.0.0"; }
};

// Example usage of the asset system
void demonstrate_basic_usage() {
    std::cout << "=== Basic Asset System Usage ===" << std::endl;

    // Initialize the asset manager
    AssetManager asset_manager;

    AssetStreamConfig stream_config;
    stream_config.max_concurrent_streams = 4;
    stream_config.buffer_size = 1024 * 1024; // 1MB
    stream_config.enable_compression = true;
    stream_config.preferred_compression = CompressionType::LZ4;

    HotReloadConfig hot_reload_config;
    hot_reload_config.enabled = true;
    hot_reload_config.check_interval = std::chrono::milliseconds(500);
    hot_reload_config.recursive_watch = true;
    hot_reload_config.watch_directories = {"./assets", "./data"};
    hot_reload_config.watch_extensions = {"txt", "json", "xml", "png", "jpg", "wav", "ogg"};

    asset_manager.initialize(stream_config, hot_reload_config);

    // Register custom asset loader
    auto text_loader = std::make_unique<TextAssetLoader>();
    asset_manager.register_asset_loader<std::string>(AssetType::Text, std::move(text_loader));

    // Create a sample Lore package
    auto package_result = asset_manager.create_package("./sample_assets.lore");
    if (package_result) {
        std::cout << "Created Lore package with ID: " << *package_result << std::endl;
    }

    // Load an existing package
    auto load_result = asset_manager.load_package("./game_assets.lore");
    if (load_result) {
        std::cout << "Loaded Lore package with ID: " << *load_result << std::endl;

        // Get all assets in the package
        auto asset_ids = asset_manager.get_assets_in_package(*load_result);
        std::cout << "Package contains " << asset_ids.size() << " assets" << std::endl;

        for (auto asset_id : asset_ids) {
            auto metadata_result = asset_manager.get_asset_metadata(asset_id);
            if (metadata_result) {
                const auto& metadata = *metadata_result;
                std::cout << "  Asset: " << metadata.name
                         << " (Type: " << utils::asset_type_to_string(metadata.type)
                         << ", Size: " << metadata.size_bytes << " bytes)" << std::endl;
            }
        }
    }

    asset_manager.shutdown();
}

// Example ECS integration
void demonstrate_ecs_integration() {
    std::cout << "\n=== ECS Integration Example ===" << std::endl;

    // Create ECS world
    ecs::World world;

    // Add asset system to the world
    auto& asset_system = world.add_system<AssetSystem>();

    // Create entities with asset components
    auto entity1 = world.create_entity();
    auto entity2 = world.create_entity();
    auto entity3 = world.create_entity();

    // Add asset components
    AssetComponent texture_component;
    texture_component.asset_id = utils::generate_asset_id("textures/player.png");
    texture_component.priority = AssetPriority::High;
    texture_component.auto_load = true;
    texture_component.keep_loaded = true;
    world.add_component(entity1, texture_component);

    AssetComponent audio_component;
    audio_component.asset_id = utils::generate_asset_id("audio/footsteps.wav");
    audio_component.priority = AssetPriority::Medium;
    audio_component.auto_load = true;
    audio_component.keep_loaded = false;
    world.add_component(entity2, audio_component);

    AssetComponent mesh_component;
    mesh_component.asset_id = utils::generate_asset_id("meshes/character.fbx");
    mesh_component.priority = AssetPriority::Critical;
    mesh_component.auto_load = true;
    mesh_component.keep_loaded = true;
    world.add_component(entity3, mesh_component);

    // Add asset references
    AssetReferenceComponent material_ref;
    material_ref.asset_path = "materials/metal.mat";
    material_ref.expected_type = AssetType::Material;
    world.add_component(entity1, material_ref);

    // Initialize and update the world
    world.update(0.016f); // 60 FPS

    // Preload assets for specific entities
    asset_system.preload_entity_assets(world, entity1);
    asset_system.preload_entity_assets(world, entity3);

    // Resolve asset references
    asset_system.resolve_asset_references(world);

    // Simulate game loop
    for (int frame = 0; frame < 10; ++frame) {
        world.update(0.016f);

        // Check asset loading status
        auto& texture_comp = world.get_component<AssetComponent>(entity1);
        if (texture_comp.is_loaded()) {
            std::cout << "Frame " << frame << ": Texture asset loaded successfully" << std::endl;
        }
    }

    // Print asset system statistics
    auto stats = asset_system.get_asset_manager().get_statistics();
    std::cout << "Asset System Statistics:" << std::endl;
    std::cout << "  Total assets: " << stats.total_assets << std::endl;
    std::cout << "  Loaded assets: " << stats.loaded_assets << std::endl;
    std::cout << "  Memory usage: " << stats.memory_usage_bytes / 1024 / 1024 << " MB" << std::endl;
    std::cout << "  Cache hits: " << stats.cache_hits << std::endl;
    std::cout << "  Cache misses: " << stats.cache_misses << std::endl;
    std::cout << "  Average load time: " << stats.average_load_time.count() << " ms" << std::endl;
}

// Example async asset loading
void demonstrate_async_loading() {
    std::cout << "\n=== Async Asset Loading Example ===" << std::endl;

    AssetManager asset_manager;
    asset_manager.initialize();

    // Load multiple assets asynchronously
    std::vector<std::future<AssetResult<AssetHandle>>> futures;
    std::vector<AssetID> asset_ids = {
        utils::generate_asset_id("textures/background.png"),
        utils::generate_asset_id("audio/music.ogg"),
        utils::generate_asset_id("meshes/environment.obj"),
        utils::generate_asset_id("scripts/gameplay.lua")
    };

    for (auto asset_id : asset_ids) {
        AssetLoadContext context;
        context.id = asset_id;
        context.priority = AssetPriority::Medium;
        context.load_dependencies = true;
        context.cache_after_load = true;
        context.timeout = std::chrono::seconds(30);

        context.on_loaded = [asset_id](AssetHandle handle) {
            std::cout << "Asset " << asset_id << " loaded successfully" << std::endl;
        };

        context.on_failed = [asset_id](AssetError error) {
            std::cout << "Asset " << asset_id << " failed to load: "
                     << utils::asset_error_to_string(error) << std::endl;
        };

        futures.push_back(asset_manager.load_asset_async(asset_id, context));
    }

    // Wait for all assets to load
    for (auto& future : futures) {
        auto result = future.get();
        if (result) {
            auto handle = *result;
            std::cout << "Retrieved asset handle for ID: " << handle.get_id()
                     << " (Type: " << utils::asset_type_to_string(handle.get_type()) << ")" << std::endl;
        }
    }

    asset_manager.shutdown();
}

// Example hot-reload functionality
void demonstrate_hot_reload() {
    std::cout << "\n=== Hot-Reload Example ===" << std::endl;

    AssetManager asset_manager;

    HotReloadConfig hot_reload_config;
    hot_reload_config.enabled = true;
    hot_reload_config.check_interval = std::chrono::milliseconds(100);
    hot_reload_config.watch_directories = {"./test_assets"};
    hot_reload_config.watch_extensions = {"txt", "json"};
    hot_reload_config.recursive_watch = true;

    // Set up callbacks
    hot_reload_config.on_asset_changed = [&asset_manager](AssetID asset_id, const std::string& path) {
        std::cout << "Asset changed: " << path << " (ID: " << asset_id << ")" << std::endl;

        // Reload the asset
        auto reload_result = asset_manager.reload_asset(asset_id);
        if (reload_result) {
            std::cout << "Asset reloaded successfully" << std::endl;
        } else {
            std::cout << "Failed to reload asset: "
                     << utils::asset_error_to_string(reload_result.error()) << std::endl;
        }
    };

    hot_reload_config.on_file_added = [](const std::string& path) {
        std::cout << "New file detected: " << path << std::endl;
    };

    hot_reload_config.on_file_removed = [](const std::string& path) {
        std::cout << "File removed: " << path << std::endl;
    };

    AssetStreamConfig stream_config;
    asset_manager.initialize(stream_config, hot_reload_config);

    // Create test directory and file
    std::filesystem::create_directories("./test_assets");

    // Create a test file
    {
        std::ofstream test_file("./test_assets/test.txt");
        test_file << "Initial content";
    }

    std::cout << "Watching for file changes... (modify ./test_assets/test.txt)" << std::endl;
    std::cout << "Monitoring for 5 seconds..." << std::endl;

    // Monitor for changes
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    asset_manager.shutdown();

    // Clean up
    std::filesystem::remove_all("./test_assets");
}

// Example compression and validation
void demonstrate_compression_and_validation() {
    std::cout << "\n=== Compression and Validation Example ===" << std::endl;

    // Test compression algorithms
    std::string test_data = "This is a test string for compression. ";
    for (int i = 0; i < 100; ++i) {
        test_data += "Repeated data to increase compression effectiveness. ";
    }

    std::span<const std::uint8_t> data_span(
        reinterpret_cast<const std::uint8_t*>(test_data.data()),
        test_data.size()
    );

    std::cout << "Original data size: " << data_span.size() << " bytes" << std::endl;

    // Test LZ4 compression
    auto lz4_result = utils::compress_data(data_span, CompressionType::LZ4);
    if (lz4_result) {
        std::cout << "LZ4 compressed size: " << lz4_result->size() << " bytes ("
                 << utils::get_compression_ratio(data_span.size(), lz4_result->size()) << "x)" << std::endl;

        // Test decompression
        auto decomp_result = utils::decompress_data(*lz4_result, CompressionType::LZ4, data_span.size());
        if (decomp_result) {
            std::cout << "LZ4 decompression successful, size: " << decomp_result->size() << " bytes" << std::endl;
        }
    }

    // Test ZSTD compression
    auto zstd_result = utils::compress_data(data_span, CompressionType::ZSTD);
    if (zstd_result) {
        std::cout << "ZSTD compressed size: " << zstd_result->size() << " bytes ("
                 << utils::get_compression_ratio(data_span.size(), zstd_result->size()) << "x)" << std::endl;

        // Test decompression
        auto decomp_result = utils::decompress_data(*zstd_result, CompressionType::ZSTD, data_span.size());
        if (decomp_result) {
            std::cout << "ZSTD decompression successful, size: " << decomp_result->size() << " bytes" << std::endl;
        }
    }

    // Test validation functions
    auto crc32 = utils::calculate_crc32(data_span);
    auto sha256 = utils::calculate_sha256(data_span);

    std::cout << "CRC32 checksum: 0x" << std::hex << crc32 << std::dec << std::endl;
    std::cout << "SHA256 hash: " << sha256.substr(0, 16) << "..." << std::endl;

    // Validate checksums
    bool crc_valid = utils::validate_checksum(data_span, crc32);
    bool hash_valid = utils::validate_hash(data_span, sha256);

    std::cout << "CRC32 validation: " << (crc_valid ? "PASS" : "FAIL") << std::endl;
    std::cout << "SHA256 validation: " << (hash_valid ? "PASS" : "FAIL") << std::endl;
}

// Example memory management and profiling
void demonstrate_memory_management() {
    std::cout << "\n=== Memory Management Example ===" << std::endl;

    AssetManager asset_manager;

    AssetStreamConfig stream_config;
    stream_config.buffer_size = 512 * 1024; // 512KB buffer
    asset_manager.initialize(stream_config);

    // Set memory budget
    asset_manager.set_memory_budget(100 * 1024 * 1024); // 100MB
    asset_manager.set_cache_size_limit(50 * 1024 * 1024); // 50MB cache

    std::cout << "Memory budget: " << asset_manager.get_memory_budget() / 1024 / 1024 << " MB" << std::endl;
    std::cout << "Cache limit: " << asset_manager.get_cache_size_limit() / 1024 / 1024 << " MB" << std::endl;

    // Simulate loading many assets
    std::vector<AssetHandle> loaded_assets;
    for (int i = 0; i < 100; ++i) {
        auto asset_id = utils::generate_random_asset_id();

        // Mock load some assets (in real usage, these would come from packages)
        AssetLoadContext context;
        context.id = asset_id;
        context.cache_after_load = true;

        // In a real scenario, we would load from a package
        // auto result = asset_manager.load_asset(asset_id, context);
        // if (result) {
        //     loaded_assets.push_back(*result);
        // }
    }

    // Check memory usage
    auto current_usage = asset_manager.get_memory_usage();
    auto memory_pressure = asset_manager.get_memory_pressure();

    std::cout << "Current memory usage: " << current_usage / 1024 / 1024 << " MB" << std::endl;
    std::cout << "Memory pressure: " << (memory_pressure * 100.0f) << "%" << std::endl;

    // Trigger garbage collection
    std::cout << "Running garbage collection..." << std::endl;
    asset_manager.garbage_collect();

    auto usage_after_gc = asset_manager.get_memory_usage();
    std::cout << "Memory usage after GC: " << usage_after_gc / 1024 / 1024 << " MB" << std::endl;

    // Compact memory
    std::cout << "Compacting memory..." << std::endl;
    asset_manager.compact_memory();

    // Final statistics
    auto final_stats = asset_manager.get_statistics();
    std::cout << "Final Statistics:" << std::endl;
    std::cout << "  Total loads: " << final_stats.total_loads << std::endl;
    std::cout << "  Total unloads: " << final_stats.total_unloads << std::endl;
    std::cout << "  Cache hit ratio: " <<
        (final_stats.cache_hits * 100.0f / (final_stats.cache_hits + final_stats.cache_misses)) << "%" << std::endl;

    asset_manager.shutdown();
}

} // namespace lore::assets::examples

// Main function to run all examples
int main() {
    using namespace lore::assets::examples;

    try {
        demonstrate_basic_usage();
        demonstrate_ecs_integration();
        demonstrate_async_loading();
        demonstrate_hot_reload();
        demonstrate_compression_and_validation();
        demonstrate_memory_management();

        std::cout << "\n=== All Examples Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error running examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}