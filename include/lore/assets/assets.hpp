#pragma once

#include <lore/math/math.hpp>
#include <lore/ecs/ecs.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <future>
#include <span>
#include <expected>
#include <cstdint>
#include <fstream>

namespace lore::assets {

// Advanced Lore package system for high-performance asset management
// Designed for 100,000+ assets with streaming, compression, and hot-reloading

// Forward declarations
class AssetManager;
class AssetPackage;
class AssetStream;
class AssetCache;
class AssetLoader;
class AssetValidator;

// Asset identification and versioning
using AssetID = std::uint64_t;
using AssetTypeID = std::uint32_t;
using AssetVersion = std::uint32_t;
using PackageID = std::uint32_t;

constexpr AssetID INVALID_ASSET_ID = 0;
constexpr AssetTypeID INVALID_ASSET_TYPE = 0;
constexpr PackageID INVALID_PACKAGE_ID = 0;

// Asset types for runtime type safety
enum class AssetType : AssetTypeID {
    Unknown = 0,
    Texture2D = 1,
    Texture3D = 2,
    TextureCube = 3,
    Mesh = 4,
    Material = 5,
    Shader = 6,
    Audio = 7,
    Font = 8,
    Animation = 9,
    Skeleton = 10,
    Prefab = 11,
    Scene = 12,
    Script = 13,
    Binary = 14,
    Text = 15,
    JSON = 16,
    XML = 17,
    Configuration = 18,
    Localization = 19,
    // Custom types can be registered at runtime starting from 1000
    CustomStart = 1000
};

// Asset loading priority for resource management
enum class AssetPriority : std::uint8_t {
    Critical = 0,    // Must be loaded immediately (essential game assets)
    High = 1,        // Important but can wait briefly (UI, effects)
    Medium = 2,      // Standard priority (most game content)
    Low = 3,         // Can be delayed (background textures, ambient audio)
    Preload = 4      // Load during loading screens or idle time
};

// Asset compression algorithms
enum class CompressionType : std::uint8_t {
    None = 0,
    LZ4 = 1,         // Fast compression/decompression
    LZ4HC = 2,       // High compression ratio LZ4
    ZSTD = 3,        // Best compression ratio
    Custom = 255     // Custom compression algorithm
};

// Asset loading state for async operations
enum class AssetLoadState : std::uint8_t {
    Unloaded = 0,    // Asset not yet requested
    Queued = 1,      // Queued for loading
    Loading = 2,     // Currently being loaded
    Loaded = 3,      // Successfully loaded and available
    Failed = 4,      // Failed to load
    Unloading = 5,   // Being unloaded from memory
    Cached = 6       // Loaded but compressed in cache
};

// Asset error codes for robust error handling
enum class AssetError : std::uint16_t {
    None = 0,
    FileNotFound = 1,
    CorruptedData = 2,
    UnsupportedFormat = 3,
    InsufficientMemory = 4,
    CompressionError = 5,
    DecompressionError = 6,
    ValidationFailed = 7,
    DependencyMissing = 8,
    VersionMismatch = 9,
    AccessDenied = 10,
    NetworkTimeout = 11,
    InvalidPackage = 12,
    CircularDependency = 13,
    LoaderNotFound = 14,
    UnknownError = 255
};

// Asset metadata for efficient management
struct AssetMetadata {
    AssetID id = INVALID_ASSET_ID;
    AssetType type = AssetType::Unknown;
    std::string name;
    std::string path;
    std::uint64_t size_bytes = 0;
    std::uint64_t compressed_size_bytes = 0;
    AssetVersion version = 1;
    CompressionType compression = CompressionType::None;
    std::chrono::system_clock::time_point last_modified;
    std::chrono::system_clock::time_point created;
    std::vector<AssetID> dependencies;
    std::unordered_map<std::string, std::string> custom_properties;
    std::uint32_t checksum = 0;
    std::string hash_sha256;

    // Getters
    AssetID get_id() const noexcept { return id; }
    AssetType get_type() const noexcept { return type; }
    const std::string& get_name() const noexcept { return name; }
    const std::string& get_path() const noexcept { return path; }
    std::uint64_t get_size() const noexcept { return size_bytes; }
    std::uint64_t get_compressed_size() const noexcept { return compressed_size_bytes; }
    AssetVersion get_version() const noexcept { return version; }
    CompressionType get_compression() const noexcept { return compression; }
    const std::vector<AssetID>& get_dependencies() const noexcept { return dependencies; }
    std::uint32_t get_checksum() const noexcept { return checksum; }
    const std::string& get_hash() const noexcept { return hash_sha256; }

