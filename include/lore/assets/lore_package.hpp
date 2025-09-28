#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <span>
#include <optional>
#include <expected>
#include <shared_mutex>
#include <atomic>
#include <filesystem>

namespace lore::assets {

// Lore Package System - High-Performance Asset Management
// Designed for zero-copy asset streaming with thread-safe random access
// Supports compression, integrity validation, and dependency tracking

// ==============================================================================
// Lore Binary Format Specification
// ==============================================================================

// File Format Version
constexpr std::uint32_t LORE_FORMAT_VERSION_MAJOR = 1;
constexpr std::uint32_t LORE_FORMAT_VERSION_MINOR = 0;
constexpr std::uint32_t LORE_FORMAT_VERSION_PATCH = 0;

// Magic number: "LORE" in ASCII (0x45524F4C)
constexpr std::array<std::uint8_t, 4> LORE_MAGIC_NUMBER = {0x4C, 0x4F, 0x52, 0x45};

// Maximum values for safety validation
constexpr std::size_t MAX_ASSET_COUNT = 1'000'000;
constexpr std::size_t MAX_ASSET_NAME_LENGTH = 256;
constexpr std::size_t MAX_PACKAGE_SIZE = 16ULL * 1024 * 1024 * 1024; // 16GB
constexpr std::size_t MAX_ASSET_SIZE = 512ULL * 1024 * 1024; // 512MB per asset
constexpr std::size_t MAX_DEPENDENCY_COUNT = 64;
constexpr std::size_t MAX_METADATA_SIZE = 64 * 1024; // 64KB

// ==============================================================================
// Asset Type System
// ==============================================================================

enum class AssetType : std::uint32_t {
    Unknown     = 0x00000000,
    Texture2D   = 0x54455832,  // "TEX2"
    Texture3D   = 0x54455833,  // "TEX3"
    TextureCube = 0x54455843,  // "TEXC"
    Audio       = 0x41554449,  // "AUDI"
    Mesh        = 0x4D455348,  // "MESH"
    Model       = 0x4D4F444C,  // "MODL"
    Font        = 0x464F4E54,  // "FONT"
    Shader      = 0x53484452,  // "SHDR"
    Animation   = 0x414E494D,  // "ANIM"
    Material    = 0x4D415452,  // "MATR"
    Scene       = 0x53434E45,  // "SCNE"
    Script      = 0x53435254,  // "SCRT"
    Data        = 0x44415441,  // "DATA"
    Config      = 0x434F4E46,  // "CONF"
    Prefab      = 0x50524642,  // "PRFB"
};

// Asset format specifications for different types
enum class TextureFormat : std::uint32_t {
    Unknown = 0,
    RGBA8   = 1,
    RGBA16F = 2,
    RGBA32F = 3,
    DXT1    = 4,
    DXT5    = 5,
    BC7     = 6,
    ASTC    = 7,
};

enum class AudioFormat : std::uint32_t {
    Unknown = 0,
    PCM16   = 1,
    PCM24   = 2,
    PCM32F  = 3,
    Vorbis  = 4,
    MP3     = 5,
    FLAC    = 6,
};

enum class MeshFormat : std::uint32_t {
    Unknown  = 0,
    Static   = 1,
    Skinned  = 2,
    Morph    = 3,
    Instanced = 4,
};

// ==============================================================================
// Compression System
// ==============================================================================

enum class CompressionType : std::uint32_t {
    None = 0x4E4F4E45,  // "NONE"
    LZ4  = 0x4C5A3420,  // "LZ4 "
    ZSTD = 0x5A535444,  // "ZSTD"
    LZMA = 0x4C5A4D41,  // "LZMA"
};

enum class HashType : std::uint32_t {
    None    = 0x4E4F4E45,  // "NONE"
    CRC32   = 0x43524332,  // "CRC2"
    SHA256  = 0x53484132,  // "SHA2"
    BLAKE3  = 0x424C4B33,  // "BLK3"
    XXH3    = 0x58584833,  // "XXH3"
};

// ==============================================================================
// Binary Format Structures (Packed for Disk Layout)
// ==============================================================================

#pragma pack(push, 1)

// Main package header - exactly 256 bytes for cache alignment
struct PackageHeader {
    std::array<std::uint8_t, 4> magic_number;      // 4 bytes: LORE magic
    std::uint32_t version_major;                    // 4 bytes
    std::uint32_t version_minor;                    // 4 bytes
    std::uint32_t version_patch;                    // 4 bytes
    std::uint64_t creation_timestamp;               // 8 bytes: Unix timestamp
    std::uint64_t modification_timestamp;           // 8 bytes: Unix timestamp
    std::uint64_t package_size;                     // 8 bytes: Total file size
    std::uint32_t asset_count;                      // 4 bytes: Number of assets
    std::uint32_t compression_flags;                // 4 bytes: Global compression settings
    HashType integrity_hash_type;                   // 4 bytes: Hash algorithm
    std::array<std::uint8_t, 32> integrity_hash;   // 32 bytes: Package integrity hash
    std::uint64_t asset_table_offset;              // 8 bytes: Offset to asset table
    std::uint64_t asset_table_size;                // 8 bytes: Size of asset table
    std::uint64_t string_table_offset;             // 8 bytes: Offset to string table
    std::uint64_t string_table_size;               // 8 bytes: Size of string table
    std::uint64_t dependency_table_offset;         // 8 bytes: Offset to dependency table
    std::uint64_t dependency_table_size;           // 8 bytes: Size of dependency table
    std::uint64_t metadata_offset;                 // 8 bytes: Offset to package metadata
    std::uint64_t metadata_size;                   // 8 bytes: Size of package metadata
    std::array<std::uint8_t, 128> reserved;        // 128 bytes: Reserved for future use
};

// Asset entry in the asset table - exactly 128 bytes
struct AssetEntry {
    std::uint32_t asset_id;                         // 4 bytes: Unique asset identifier
    AssetType asset_type;                           // 4 bytes: Type of asset
    std::uint32_t format_type;                      // 4 bytes: Format-specific type
    std::uint32_t name_offset;                      // 4 bytes: Offset in string table
    std::uint32_t name_length;                      // 4 bytes: Length of asset name
    std::uint64_t data_offset;                      // 8 bytes: Offset to asset data
    std::uint64_t compressed_size;                  // 8 bytes: Size when compressed
    std::uint64_t uncompressed_size;                // 8 bytes: Size when uncompressed
    CompressionType compression_type;               // 4 bytes: Compression algorithm
    HashType hash_type;                             // 4 bytes: Hash algorithm
    std::array<std::uint8_t, 32> data_hash;        // 32 bytes: Asset data hash
    std::uint64_t creation_timestamp;               // 8 bytes: Asset creation time
    std::uint64_t modification_timestamp;           // 8 bytes: Asset modification time
    std::uint32_t dependency_count;                 // 4 bytes: Number of dependencies
    std::uint32_t dependency_offset;                // 4 bytes: Offset in dependency table
    std::uint32_t metadata_size;                    // 4 bytes: Size of asset metadata
    std::uint32_t metadata_offset;                  // 4 bytes: Offset to asset metadata
    std::array<std::uint8_t, 16> reserved;          // 16 bytes: Reserved for future use
};

// Dependency entry in the dependency table - exactly 32 bytes
struct DependencyEntry {
    std::uint32_t dependent_asset_id;               // 4 bytes: Asset that depends
    std::uint32_t dependency_asset_id;              // 4 bytes: Asset being depended on
    std::uint32_t dependency_type;                  // 4 bytes: Type of dependency
    std::uint32_t flags;                            // 4 bytes: Dependency flags
    std::uint64_t version_hash;                     // 8 bytes: Version compatibility hash
    std::array<std::uint8_t, 8> reserved;          // 8 bytes: Reserved for future use
};

// Compression info block - exactly 64 bytes
struct CompressionInfo {
    CompressionType compression_type;               // 4 bytes: Algorithm used
    std::uint32_t compression_level;                // 4 bytes: Compression level
    std::uint64_t compressed_size;                  // 8 bytes: Size after compression
    std::uint64_t uncompressed_size;                // 8 bytes: Original size
    std::uint32_t block_size;                       // 4 bytes: Compression block size
    std::uint32_t block_count;                      // 4 bytes: Number of blocks
    HashType hash_type;                             // 4 bytes: Hash algorithm
    std::array<std::uint8_t, 32> compressed_hash;  // 32 bytes: Hash of compressed data
};

#pragma pack(pop)

// ==============================================================================
// Error Handling System
// ==============================================================================

enum class LoreError {
    Success = 0,
    FileNotFound,
    InvalidFormat,
    CorruptedHeader,
    CorruptedAsset,
    UnsupportedVersion,
    UnsupportedCompression,
    DecompressionFailed,
    HashMismatch,
    AssetNotFound,
    DependencyNotFound,
    MemoryAllocationFailed,
    IOError,
    InvalidAssetType,
    AssetTooLarge,
    PackageTooLarge,
    TooManyAssets,
    CircularDependency,
    AccessDenied,
    ThreadingError,
    InvalidParameter,
    BufferOverflow,
    Timeout,
};

template<typename T>
using Result = std::expected<T, LoreError>;

// ==============================================================================
// Asset Metadata System
// ==============================================================================

class AssetMetadata {
public:
    // Construction
    AssetMetadata() = default;
    explicit AssetMetadata(std::span<const std::uint8_t> data);

