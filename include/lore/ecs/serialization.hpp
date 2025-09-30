#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/ecs/entity_manager.hpp>
#include <lore/ecs/world_manager.hpp>
#include <lore/ecs/component_tracking.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>
#include <type_traits>
#include <chrono>

namespace lore::ecs {

// Forward declarations
class WorldSerializer;
class ComponentSerializer;
class BinaryArchive;
class JsonArchive;

// Serialization traits for components
template<typename T>
struct SerializationTraits {
    static constexpr bool is_serializable = std::is_trivially_copyable_v<T>;
    static constexpr bool needs_custom_serialization = false;
};

// Custom serialization interface
template<typename T>
class CustomSerializable {
public:
    virtual ~CustomSerializable() = default;
    virtual void serialize(BinaryArchive& archive) const = 0;
    virtual void deserialize(BinaryArchive& archive) = 0;
    virtual void serialize(JsonArchive& archive) const = 0;
    virtual void deserialize(JsonArchive& archive) = 0;
};

// Binary archive for high-performance serialization
class BinaryArchive {
public:
    enum class Mode { Write, Read };

    explicit BinaryArchive(Mode mode);
    BinaryArchive(Mode mode, std::vector<std::byte>& buffer);
    BinaryArchive(Mode mode, const std::vector<std::byte>& buffer);
    ~BinaryArchive();

    // Basic type serialization
    template<typename T>
    BinaryArchive& operator<<(const T& value);

    template<typename T>
    BinaryArchive& operator>>(T& value);

    // Container serialization
    template<typename T>
    void write_vector(const std::vector<T>& vec);

    template<typename T>
    void read_vector(std::vector<T>& vec);

    template<typename K, typename V>
    void write_map(const std::unordered_map<K, V>& map);

    template<typename K, typename V>
    void read_map(std::unordered_map<K, V>& map);

    // String serialization
    void write_string(const std::string& str);
    void read_string(std::string& str);

    // Raw data
    void write_bytes(const void* data, std::size_t size);
    void read_bytes(void* data, std::size_t size);

    // Archive state
    bool is_writing() const { return mode_ == Mode::Write; }
    bool is_reading() const { return mode_ == Mode::Read; }
    std::size_t get_position() const { return position_; }
    std::size_t get_size() const { return buffer_.size(); }

    // Buffer access
    const std::vector<std::byte>& get_buffer() const { return buffer_; }
    void clear();

    // File I/O
    bool save_to_file(const std::string& filename) const;
    bool load_from_file(const std::string& filename);

private:
    Mode mode_;
    std::vector<std::byte>& buffer_;
    std::vector<std::byte> owned_buffer_;
    std::size_t position_{0};

    void ensure_capacity(std::size_t additional_size);
    void validate_read_size(std::size_t size) const;
};

// JSON archive for human-readable serialization
class JsonArchive {
public:
    enum class Mode { Write, Read };

    explicit JsonArchive(Mode mode);
    JsonArchive(Mode mode, const std::string& json_data);
    ~JsonArchive();

    // Object/array management
    void begin_object(const std::string& key = "");
    void end_object();
    void begin_array(const std::string& key = "");
    void end_array();

    // Value serialization
    template<typename T>
    void write_value(const std::string& key, const T& value);

    template<typename T>
    void read_value(const std::string& key, T& value);

    // Container serialization
    template<typename T>
    void write_vector(const std::string& key, const std::vector<T>& vec);

    template<typename T>
    void read_vector(const std::string& key, std::vector<T>& vec);

    // Archive state
    bool is_writing() const { return mode_ == Mode::Write; }
    bool is_reading() const { return mode_ == Mode::Read; }

    // JSON access
    std::string to_string() const;
    const std::string& get_json_data() const { return json_data_; }
    bool save_to_file(const std::string& filename) const;
    bool load_from_file(const std::string& filename);

private:
    Mode mode_;
    std::string json_data_;

    // JSON parsing/generation would use a library like nlohmann/json
    // For this implementation, we'll use a simplified approach
    std::unordered_map<std::string, std::string> key_value_pairs_;
    std::vector<std::string> object_stack_;
    std::string current_object_prefix_;

    void update_prefix();
    std::string make_key(const std::string& key) const;
    void parse_json_simple();
};

// Component serializer registry
class ComponentSerializerRegistry {
public:
    using SerializeFunc = std::function<void(const void*, BinaryArchive&)>;
    using DeserializeFunc = std::function<void(void*, BinaryArchive&)>;
    using JsonSerializeFunc = std::function<void(const void*, JsonArchive&)>;
    using JsonDeserializeFunc = std::function<void(void*, JsonArchive&)>;