    // Setters
    void set_id(AssetID asset_id) noexcept { id = asset_id; }
    void set_type(AssetType asset_type) noexcept { type = asset_type; }
    void set_name(const std::string& asset_name) { name = asset_name; }
    void set_path(const std::string& asset_path) { path = asset_path; }
    void set_size(std::uint64_t bytes) noexcept { size_bytes = bytes; }
    void set_compressed_size(std::uint64_t bytes) noexcept { compressed_size_bytes = bytes; }
    void set_version(AssetVersion v) noexcept { version = v; }
    void set_compression(CompressionType comp) noexcept { compression = comp; }
    void set_checksum(std::uint32_t sum) noexcept { checksum = sum; }
    void set_hash(const std::string& sha256) { hash_sha256 = sha256; }

    // Dependency management
    void add_dependency(AssetID dependency) { dependencies.push_back(dependency); }
    void remove_dependency(AssetID dependency);
    bool has_dependency(AssetID dependency) const noexcept;
    void clear_dependencies() noexcept { dependencies.clear(); }

    // Custom properties
    void set_property(const std::string& key, const std::string& value) { custom_properties[key] = value; }
    std::string get_property(const std::string& key, const std::string& default_value = "") const;
    bool has_property(const std::string& key) const noexcept;
    void remove_property(const std::string& key) { custom_properties.erase(key); }

    // Validation
    bool is_valid() const noexcept;
    float get_compression_ratio() const noexcept;
};

// Asset handle for safe asset access with reference counting
class AssetHandle {
public:
    AssetHandle() = default;
    explicit AssetHandle(AssetID id, std::shared_ptr<void> data, const AssetMetadata& metadata);
    AssetHandle(const AssetHandle& other) = default;
    AssetHandle(AssetHandle&& other) noexcept = default;
    AssetHandle& operator=(const AssetHandle& other) = default;
    AssetHandle& operator=(AssetHandle&& other) noexcept = default;
    ~AssetHandle() = default;

    // Asset access
    template<typename T>
    T* get() const noexcept;

    template<typename T>
    const T* get_const() const noexcept;

    template<typename T>
    std::shared_ptr<T> get_shared() const noexcept;

    // Asset information
    AssetID get_id() const noexcept;
    AssetType get_type() const noexcept;
    const AssetMetadata& get_metadata() const noexcept;
    std::size_t get_reference_count() const noexcept;
    bool is_valid() const noexcept;
    bool is_loaded() const noexcept;

    // Operators
    explicit operator bool() const noexcept { return is_valid(); }
    bool operator==(const AssetHandle& other) const noexcept;
    bool operator!=(const AssetHandle& other) const noexcept;

private:
    AssetID id_ = INVALID_ASSET_ID;
    std::shared_ptr<void> data_;
    AssetMetadata metadata_;
};

// Asset loading context for async operations
struct AssetLoadContext {
    AssetID id = INVALID_ASSET_ID;
    AssetPriority priority = AssetPriority::Medium;
    bool load_dependencies = true;
    bool cache_after_load = true;
    std::function<void(AssetHandle)> on_loaded;
    std::function<void(AssetError)> on_failed;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000);

    // Getters
    AssetID get_id() const noexcept { return id; }
    AssetPriority get_priority() const noexcept { return priority; }
    bool get_load_dependencies() const noexcept { return load_dependencies; }
    bool get_cache_after_load() const noexcept { return cache_after_load; }
    std::chrono::milliseconds get_timeout() const noexcept { return timeout; }

    // Setters
    void set_id(AssetID asset_id) noexcept { id = asset_id; }
    void set_priority(AssetPriority p) noexcept { priority = p; }
    void set_load_dependencies(bool load_deps) noexcept { load_dependencies = load_deps; }
    void set_cache_after_load(bool cache) noexcept { cache_after_load = cache; }
    void set_timeout(std::chrono::milliseconds ms) noexcept { timeout = ms; }
};

// Asset streaming configuration
struct AssetStreamConfig {
    std::size_t buffer_size = 1024 * 1024;     // 1MB default buffer
    std::size_t chunk_size = 64 * 1024;        // 64KB chunks
    std::size_t max_concurrent_streams = 16;    // Concurrent streaming operations
    bool enable_prefetching = true;             // Prefetch next chunks
    bool enable_compression = true;             // Use compression during streaming
    CompressionType preferred_compression = CompressionType::LZ4;