    // Metadata access
    template<typename T>
    Result<T> get_value(const std::string& key) const;

    template<typename T>
    void set_value(const std::string& key, const T& value);

    bool has_key(const std::string& key) const noexcept;
    std::vector<std::string> get_keys() const;

    // Serialization
    std::vector<std::uint8_t> serialize() const;
    Result<void> deserialize(std::span<const std::uint8_t> data);

    // Size information
    std::size_t get_size() const noexcept;
    bool is_empty() const noexcept;
    void clear() noexcept;

private:
    std::unordered_map<std::string, std::vector<std::uint8_t>> metadata_;
    mutable std::shared_mutex mutex_;
};

// ==============================================================================
// Asset Information
// ==============================================================================

class AssetInfo {
public:
    // Construction
    AssetInfo() = default;
    AssetInfo(const AssetEntry& entry, std::string name);

    // Basic properties
    std::uint32_t get_asset_id() const noexcept { return asset_id_; }
    AssetType get_asset_type() const noexcept { return asset_type_; }
    std::uint32_t get_format_type() const noexcept { return format_type_; }
    const std::string& get_name() const noexcept { return name_; }

    // Size information
    std::uint64_t get_compressed_size() const noexcept { return compressed_size_; }
    std::uint64_t get_uncompressed_size() const noexcept { return uncompressed_size_; }
    float get_compression_ratio() const noexcept;

