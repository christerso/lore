#include <lore/ecs/serialization.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace lore::ecs {

// BinaryArchive implementation
BinaryArchive::BinaryArchive(Mode mode)
    : mode_(mode), buffer_(owned_buffer_) {}

BinaryArchive::BinaryArchive(Mode mode, std::vector<std::byte>& buffer)
    : mode_(mode), buffer_(buffer) {}

BinaryArchive::BinaryArchive(Mode mode, const std::vector<std::byte>& buffer)
    : mode_(mode), buffer_(const_cast<std::vector<std::byte>&>(buffer)) {
    if (mode == Mode::Write) {
        throw std::invalid_argument("Cannot write to const buffer");
    }
}

BinaryArchive::~BinaryArchive() = default;

void BinaryArchive::write_string(const std::string& str) {
    std::size_t size = str.size();
    *this << size;
    if (size > 0) {
        write_bytes(str.data(), size);
    }
}

void BinaryArchive::read_string(std::string& str) {
    std::size_t size;
    *this >> size;

    str.resize(size);
    if (size > 0) {
        read_bytes(str.data(), size);
    }
}

void BinaryArchive::write_bytes(const void* data, std::size_t size) {
    if (mode_ != Mode::Write) {
        throw std::runtime_error("Archive is not in write mode");
    }

    ensure_capacity(size);

    const std::byte* byte_data = static_cast<const std::byte*>(data);
    std::copy(byte_data, byte_data + size, buffer_.begin() + position_);
    position_ += size;
}

void BinaryArchive::read_bytes(void* data, std::size_t size) {
    if (mode_ != Mode::Read) {
        throw std::runtime_error("Archive is not in read mode");
    }

    validate_read_size(size);

    std::byte* byte_data = static_cast<std::byte*>(data);
    std::copy(buffer_.begin() + position_, buffer_.begin() + position_ + size, byte_data);
    position_ += size;
}

void BinaryArchive::clear() {
    buffer_.clear();
    position_ = 0;
}

bool BinaryArchive::save_to_file(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(buffer_.data()), buffer_.size());
    return file.good();
}

bool BinaryArchive::load_from_file(const std::string& filename) {
    if (mode_ != Mode::Read) {
        throw std::runtime_error("Archive must be in read mode to load from file");
    }

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer_.resize(file_size);
    file.read(reinterpret_cast<char*>(buffer_.data()), file_size);

    position_ = 0;
    return file.good();
}

void BinaryArchive::ensure_capacity(std::size_t additional_size) {
    std::size_t required_size = position_ + additional_size;
    if (buffer_.size() < required_size) {
        buffer_.resize(required_size * 2); // Grow by 2x for amortized performance
    }
}

void BinaryArchive::validate_read_size(std::size_t size) const {
    if (position_ + size > buffer_.size()) {
        throw std::runtime_error("Attempt to read beyond buffer bounds");
    }
}

// JsonArchive implementation
JsonArchive::JsonArchive(Mode mode) : mode_(mode) {}

JsonArchive::JsonArchive(Mode mode, const std::string& json_data)
    : mode_(mode), json_data_(json_data) {
    if (mode == Mode::Read) {
        // Parse JSON data into key-value pairs
        // This is a simplified implementation - a real version would use a proper JSON parser
        parse_json_simple();
    }
}

JsonArchive::~JsonArchive() = default;

void JsonArchive::begin_object(const std::string& key) {
    if (!key.empty()) {
        object_stack_.push_back(key);
        update_prefix();
    }
}

void JsonArchive::end_object() {
    if (!object_stack_.empty()) {
        object_stack_.pop_back();
        update_prefix();
    }
}

void JsonArchive::begin_array(const std::string& key) {
    // Arrays are handled similarly to objects in this simplified implementation
    begin_object(key);
}

void JsonArchive::end_array() {
    end_object();
}

std::string JsonArchive::to_string() const {
    if (mode_ != Mode::Write) {
        return json_data_;
    }

    // Convert key-value pairs back to JSON
    // This is a simplified implementation
    std::ostringstream oss;
    oss << "{\n";

    bool first = true;
    for (const auto& [key, value] : key_value_pairs_) {
        if (!first) {
            oss << ",\n";
        }
        oss << "  \"" << key << "\": " << value;
        first = false;
    }

    oss << "\n}";
    return oss.str();
}

bool JsonArchive::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << to_string();
    return file.good();
}