    // Getters
    std::size_t get_buffer_size() const noexcept { return buffer_size; }
    std::size_t get_chunk_size() const noexcept { return chunk_size; }
    std::size_t get_max_concurrent_streams() const noexcept { return max_concurrent_streams; }
    bool get_enable_prefetching() const noexcept { return enable_prefetching; }
    bool get_enable_compression() const noexcept { return enable_compression; }
    CompressionType get_preferred_compression() const noexcept { return preferred_compression; }

    // Setters
    void set_buffer_size(std::size_t size) noexcept { buffer_size = size; }
    void set_chunk_size(std::size_t size) noexcept { chunk_size = size; }
    void set_max_concurrent_streams(std::size_t max_streams) noexcept { max_concurrent_streams = max_streams; }
    void set_enable_prefetching(bool enable) noexcept { enable_prefetching = enable; }
    void set_enable_compression(bool enable) noexcept { enable_compression = enable; }
    void set_preferred_compression(CompressionType compression) noexcept { preferred_compression = compression; }
};

// Hot-reload configuration for development
struct HotReloadConfig {
    bool enabled = false;
    std::chrono::milliseconds check_interval = std::chrono::milliseconds(1000);
    std::vector<std::string> watch_directories;
    std::vector<std::string> watch_extensions;
    bool recursive_watch = true;
    std::function<void(AssetID, const std::string&)> on_asset_changed;
    std::function<void(const std::string&)> on_file_added;
    std::function<void(const std::string&)> on_file_removed;

    // Getters
    bool get_enabled() const noexcept { return enabled; }
    std::chrono::milliseconds get_check_interval() const noexcept { return check_interval; }
    const std::vector<std::string>& get_watch_directories() const noexcept { return watch_directories; }
    const std::vector<std::string>& get_watch_extensions() const noexcept { return watch_extensions; }
    bool get_recursive_watch() const noexcept { return recursive_watch; }

    // Setters
    void set_enabled(bool enable) noexcept { enabled = enable; }
    void set_check_interval(std::chrono::milliseconds interval) noexcept { check_interval = interval; }
    void set_recursive_watch(bool recursive) noexcept { recursive_watch = recursive; }

    // Directory and extension management
    void add_watch_directory(const std::string& directory) { watch_directories.push_back(directory); }
    void remove_watch_directory(const std::string& directory);
    void add_watch_extension(const std::string& extension) { watch_extensions.push_back(extension); }
    void remove_watch_extension(const std::string& extension);
    void clear_watch_directories() { watch_directories.clear(); }
    void clear_watch_extensions() { watch_extensions.clear(); }
};

// Asset component for ECS integration
struct AssetComponent {
    AssetID asset_id = INVALID_ASSET_ID;
    AssetHandle handle;
    AssetLoadState load_state = AssetLoadState::Unloaded;
    AssetPriority priority = AssetPriority::Medium;
    bool auto_load = true;
    bool keep_loaded = false;
    std::chrono::system_clock::time_point last_accessed;

    // Getters
    AssetID get_asset_id() const noexcept { return asset_id; }
    const AssetHandle& get_handle() const noexcept { return handle; }
    AssetLoadState get_load_state() const noexcept { return load_state; }
    AssetPriority get_priority() const noexcept { return priority; }
    bool get_auto_load() const noexcept { return auto_load; }
    bool get_keep_loaded() const noexcept { return keep_loaded; }

    // Setters
    void set_asset_id(AssetID id) noexcept { asset_id = id; }
    void set_handle(const AssetHandle& h) { handle = h; }
    void set_load_state(AssetLoadState state) noexcept { load_state = state; }
    void set_priority(AssetPriority p) noexcept { priority = p; }
    void set_auto_load(bool auto_loading) noexcept { auto_load = auto_loading; }
    void set_keep_loaded(bool keep) noexcept { keep_loaded = keep; }

    // Asset operations
    template<typename T>
    T* get_asset() const noexcept { return handle.get<T>(); }

    bool is_loaded() const noexcept { return load_state == AssetLoadState::Loaded && handle.is_valid(); }
    bool is_loading() const noexcept { return load_state == AssetLoadState::Loading || load_state == AssetLoadState::Queued; }
    bool failed_to_load() const noexcept { return load_state == AssetLoadState::Failed; }

    void mark_accessed() noexcept { last_accessed = std::chrono::system_clock::now(); }
    std::chrono::milliseconds time_since_last_access() const noexcept;
};