    // Compression and integrity
    CompressionType get_compression_type() const noexcept { return compression_type_; }
    HashType get_hash_type() const noexcept { return hash_type_; }
    std::span<const std::uint8_t> get_data_hash() const noexcept;

    // Timestamps
    std::chrono::system_clock::time_point get_creation_time() const noexcept;
    std::chrono::system_clock::time_point get_modification_time() const noexcept;

    // Dependencies
    std::span<const std::uint32_t> get_dependencies() const noexcept;
    bool has_dependencies() const noexcept;
    std::size_t get_dependency_count() const noexcept;

    // Metadata
    const AssetMetadata& get_metadata() const noexcept { return metadata_; }
    AssetMetadata& get_metadata() noexcept { return metadata_; }

    // Validation
    bool is_valid() const noexcept;
    Result<void> validate_integrity(std::span<const std::uint8_t> data) const;

private:
    std::uint32_t asset_id_ = 0;
    AssetType asset_type_ = AssetType::Unknown;
    std::uint32_t format_type_ = 0;
    std::string name_;
    std::uint64_t compressed_size_ = 0;
    std::uint64_t uncompressed_size_ = 0;
    CompressionType compression_type_ = CompressionType::None;
    HashType hash_type_ = HashType::None;
    std::array<std::uint8_t, 32> data_hash_{};
    std::uint64_t creation_timestamp_ = 0;
    std::uint64_t modification_timestamp_ = 0;
    std::vector<std::uint32_t> dependencies_;
    AssetMetadata metadata_;
};

// ==============================================================================
// Asset Stream - Zero-Copy Asset Access
// ==============================================================================

class AssetStream {
public:
    // Construction
    AssetStream() = default;
    AssetStream(std::span<const std::uint8_t> data, const AssetInfo& info);