bool JsonArchive::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    json_data_ = buffer.str();

    if (mode_ == Mode::Read) {
        parse_json_simple();
    }

    return true;
}

void JsonArchive::update_prefix() {
    current_object_prefix_.clear();
    for (const auto& obj : object_stack_) {
        if (!current_object_prefix_.empty()) {
            current_object_prefix_ += ".";
        }
        current_object_prefix_ += obj;
    }
}

std::string JsonArchive::make_key(const std::string& key) const {
    if (current_object_prefix_.empty()) {
        return key;
    }
    return current_object_prefix_ + "." + key;
}

void JsonArchive::parse_json_simple() {
    // Simplified JSON parsing - in practice, use a proper JSON library
    key_value_pairs_.clear();

    // This is a placeholder implementation
    // A real implementation would properly parse JSON structure
    std::istringstream iss(json_data_);
    std::string line;

    while (std::getline(iss, line)) {
        // Look for key-value pairs
        std::size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Remove quotes and whitespace
            key.erase(std::remove_if(key.begin(), key.end(), [](char c) {
                return c == '"' || c == ' ' || c == '\t';
            }), key.end());

            value.erase(0, value.find_first_not_of(" \t"));
            if (value.back() == ',') {
                value.pop_back();
            }

            key_value_pairs_[key] = value;
        }
    }
}

// ComponentSerializerRegistry implementation
ComponentSerializerRegistry& ComponentSerializerRegistry::instance() {
    static ComponentSerializerRegistry instance;
    return instance;
}

bool ComponentSerializerRegistry::serialize_component(ComponentID id, const void* component, BinaryArchive& archive) const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = serializers_.find(id);
    if (it == serializers_.end() || !it->second.is_registered) {
        return false;
    }

    try {
        it->second.binary_serialize(component, archive);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ComponentSerializerRegistry::deserialize_component(ComponentID id, void* component, BinaryArchive& archive) const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = serializers_.find(id);
    if (it == serializers_.end() || !it->second.is_registered) {
        return false;
    }

    try {
        it->second.binary_deserialize(component, archive);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ComponentSerializerRegistry::serialize_component_json(ComponentID id, const void* component, JsonArchive& archive) const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = serializers_.find(id);
    if (it == serializers_.end() || !it->second.is_registered) {
        return false;
    }

    try {
        it->second.json_serialize(component, archive);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ComponentSerializerRegistry::deserialize_component_json(ComponentID id, void* component, JsonArchive& archive) const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = serializers_.find(id);
    if (it == serializers_.end() || !it->second.is_registered) {
        return false;
    }

    try {
        it->second.json_deserialize(component, archive);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ComponentSerializerRegistry::is_component_serializable(ComponentID id) const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = serializers_.find(id);
    return it != serializers_.end() && it->second.is_registered;
}

std::vector<ComponentID> ComponentSerializerRegistry::get_serializable_components() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    std::vector<ComponentID> serializable;
    for (const auto& [id, info] : serializers_) {
        if (info.is_registered) {
            serializable.push_back(id);
        }
    }

    return serializable;
}

// SerializationMetadata implementation
void SerializationMetadata::serialize(BinaryArchive& archive) const {
    archive.write_string(version);

    auto time_since_epoch = timestamp.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time_since_epoch).count();
    archive << microseconds;

    archive << entity_count << component_type_count;
    archive.write_vector(serialized_components);

    std::size_t custom_data_size = custom_data.size();
    archive << custom_data_size;
    for (const auto& [key, value] : custom_data) {
        archive.write_string(key);
        archive.write_string(value);
    }
}

void SerializationMetadata::deserialize(BinaryArchive& archive) {
    archive.read_string(version);

    int64_t microseconds;
    archive >> microseconds;
    timestamp = std::chrono::system_clock::time_point(std::chrono::microseconds(microseconds));

    archive >> entity_count >> component_type_count;
    archive.read_vector(serialized_components);

    std::size_t custom_data_size;
    archive >> custom_data_size;

    custom_data.clear();
    for (std::size_t i = 0; i < custom_data_size; ++i) {
        std::string key, value;
        archive.read_string(key);
        archive.read_string(value);
        custom_data[key] = value;
    }
}

void SerializationMetadata::serialize(JsonArchive& archive) const {
    archive.write_value("version", version);

    auto time_since_epoch = timestamp.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time_since_epoch).count();
    archive.write_value("timestamp", microseconds);

    archive.write_value("entity_count", entity_count);
    archive.write_value("component_type_count", component_type_count);
    archive.write_vector("serialized_components", serialized_components);

    archive.begin_object("custom_data");
    for (const auto& [key, value] : custom_data) {
        archive.write_value(key, value);
    }
    archive.end_object();
}