// Asset reference component for lightweight asset referencing
struct AssetReferenceComponent {
    std::string asset_path;
    AssetType expected_type = AssetType::Unknown;
    bool resolved = false;
    AssetID resolved_id = INVALID_ASSET_ID;

    // Getters
    const std::string& get_asset_path() const noexcept { return asset_path; }
    AssetType get_expected_type() const noexcept { return expected_type; }
    bool get_resolved() const noexcept { return resolved; }
    AssetID get_resolved_id() const noexcept { return resolved_id; }

    // Setters
    void set_asset_path(const std::string& path) { asset_path = path; }
    void set_expected_type(AssetType type) noexcept { expected_type = type; }
    void set_resolved(bool is_resolved) noexcept { resolved = is_resolved; }
    void set_resolved_id(AssetID id) noexcept { resolved_id = id; }
};

// Result type for operations that can fail
template<typename T>
using AssetResult = std::expected<T, AssetError>;

// Main asset system for ECS integration
class AssetSystem : public ecs::System {
public:
    AssetSystem();
    ~AssetSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Asset manager access
    AssetManager& get_asset_manager() noexcept;
    const AssetManager& get_asset_manager() const noexcept;

    // ECS-specific asset operations
    void preload_entity_assets(ecs::World& world, ecs::EntityHandle entity) noexcept;
    void unload_entity_assets(ecs::World& world, ecs::EntityHandle entity) noexcept;
    void resolve_asset_references(ecs::World& world) noexcept;

    // Bulk operations for performance
    void preload_all_assets(ecs::World& world) noexcept;
    void unload_unused_assets(ecs::World& world) noexcept;
    void garbage_collect_assets(ecs::World& world) noexcept;

    // Asset loading statistics
    std::size_t get_loaded_asset_count() const noexcept;
    std::size_t get_loading_asset_count() const noexcept;
    std::size_t get_failed_asset_count() const noexcept;
    std::size_t get_total_memory_usage() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Main asset manager class
class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Initialization and configuration
    void initialize(const AssetStreamConfig& stream_config = {},
                   const HotReloadConfig& hot_reload_config = {}) noexcept;
    void shutdown() noexcept;
    bool is_initialized() const noexcept;

    // Package management
    AssetResult<PackageID> load_package(const std::string& package_path) noexcept;
    AssetResult<void> unload_package(PackageID package_id) noexcept;
    AssetResult<PackageID> create_package(const std::string& package_path) noexcept;
    bool is_package_loaded(PackageID package_id) const noexcept;
    std::vector<PackageID> get_loaded_packages() const noexcept;

    // Asset loading (async by default)
    std::future<AssetResult<AssetHandle>> load_asset_async(AssetID id, const AssetLoadContext& context = {}) noexcept;
    std::future<AssetResult<AssetHandle>> load_asset_async(const std::string& path, const AssetLoadContext& context = {}) noexcept;

    // Asset loading (blocking)
    AssetResult<AssetHandle> load_asset(AssetID id, const AssetLoadContext& context = {}) noexcept;
    AssetResult<AssetHandle> load_asset(const std::string& path, const AssetLoadContext& context = {}) noexcept;

    // Asset unloading
    AssetResult<void> unload_asset(AssetID id) noexcept;
    AssetResult<void> unload_asset(const AssetHandle& handle) noexcept;
    void unload_all_assets() noexcept;

    // Asset lookup and metadata
    AssetResult<AssetMetadata> get_asset_metadata(AssetID id) const noexcept;
    AssetResult<AssetMetadata> get_asset_metadata(const std::string& path) const noexcept;
    AssetResult<AssetID> get_asset_id(const std::string& path) const noexcept;
    AssetResult<std::string> get_asset_path(AssetID id) const noexcept;
    bool is_asset_loaded(AssetID id) const noexcept;
    AssetLoadState get_asset_load_state(AssetID id) const noexcept;

    // Asset enumeration
    std::vector<AssetID> get_all_asset_ids() const noexcept;
    std::vector<AssetID> get_assets_by_type(AssetType type) const noexcept;
    std::vector<AssetID> get_loaded_assets() const noexcept;
    std::vector<AssetID> get_assets_in_package(PackageID package_id) const noexcept;

    // Asset validation and integrity
    AssetResult<bool> validate_asset(AssetID id) const noexcept;
    AssetResult<bool> validate_package(PackageID package_id) const noexcept;
    AssetResult<void> repair_asset(AssetID id) noexcept;
    AssetResult<void> recompute_checksums(PackageID package_id) noexcept;