    // Stream properties
    std::size_t get_size() const noexcept { return data_.size(); }
    bool is_empty() const noexcept { return data_.empty(); }
    bool is_compressed() const noexcept;

    // Asset information
    const AssetInfo& get_asset_info() const noexcept { return asset_info_; }

    // Raw data access (compressed)
    std::span<const std::uint8_t> get_raw_data() const noexcept { return data_; }

    // Decompressed data access
    Result<std::vector<std::uint8_t>> get_decompressed_data() const;
    Result<std::span<const std::uint8_t>> get_decompressed_span() const;

    // Streaming operations
    Result<std::size_t> read(std::span<std::uint8_t> buffer, std::size_t offset = 0) const;
    Result<std::vector<std::uint8_t>> read_range(std::size_t offset, std::size_t size) const;

    // Validation
    Result<void> validate_integrity() const;
    bool is_valid() const noexcept;

private:
    std::span<const std::uint8_t> data_;
    AssetInfo asset_info_;
    mutable std::vector<std::uint8_t> decompressed_cache_;
    mutable bool cache_valid_ = false;
    mutable std::shared_mutex cache_mutex_;
};

// ==============================================================================
// Package Information
// ==============================================================================

class PackageInfo {
public:
    // Construction
    PackageInfo() = default;
    explicit PackageInfo(const PackageHeader& header);

    // Version information
    std::uint32_t get_version_major() const noexcept { return version_major_; }
    std::uint32_t get_version_minor() const noexcept { return version_minor_; }
    std::uint32_t get_version_patch() const noexcept { return version_patch_; }
    std::string get_version_string() const;

    // Timestamps
    std::chrono::system_clock::time_point get_creation_time() const noexcept;
    std::chrono::system_clock::time_point get_modification_time() const noexcept;

    // Size information
    std::uint64_t get_package_size() const noexcept { return package_size_; }
    std::uint32_t get_asset_count() const noexcept { return asset_count_; }

    // Compression settings
    std::uint32_t get_compression_flags() const noexcept { return compression_flags_; }
    HashType get_integrity_hash_type() const noexcept { return integrity_hash_type_; }
    std::span<const std::uint8_t> get_integrity_hash() const noexcept;

    // Validation
    bool is_valid() const noexcept;
    bool is_compatible_version() const noexcept;

private:
    std::uint32_t version_major_ = 0;
    std::uint32_t version_minor_ = 0;
    std::uint32_t version_patch_ = 0;
    std::uint64_t creation_timestamp_ = 0;
    std::uint64_t modification_timestamp_ = 0;
    std::uint64_t package_size_ = 0;
    std::uint32_t asset_count_ = 0;
    std::uint32_t compression_flags_ = 0;
    HashType integrity_hash_type_ = HashType::None;
    std::array<std::uint8_t, 32> integrity_hash_{};
};

// ==============================================================================
// Lore Package - Main Package Interface
// ==============================================================================

class LorePackage {
public:
    // Construction and lifecycle
    LorePackage();
    ~LorePackage();