    static ComponentSerializerRegistry& instance();

    // Registration
    template<typename T>
    void register_component();

    template<typename T>
    void register_custom_component(SerializeFunc serialize_func,
                                 DeserializeFunc deserialize_func,
                                 JsonSerializeFunc json_serialize_func,
                                 JsonDeserializeFunc json_deserialize_func);

    // Serialization
    bool serialize_component(ComponentID id, const void* component, BinaryArchive& archive) const;
    bool deserialize_component(ComponentID id, void* component, BinaryArchive& archive) const;
    bool serialize_component_json(ComponentID id, const void* component, JsonArchive& archive) const;
    bool deserialize_component_json(ComponentID id, void* component, JsonArchive& archive) const;

    // Query
    bool is_component_serializable(ComponentID id) const;
    std::vector<ComponentID> get_serializable_components() const;

private:
    struct ComponentSerializationInfo {
        SerializeFunc binary_serialize;
        DeserializeFunc binary_deserialize;
        JsonSerializeFunc json_serialize;
        JsonDeserializeFunc json_deserialize;
        bool is_registered;
    };

    ComponentSerializerRegistry() = default;

    std::unordered_map<ComponentID, ComponentSerializationInfo> serializers_;
    mutable std::shared_mutex registry_mutex_;
};

// World serialization metadata
struct SerializationMetadata {
    std::string version = "1.0";
    std::chrono::system_clock::time_point timestamp;
    std::size_t entity_count = 0;
    std::size_t component_type_count = 0;
    std::vector<ComponentID> serialized_components;
    std::unordered_map<std::string, std::string> custom_data;

    void serialize(BinaryArchive& archive) const;
    void deserialize(BinaryArchive& archive);
    void serialize(JsonArchive& archive) const;
    void deserialize(JsonArchive& archive);
};

// Entity serialization data
struct SerializedEntity {
    Entity id;
    Generation generation;
    std::vector<ComponentID> components;
    std::vector<std::vector<std::byte>> component_data;

    void serialize(BinaryArchive& archive) const;
    void deserialize(BinaryArchive& archive);
    void serialize(JsonArchive& archive) const;
    void deserialize(JsonArchive& archive);
};

// World serializer for complete world state
class WorldSerializer {
public:
    enum class Format { Binary, Json };

    WorldSerializer();
    ~WorldSerializer();

    // Serialization options
    void set_compression_enabled(bool enabled) { compression_enabled_ = enabled; }
    void set_include_destroyed_entities(bool include) { include_destroyed_ = include; }
    void set_component_filter(const std::vector<ComponentID>& components) { component_filter_ = components; }
    void set_custom_metadata(const std::unordered_map<std::string, std::string>& metadata) { custom_metadata_ = metadata; }

    // World serialization
    bool serialize_world(const AdvancedWorld& world, const std::string& filename, Format format = Format::Binary);
    bool deserialize_world(AdvancedWorld& world, const std::string& filename);

    // Streaming serialization for large worlds
    class WorldSerializationStream {
    public:
        WorldSerializationStream(const std::string& filename, Format format);
        ~WorldSerializationStream();

        bool write_metadata(const SerializationMetadata& metadata);
        bool write_entity(const SerializedEntity& entity);
        bool finalize();

    private:
        std::unique_ptr<std::ofstream> file_stream_;
        std::unique_ptr<BinaryArchive> binary_archive_;
        std::unique_ptr<JsonArchive> json_archive_;
        Format format_;
        bool finalized_ = false;
    };

    class WorldDeserializationStream {
    public:
        WorldDeserializationStream(const std::string& filename);
        ~WorldDeserializationStream();

        bool read_metadata(SerializationMetadata& metadata);
        bool read_entity(SerializedEntity& entity);
        bool has_more_entities() const;

    private:
        std::unique_ptr<std::ifstream> file_stream_;
        std::unique_ptr<BinaryArchive> binary_archive_;
        std::unique_ptr<JsonArchive> json_archive_;
        Format format_;
        std::size_t entities_read_ = 0;
        std::size_t total_entities_ = 0;
    };

    std::unique_ptr<WorldSerializationStream> create_serialization_stream(const std::string& filename, Format format = Format::Binary);
    std::unique_ptr<WorldDeserializationStream> create_deserialization_stream(const std::string& filename);

    // Partial serialization
    bool serialize_entities(const std::vector<EntityHandle>& entities, const AdvancedWorld& world, const std::string& filename, Format format = Format::Binary);
    bool deserialize_entities_into_world(AdvancedWorld& world, const std::string& filename);

    // Validation
    bool validate_serialized_file(const std::string& filename) const;
    SerializationMetadata get_file_metadata(const std::string& filename) const;