    // Dependency management
    AssetResult<std::vector<AssetID>> get_asset_dependencies(AssetID id) const noexcept;
    AssetResult<std::vector<AssetID>> get_asset_dependents(AssetID id) const noexcept;
    AssetResult<std::vector<AssetID>> resolve_loading_order(const std::vector<AssetID>& assets) const noexcept;
    AssetResult<void> preload_dependencies(AssetID id) noexcept;

    // Hot-reloading
    void enable_hot_reload(const HotReloadConfig& config) noexcept;
    void disable_hot_reload() noexcept;
    bool is_hot_reload_enabled() const noexcept;
    AssetResult<void> reload_asset(AssetID id) noexcept;
    AssetResult<void> reload_changed_assets() noexcept;

    // Streaming and caching
    void set_stream_config(const AssetStreamConfig& config) noexcept;
    const AssetStreamConfig& get_stream_config() const noexcept;
    void clear_cache() noexcept;
    void set_cache_size_limit(std::size_t bytes) noexcept;
    std::size_t get_cache_size_limit() const noexcept;
    std::size_t get_current_cache_usage() const noexcept;

    // Custom asset loaders
    template<typename T>
    void register_asset_loader(AssetType type, std::unique_ptr<AssetLoader> loader) noexcept;

    void unregister_asset_loader(AssetType type) noexcept;
    bool has_asset_loader(AssetType type) const noexcept;

    // Custom asset types
    AssetTypeID register_asset_type(const std::string& type_name) noexcept;
    AssetResult<std::string> get_asset_type_name(AssetTypeID type_id) const noexcept;
    AssetResult<AssetTypeID> get_asset_type_id(const std::string& type_name) const noexcept;

    // Memory management
    void garbage_collect() noexcept;
    void compact_memory() noexcept;
    void set_memory_budget(std::size_t bytes) noexcept;
    std::size_t get_memory_budget() const noexcept;
    std::size_t get_memory_usage() const noexcept;
    float get_memory_pressure() const noexcept;

    // Statistics and monitoring
    struct Statistics {
        std::size_t total_assets = 0;
        std::size_t loaded_assets = 0;
        std::size_t cached_assets = 0;
        std::size_t failed_assets = 0;
        std::size_t memory_usage_bytes = 0;
        std::size_t cache_usage_bytes = 0;
        std::size_t total_loads = 0;
        std::size_t total_unloads = 0;
        std::size_t cache_hits = 0;
        std::size_t cache_misses = 0;
        std::chrono::milliseconds average_load_time{0};
        std::chrono::milliseconds total_load_time{0};
    };

    Statistics get_statistics() const noexcept;
    void reset_statistics() noexcept;

    // Threading and async operations
    void set_worker_thread_count(std::size_t count) noexcept;
    std::size_t get_worker_thread_count() const noexcept;
    void wait_for_all_loads() noexcept;
    void cancel_all_loads() noexcept;

    // Event callbacks
    using AssetLoadedCallback = std::function<void(AssetID, AssetHandle)>;
    using AssetUnloadedCallback = std::function<void(AssetID)>;
    using AssetFailedCallback = std::function<void(AssetID, AssetError)>;
    using AssetReloadedCallback = std::function<void(AssetID, AssetHandle)>;

    void set_asset_loaded_callback(AssetLoadedCallback callback) noexcept;
    void set_asset_unloaded_callback(AssetUnloadedCallback callback) noexcept;
    void set_asset_failed_callback(AssetFailedCallback callback) noexcept;
    void set_asset_reloaded_callback(AssetReloadedCallback callback) noexcept;

    // Prevent copying
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Base asset loader interface for custom asset types
class AssetLoader {
public:
    virtual ~AssetLoader() = default;

    // Core loading interface
    virtual AssetResult<std::shared_ptr<void>> load(std::span<const std::uint8_t> data,
                                                   const AssetMetadata& metadata) = 0;
    virtual AssetResult<void> unload(std::shared_ptr<void> asset_data) = 0;

    // Optional streaming support
    virtual bool supports_streaming() const noexcept { return false; }
    virtual AssetResult<std::shared_ptr<void>> load_streaming(std::istream& stream,
                                                             const AssetMetadata& metadata) {
        return std::unexpected(AssetError::UnsupportedFormat);
    }

    // Optional hot-reload support
    virtual bool supports_hot_reload() const noexcept { return false; }
    virtual AssetResult<std::shared_ptr<void>> reload(std::shared_ptr<void> existing_data,
                                                     std::span<const std::uint8_t> new_data,
                                                     const AssetMetadata& metadata) {
        return load(new_data, metadata);
    }