    // Package loading
    Result<void> load_from_file(const std::filesystem::path& file_path);
    Result<void> load_from_memory(std::span<const std::uint8_t> data);
    Result<void> load_from_stream(std::istream& stream);

    // Package information
    const PackageInfo& get_package_info() const noexcept { return package_info_; }
    bool is_loaded() const noexcept { return is_loaded_; }
    const std::filesystem::path& get_file_path() const noexcept { return file_path_; }

    // Asset enumeration
    std::vector<std::uint32_t> get_asset_ids() const;
    std::vector<std::string> get_asset_names() const;
    std::vector<AssetType> get_asset_types() const;
    std::size_t get_asset_count() const noexcept;

    // Asset queries
    bool has_asset(std::uint32_t asset_id) const noexcept;
    bool has_asset(const std::string& asset_name) const noexcept;
    Result<std::uint32_t> get_asset_id(const std::string& asset_name) const;
    Result<std::string> get_asset_name(std::uint32_t asset_id) const;

    // Asset information
    Result<AssetInfo> get_asset_info(std::uint32_t asset_id) const;
    Result<AssetInfo> get_asset_info(const std::string& asset_name) const;

    // Asset streaming
    Result<AssetStream> get_asset_stream(std::uint32_t asset_id) const;
    Result<AssetStream> get_asset_stream(const std::string& asset_name) const;

    // Asset data access
    Result<std::vector<std::uint8_t>> load_asset_data(std::uint32_t asset_id) const;
    Result<std::vector<std::uint8_t>> load_asset_data(const std::string& asset_name) const;

    // Dependency management
    Result<std::vector<std::uint32_t>> get_asset_dependencies(std::uint32_t asset_id) const;
    Result<std::vector<std::uint32_t>> get_dependent_assets(std::uint32_t asset_id) const;
    Result<std::vector<std::uint32_t>> resolve_dependency_chain(std::uint32_t asset_id) const;

    // Asset filtering
    std::vector<std::uint32_t> find_assets_by_type(AssetType type) const;
    std::vector<std::uint32_t> find_assets_by_name_pattern(const std::string& pattern) const;
    std::vector<std::uint32_t> find_assets_by_metadata(const std::string& key, const std::string& value) const;

    // Package validation
    Result<void> validate_package_integrity() const;
    Result<void> validate_asset_integrity(std::uint32_t asset_id) const;
    Result<std::vector<std::uint32_t>> find_corrupted_assets() const;

    // Statistics
    std::uint64_t get_total_compressed_size() const noexcept;
    std::uint64_t get_total_uncompressed_size() const noexcept;
    float get_overall_compression_ratio() const noexcept;
    std::unordered_map<AssetType, std::size_t> get_asset_type_counts() const;

    // Thread safety
    void enable_thread_safety(bool enabled = true) noexcept { thread_safe_ = enabled; }
    bool is_thread_safe() const noexcept { return thread_safe_; }

    // Memory management
    void clear_cache() noexcept;
    std::size_t get_memory_usage() const noexcept;
    void set_memory_limit(std::size_t limit_bytes) noexcept { memory_limit_ = limit_bytes; }
    std::size_t get_memory_limit() const noexcept { return memory_limit_; }

    // Prevent copying (expensive operation)
    LorePackage(const LorePackage&) = delete;
    LorePackage& operator=(const LorePackage&) = delete;

    // Move semantics
    LorePackage(LorePackage&& other) noexcept;
    LorePackage& operator=(LorePackage&& other) noexcept;

private:
    // Implementation details
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Package state
    bool is_loaded_ = false;
    bool thread_safe_ = true;
    std::filesystem::path file_path_;
    PackageInfo package_info_;
    std::size_t memory_limit_ = 512ULL * 1024 * 1024; // 512MB default