    // Statistics
    struct SerializationStats {
        std::size_t entities_serialized = 0;
        std::size_t components_serialized = 0;
        std::size_t bytes_written = 0;
        std::chrono::milliseconds serialization_time{0};
        float compression_ratio = 1.0f;
    };

    SerializationStats get_last_serialization_stats() const { return last_stats_; }

private:
    bool compression_enabled_ = true;
    bool include_destroyed_ = false;
    std::vector<ComponentID> component_filter_;
    std::unordered_map<std::string, std::string> custom_metadata_;
    SerializationStats last_stats_;

    bool serialize_world_binary(const AdvancedWorld& world, const std::string& filename);
    bool serialize_world_json(const AdvancedWorld& world, const std::string& filename);
    bool deserialize_world_binary(AdvancedWorld& world, const std::string& filename);
    bool deserialize_world_json(AdvancedWorld& world, const std::string& filename);

    SerializedEntity serialize_entity(EntityHandle entity, const AdvancedWorld& world);
    void deserialize_entity(const SerializedEntity& serialized_entity, AdvancedWorld& world);

    bool should_serialize_component(ComponentID component_id) const;
    std::vector<std::byte> compress_data(const std::vector<std::byte>& data) const;
    std::vector<std::byte> decompress_data(const std::vector<std::byte>& compressed_data) const;
};

// Incremental serialization for streaming worlds
class IncrementalSerializer {
public:
    IncrementalSerializer();
    ~IncrementalSerializer();

    // Change tracking
    void start_tracking(const AdvancedWorld& world);
    void stop_tracking();
    bool is_tracking() const { return tracking_enabled_; }

    // Delta serialization
    bool serialize_changes(const std::string& filename, WorldSerializer::Format format = WorldSerializer::Format::Binary);
    bool apply_changes(AdvancedWorld& world, const std::string& filename);

    // Snapshot management
    bool create_snapshot(const AdvancedWorld& world, const std::string& filename);
    bool restore_from_snapshot(AdvancedWorld& world, const std::string& filename);

    // Configuration
    void set_change_buffer_size(std::size_t size) { max_changes_ = size; }
    void set_auto_snapshot_interval(std::chrono::minutes interval) { snapshot_interval_ = interval; }

private:
    struct EntityChange {
        EntityHandle entity;
        enum class Type { Created, Modified, Destroyed } type;
        std::vector<ComponentID> changed_components;
        std::chrono::steady_clock::time_point timestamp;
    };

    bool tracking_enabled_ = false;
    std::vector<EntityChange> pending_changes_;
    std::size_t max_changes_ = 10000;
    std::chrono::minutes snapshot_interval_{60};
    std::chrono::steady_clock::time_point last_snapshot_;

    std::shared_ptr<ComponentChangeTracker> change_tracker_;
    std::size_t change_callback_id_ = 0;

    void on_component_change(EntityHandle entity, ComponentID component_id, ComponentChangeTracker::ChangeRecord::Type type);
    void cleanup_old_changes();
};

// Performance profiling for serialization
class SerializationProfiler {
public:
    struct ComponentProfile {
        ComponentID component_id;
        std::size_t serialization_count = 0;
        std::chrono::microseconds total_serialization_time{0};
        std::chrono::microseconds average_serialization_time{0};
        std::size_t total_bytes_serialized = 0;
        float average_bytes_per_component = 0.0f;
    };

    static SerializationProfiler& instance();

    void start_profiling_component(ComponentID component_id);
    void end_profiling_component(ComponentID component_id, std::size_t bytes_serialized);

    ComponentProfile get_component_profile(ComponentID component_id) const;
    std::vector<ComponentProfile> get_all_profiles() const;

    void reset_profiles();
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

private:
    SerializationProfiler() = default;

    bool profiling_enabled_ = false;
    std::unordered_map<ComponentID, ComponentProfile> profiles_;
    std::unordered_map<ComponentID, std::chrono::steady_clock::time_point> start_times_;
    mutable std::shared_mutex profiler_mutex_;
};

// Utility macros for easy component serialization registration
#define LORE_REGISTER_SERIALIZABLE_COMPONENT(ComponentType) \
    ComponentSerializerRegistry::instance().register_component<ComponentType>()

#define LORE_REGISTER_CUSTOM_SERIALIZABLE_COMPONENT(ComponentType, SerializeFunc, DeserializeFunc, JsonSerializeFunc, JsonDeserializeFunc) \
    ComponentSerializerRegistry::instance().register_custom_component<ComponentType>(SerializeFunc, DeserializeFunc, JsonSerializeFunc, JsonDeserializeFunc)

} // namespace lore::ecs

#include "serialization.inl"