    // Validation support
    virtual AssetResult<bool> validate(std::span<const std::uint8_t> data,
                                      const AssetMetadata& metadata) const noexcept {
        return true;
    }

    // Metadata extraction
    virtual AssetResult<AssetMetadata> extract_metadata(std::span<const std::uint8_t> data,
                                                        const std::string& path) const noexcept = 0;

    // Asset type information
    virtual AssetType get_asset_type() const noexcept = 0;
    virtual std::vector<std::string> get_supported_extensions() const noexcept = 0;
    virtual std::string get_loader_name() const noexcept = 0;
    virtual std::string get_loader_version() const noexcept = 0;
};

// Utility functions for asset management
namespace utils {
    // Path utilities
    std::string normalize_path(const std::string& path) noexcept;
    std::string get_file_extension(const std::string& path) noexcept;
    std::string get_filename(const std::string& path) noexcept;
    std::string get_directory(const std::string& path) noexcept;
    bool is_absolute_path(const std::string& path) noexcept;
    std::string make_relative_path(const std::string& path, const std::string& base) noexcept;

    // File I/O utilities
    AssetResult<std::vector<std::uint8_t>> read_file(const std::string& path) noexcept;
    AssetResult<void> write_file(const std::string& path, std::span<const std::uint8_t> data) noexcept;
    bool file_exists(const std::string& path) noexcept;
    std::chrono::system_clock::time_point get_file_modification_time(const std::string& path) noexcept;
    std::uint64_t get_file_size(const std::string& path) noexcept;

    // Hashing and validation
    std::uint32_t calculate_crc32(std::span<const std::uint8_t> data) noexcept;
    std::string calculate_sha256(std::span<const std::uint8_t> data) noexcept;
    bool validate_checksum(std::span<const std::uint8_t> data, std::uint32_t expected_crc32) noexcept;
    bool validate_hash(std::span<const std::uint8_t> data, const std::string& expected_sha256) noexcept;

    // Compression utilities
    AssetResult<std::vector<std::uint8_t>> compress_data(std::span<const std::uint8_t> data,
                                                        CompressionType type) noexcept;
    AssetResult<std::vector<std::uint8_t>> decompress_data(std::span<const std::uint8_t> compressed_data,
                                                          CompressionType type,
                                                          std::size_t expected_size = 0) noexcept;

    float get_compression_ratio(std::size_t original_size, std::size_t compressed_size) noexcept;
    CompressionType detect_compression_type(std::span<const std::uint8_t> data) noexcept;

    // Asset ID generation
    AssetID generate_asset_id(const std::string& path) noexcept;
    AssetID generate_random_asset_id() noexcept;
    bool is_valid_asset_id(AssetID id) noexcept;

    // Type conversion utilities
    std::string asset_type_to_string(AssetType type) noexcept;
    AssetType string_to_asset_type(const std::string& type_name) noexcept;
    std::string asset_error_to_string(AssetError error) noexcept;
    std::string load_state_to_string(AssetLoadState state) noexcept;
    std::string compression_type_to_string(CompressionType type) noexcept;

    // Memory utilities
    std::size_t get_asset_memory_usage(const AssetHandle& handle) noexcept;
    void* align_memory(void* ptr, std::size_t alignment) noexcept;
    std::size_t calculate_aligned_size(std::size_t size, std::size_t alignment) noexcept;

    // JSON serialization for metadata
    std::string serialize_metadata_to_json(const AssetMetadata& metadata) noexcept;
    AssetResult<AssetMetadata> deserialize_metadata_from_json(const std::string& json) noexcept;

    // Performance profiling
    class AssetProfiler {
    public:
        static AssetProfiler& instance() noexcept;

        void begin_load_profile(AssetID id) noexcept;
        void end_load_profile(AssetID id) noexcept;
        void begin_unload_profile(AssetID id) noexcept;
        void end_unload_profile(AssetID id) noexcept;

        std::chrono::microseconds get_average_load_time() const noexcept;
        std::chrono::microseconds get_average_unload_time() const noexcept;
        std::chrono::microseconds get_load_time(AssetID id) const noexcept;

        void reset_statistics() noexcept;
        void enable_profiling(bool enabled) noexcept;
        bool is_profiling_enabled() const noexcept;

    private:
        AssetProfiler() = default;
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
}

} // namespace lore::assets