    // Thread safety
    mutable std::shared_mutex package_mutex_;

    // Internal methods
    Result<void> validate_header(const PackageHeader& header) const;
    Result<std::vector<AssetEntry>> load_asset_table() const;
    Result<std::vector<std::string>> load_string_table() const;
    Result<std::vector<DependencyEntry>> load_dependency_table() const;
    Result<void> build_asset_index();
    Result<void> validate_dependencies() const;
};

// ==============================================================================
// Package Builder - For Creating Lore Packages
// ==============================================================================

class LorePackageBuilder {
public:
    // Construction
    LorePackageBuilder();
    ~LorePackageBuilder();

    // Package configuration
    void set_compression_type(CompressionType type) noexcept { default_compression_ = type; }
    void set_compression_level(std::uint32_t level) noexcept { compression_level_ = level; }
    void set_hash_type(HashType type) noexcept { default_hash_type_ = type; }
    void set_block_size(std::uint32_t size) noexcept { block_size_ = size; }

    // Asset addition
    Result<std::uint32_t> add_asset(
        const std::string& name,
        AssetType type,
        std::span<const std::uint8_t> data,
        std::uint32_t format_type = 0,
        CompressionType compression = CompressionType::None
    );

    Result<std::uint32_t> add_asset_from_file(
        const std::string& name,
        AssetType type,
        const std::filesystem::path& file_path,
        std::uint32_t format_type = 0,
        CompressionType compression = CompressionType::None
    );

    // Asset metadata
    Result<void> set_asset_metadata(std::uint32_t asset_id, const AssetMetadata& metadata);
    Result<void> add_asset_dependency(std::uint32_t asset_id, std::uint32_t dependency_id);

    // Package building
    Result<void> build_to_file(const std::filesystem::path& output_path);
    Result<std::vector<std::uint8_t>> build_to_memory();
    Result<void> build_to_stream(std::ostream& stream);

    // Statistics
    std::size_t get_asset_count() const noexcept;
    std::uint64_t get_estimated_size() const noexcept;

    // Validation
    Result<void> validate_build_state() const;

    // Clear all assets
    void clear() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Build configuration
    CompressionType default_compression_ = CompressionType::LZ4;
    std::uint32_t compression_level_ = 1;
    HashType default_hash_type_ = HashType::XXH3;
    std::uint32_t block_size_ = 64 * 1024; // 64KB blocks
};

// ==============================================================================
// Utility Functions
// ==============================================================================

namespace utils {
    // Asset type utilities
    std::string asset_type_to_string(AssetType type);
    AssetType string_to_asset_type(const std::string& type_str);
    bool is_valid_asset_type(AssetType type) noexcept;

    // Compression utilities
    std::string compression_type_to_string(CompressionType type);
    CompressionType string_to_compression_type(const std::string& type_str);
    bool is_compression_supported(CompressionType type) noexcept;

    // Hash utilities
    std::string hash_type_to_string(HashType type);
    HashType string_to_hash_type(const std::string& type_str);
    bool is_hash_supported(HashType type) noexcept;

    // Error utilities
    std::string error_to_string(LoreError error);
    const char* error_to_cstring(LoreError error) noexcept;

    // Validation utilities
    bool is_valid_asset_name(const std::string& name) noexcept;
    bool is_valid_package_size(std::uint64_t size) noexcept;
    bool is_valid_asset_size(std::uint64_t size) noexcept;

    // Format detection
    Result<AssetType> detect_asset_type_from_extension(const std::filesystem::path& path);
    Result<AssetType> detect_asset_type_from_data(std::span<const std::uint8_t> data);

    // Memory utilities
    std::size_t calculate_memory_usage(const LorePackage& package);
    std::size_t estimate_decompressed_memory(const AssetInfo& info);
}

} // namespace lore::assets