void SerializationMetadata::deserialize(JsonArchive& archive) {
    archive.read_value("version", version);

    int64_t microseconds;
    archive.read_value("timestamp", microseconds);
    timestamp = std::chrono::system_clock::time_point(std::chrono::microseconds(microseconds));

    archive.read_value("entity_count", entity_count);
    archive.read_value("component_type_count", component_type_count);
    archive.read_vector("serialized_components", serialized_components);

    // Custom data would need more sophisticated JSON parsing
    custom_data.clear();
}

// SerializedEntity implementation
void SerializedEntity::serialize(BinaryArchive& archive) const {
    archive << id << generation;
    archive.write_vector(components);

    std::size_t data_count = component_data.size();
    archive << data_count;
    for (const auto& data : component_data) {
        archive.write_vector(data);
    }
}

void SerializedEntity::deserialize(BinaryArchive& archive) {
    archive >> id >> generation;
    archive.read_vector(components);

    std::size_t data_count;
    archive >> data_count;

    component_data.resize(data_count);
    for (auto& data : component_data) {
        archive.read_vector(data);
    }
}

void SerializedEntity::serialize(JsonArchive& archive) const {
    archive.write_value("id", id);
    archive.write_value("generation", generation);
    archive.write_vector("components", components);

    archive.begin_array("component_data");
    for (std::size_t i = 0; i < component_data.size(); ++i) {
        // Binary data in JSON would need base64 encoding
        archive.write_value(std::to_string(i) + "_size", component_data[i].size());
    }
    archive.end_array();
}

void SerializedEntity::deserialize(JsonArchive& archive) {
    archive.read_value("id", id);
    archive.read_value("generation", generation);
    archive.read_vector("components", components);

    // Component data deserialization would need proper JSON handling
    component_data.clear();
}

// WorldSerializer implementation
WorldSerializer::WorldSerializer() = default;
WorldSerializer::~WorldSerializer() = default;

bool WorldSerializer::serialize_world(const AdvancedWorld& world, const std::string& filename, Format format) {
    auto start_time = std::chrono::high_resolution_clock::now();

    bool success = false;
    switch (format) {
        case Format::Binary:
            success = serialize_world_binary(world, filename);
            break;
        case Format::Json:
            success = serialize_world_json(world, filename);
            break;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    last_stats_.serialization_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return success;
}

bool WorldSerializer::deserialize_world(AdvancedWorld& world, const std::string& filename) {
    // Detect format from file extension or header
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Simple format detection
    std::string first_chars(4, '\0');
    file.read(first_chars.data(), 4);
    file.close();

    if (first_chars[0] == '{' || first_chars.find("json") != std::string::npos) {
        return deserialize_world_json(world, filename);
    } else {
        return deserialize_world_binary(world, filename);
    }
}

bool WorldSerializer::serialize_entities(const std::vector<EntityHandle>& entities, const AdvancedWorld& world, const std::string& filename, Format format) {
    auto stream = create_serialization_stream(filename, format);
    if (!stream) {
        return false;
    }

    // Create metadata
    SerializationMetadata metadata;
    metadata.timestamp = std::chrono::system_clock::now();
    metadata.entity_count = entities.size();
    metadata.component_type_count = world.get_component_type_count();
    metadata.custom_data = custom_metadata_;

    if (!stream->write_metadata(metadata)) {
        return false;
    }

    // Serialize entities
    for (EntityHandle entity : entities) {
        if (world.is_valid(entity)) {
            SerializedEntity serialized = serialize_entity(entity, world);
            if (!stream->write_entity(serialized)) {
                return false;
            }
        }
    }

    return stream->finalize();
}

bool WorldSerializer::deserialize_entities_into_world(AdvancedWorld& world, const std::string& filename) {
    auto stream = create_deserialization_stream(filename);
    if (!stream) {
        return false;
    }

    SerializationMetadata metadata;
    if (!stream->read_metadata(metadata)) {
        return false;
    }

    while (stream->has_more_entities()) {
        SerializedEntity entity;
        if (!stream->read_entity(entity)) {
            break;
        }

        deserialize_entity(entity, world);
    }

    return true;
}

bool WorldSerializer::validate_serialized_file(const std::string& filename) const {
    try {
        auto stream = const_cast<WorldSerializer*>(this)->create_deserialization_stream(filename);
        if (!stream) {
            return false;
        }

        SerializationMetadata metadata;
        return stream->read_metadata(metadata);
    } catch (const std::exception&) {
        return false;
    }
}

SerializationMetadata WorldSerializer::get_file_metadata(const std::string& filename) const {
    SerializationMetadata metadata;

    try {
        auto stream = const_cast<WorldSerializer*>(this)->create_deserialization_stream(filename);
        if (stream) {
            stream->read_metadata(metadata);
        }
    } catch (const std::exception&) {
        // Return empty metadata on error
    }

    return metadata;
}

bool WorldSerializer::serialize_world_binary(const AdvancedWorld& world, const std::string& filename) {
    BinaryArchive archive(BinaryArchive::Mode::Write);

    // Serialize metadata
    SerializationMetadata metadata;
    metadata.timestamp = std::chrono::system_clock::now();
    metadata.entity_count = world.get_entity_count();
    metadata.component_type_count = world.get_component_type_count();
    metadata.custom_data = custom_metadata_;

    auto& registry = ComponentSerializerRegistry::instance();
    metadata.serialized_components = registry.get_serializable_components();

    metadata.serialize(archive);

    // Serialize entities
    last_stats_.entities_serialized = 0;
    last_stats_.components_serialized = 0;

    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;
        if (!world.is_valid(entity)) {
            continue;
        }

        SerializedEntity serialized_entity = serialize_entity(entity, world);
        serialized_entity.serialize(archive);

        last_stats_.entities_serialized++;
        last_stats_.components_serialized += serialized_entity.components.size();
    }

    last_stats_.bytes_written = archive.get_size();

    // Apply compression if enabled
    std::vector<std::byte> final_data = archive.get_buffer();
    if (compression_enabled_) {
        auto compressed_data = compress_data(final_data);
        last_stats_.compression_ratio = static_cast<float>(compressed_data.size()) / final_data.size();
        final_data = std::move(compressed_data);
    } else {
        last_stats_.compression_ratio = 1.0f;
    }

    // Write to file
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(final_data.data()), final_data.size());
    return file.good();
}

bool WorldSerializer::serialize_world_json(const AdvancedWorld& world, const std::string& filename) {
    JsonArchive archive(JsonArchive::Mode::Write);

    // Serialize metadata
    SerializationMetadata metadata;
    metadata.timestamp = std::chrono::system_clock::now();
    metadata.entity_count = world.get_entity_count();
    metadata.component_type_count = world.get_component_type_count();
    metadata.custom_data = custom_metadata_;

    auto& registry = ComponentSerializerRegistry::instance();
    metadata.serialized_components = registry.get_serializable_components();

    archive.begin_object("metadata");
    metadata.serialize(archive);
    archive.end_object();

    // Serialize entities
    archive.begin_array("entities");

    std::size_t entity_index = 0;
    for (auto it = world.entity_manager_->begin(); it != world.entity_manager_->end(); ++it) {
        EntityHandle entity = *it;
        if (!world.is_valid(entity)) {
            continue;
        }

        archive.begin_object(std::to_string(entity_index));
        SerializedEntity serialized_entity = serialize_entity(entity, world);
        serialized_entity.serialize(archive);
        archive.end_object();

        entity_index++;
    }

    archive.end_array();

    return archive.save_to_file(filename);
}

bool WorldSerializer::deserialize_world_binary(AdvancedWorld& world, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> file_data(file_size);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);

    // Decompress if needed
    if (compression_enabled_) {
        file_data = decompress_data(file_data);
    }

    BinaryArchive archive(BinaryArchive::Mode::Read, file_data);

    // Deserialize metadata
    SerializationMetadata metadata;
    metadata.deserialize(archive);

    // Deserialize entities
    for (std::size_t i = 0; i < metadata.entity_count; ++i) {
        SerializedEntity entity;
        entity.deserialize(archive);
        deserialize_entity(entity, world);
    }

    return true;
}

bool WorldSerializer::deserialize_world_json([[maybe_unused]] AdvancedWorld& world, const std::string& filename) {
    JsonArchive archive(JsonArchive::Mode::Read);
    if (!archive.load_from_file(filename)) {
        return false;
    }

    // Deserialize metadata
    SerializationMetadata metadata;
    metadata.deserialize(archive);

    // Deserialize entities would require more sophisticated JSON handling
    // This is a simplified placeholder

    return true;
}

SerializedEntity WorldSerializer::serialize_entity(EntityHandle entity, [[maybe_unused]] const AdvancedWorld& world) {
    SerializedEntity serialized;
    serialized.id = entity.id;
    serialized.generation = entity.generation;

    [[maybe_unused]] auto& registry = ComponentSerializerRegistry::instance();
    auto serializable_components = registry.get_serializable_components();

    for (ComponentID component_id : serializable_components) {
        if (should_serialize_component(component_id)) {
            // Check if entity has this component
            // In a complete implementation, we'd need a way to check this generically
            // For now, we'll assume all serializable components are present
            serialized.components.push_back(component_id);

            // Serialize component data
            BinaryArchive component_archive(BinaryArchive::Mode::Write);
            // We'd need to get the component data from the world
            // This is a placeholder
            std::vector<std::byte> component_data;
            serialized.component_data.push_back(std::move(component_data));
        }
    }

    return serialized;
}

void WorldSerializer::deserialize_entity(const SerializedEntity& serialized_entity, AdvancedWorld& world) {
    // Create entity with specific ID if possible
    EntityHandle entity = world.create_entity();

    [[maybe_unused]] auto& registry = ComponentSerializerRegistry::instance();

    for (std::size_t i = 0; i < serialized_entity.components.size(); ++i) {
        [[maybe_unused]] ComponentID component_id = serialized_entity.components[i];
        const auto& component_data = serialized_entity.component_data[i];

        // Deserialize and add component to entity
        BinaryArchive component_archive(BinaryArchive::Mode::Read, component_data);
        // We'd need to create the component and add it to the entity
        // This is a placeholder for the actual implementation
    }
}

bool WorldSerializer::should_serialize_component(ComponentID component_id) const {
    if (!component_filter_.empty()) {
        return std::find(component_filter_.begin(), component_filter_.end(), component_id) != component_filter_.end();
    }
    return true;
}

std::vector<std::byte> WorldSerializer::compress_data(const std::vector<std::byte>& data) const {
    // Placeholder for compression implementation
    // In practice, this would use a compression library like zlib or lz4
    return data;
}

std::vector<std::byte> WorldSerializer::decompress_data(const std::vector<std::byte>& compressed_data) const {
    // Placeholder for decompression implementation
    return compressed_data;
}

// WorldSerializer stream implementations would go here
// Simplified for brevity

// SerializationProfiler implementation
SerializationProfiler& SerializationProfiler::instance() {
    static SerializationProfiler instance;
    return instance;
}

void SerializationProfiler::start_profiling_component(ComponentID component_id) {
    if (!profiling_enabled_) return;

    std::unique_lock<std::shared_mutex> lock(profiler_mutex_);
    start_times_[component_id] = std::chrono::steady_clock::now();
}

void SerializationProfiler::end_profiling_component(ComponentID component_id, std::size_t bytes_serialized) {
    if (!profiling_enabled_) return;

    auto end_time = std::chrono::steady_clock::now();

    std::unique_lock<std::shared_mutex> lock(profiler_mutex_);

    auto start_it = start_times_.find(component_id);
    if (start_it == start_times_.end()) {
        return;
    }

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_it->second);
    start_times_.erase(start_it);

    auto& profile = profiles_[component_id];
    profile.component_id = component_id;
    profile.serialization_count++;
    profile.total_serialization_time += duration;
    profile.total_bytes_serialized += bytes_serialized;

    // Update averages
    profile.average_serialization_time = profile.total_serialization_time / profile.serialization_count;
    profile.average_bytes_per_component = static_cast<float>(profile.total_bytes_serialized) / profile.serialization_count;
}

SerializationProfiler::ComponentProfile SerializationProfiler::get_component_profile(ComponentID component_id) const {
    std::shared_lock<std::shared_mutex> lock(profiler_mutex_);

    auto it = profiles_.find(component_id);
    if (it != profiles_.end()) {
        return it->second;
    }

    return ComponentProfile{component_id};
}

std::vector<SerializationProfiler::ComponentProfile> SerializationProfiler::get_all_profiles() const {
    std::shared_lock<std::shared_mutex> lock(profiler_mutex_);

    std::vector<ComponentProfile> all_profiles;
    for (const auto& [id, profile] : profiles_) {
        all_profiles.push_back(profile);
    }

    return all_profiles;
}

void SerializationProfiler::reset_profiles() {
    std::unique_lock<std::shared_mutex> lock(profiler_mutex_);
    profiles_.clear();
    start_times_.clear();
}

} // namespace lore::ecs