#pragma once

#include <cstring>
#include <sstream>

namespace lore::ecs {

// BinaryArchive template implementations
template<typename T>
BinaryArchive& BinaryArchive::operator<<(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable for binary serialization");

    if (mode_ != Mode::Write) {
        throw std::runtime_error("Archive is not in write mode");
    }

    write_bytes(&value, sizeof(T));
    return *this;
}

template<typename T>
BinaryArchive& BinaryArchive::operator>>(T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable for binary deserialization");

    if (mode_ != Mode::Read) {
        throw std::runtime_error("Archive is not in read mode");
    }

    read_bytes(&value, sizeof(T));
    return *this;
}

template<typename T>
void BinaryArchive::write_vector(const std::vector<T>& vec) {
    static_assert(std::is_trivially_copyable_v<T>, "Vector element type must be trivially copyable");

    std::size_t size = vec.size();
    *this << size;

    if (!vec.empty()) {
        write_bytes(vec.data(), size * sizeof(T));
    }
}

template<typename T>
void BinaryArchive::read_vector(std::vector<T>& vec) {
    static_assert(std::is_trivially_copyable_v<T>, "Vector element type must be trivially copyable");

    std::size_t size;
    *this >> size;

    vec.resize(size);
    if (size > 0) {
        read_bytes(vec.data(), size * sizeof(T));
    }
}

template<typename K, typename V>
void BinaryArchive::write_map(const std::unordered_map<K, V>& map) {
    static_assert(std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>,
                 "Map key and value types must be trivially copyable");

    std::size_t size = map.size();
    *this << size;

    for (const auto& [key, value] : map) {
        *this << key << value;
    }
}

template<typename K, typename V>
void BinaryArchive::read_map(std::unordered_map<K, V>& map) {
    static_assert(std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>,
                 "Map key and value types must be trivially copyable");

    std::size_t size;
    *this >> size;

    map.clear();
    map.reserve(size);

    for (std::size_t i = 0; i < size; ++i) {
        K key;
        V value;
        *this >> key >> value;
        map[key] = value;
    }
}

// JsonArchive template implementations
template<typename T>
void JsonArchive::write_value(const std::string& key, const T& value) {
    if (mode_ != Mode::Write) {
        throw std::runtime_error("Archive is not in write mode");
    }

    std::string full_key = make_key(key);
    std::ostringstream oss;

    if constexpr (std::is_same_v<T, std::string>) {
        oss << "\"" << value << "\"";
    } else if constexpr (std::is_integral_v<T>) {
        oss << value;
    } else if constexpr (std::is_floating_point_v<T>) {
        oss << std::fixed << value;
    } else if constexpr (std::is_same_v<T, bool>) {
        oss << (value ? "true" : "false");
    } else {
        // For complex types, we'd need custom serialization
        static_assert(std::is_trivially_copyable_v<T>, "Complex types need custom JSON serialization");
        oss << "\"<binary_data>\""; // Placeholder
    }

    key_value_pairs_[full_key] = oss.str();
}

template<typename T>
void JsonArchive::read_value(const std::string& key, T& value) {
    if (mode_ != Mode::Read) {
        throw std::runtime_error("Archive is not in read mode");
    }

    std::string full_key = make_key(key);
    auto it = key_value_pairs_.find(full_key);
    if (it == key_value_pairs_.end()) {
        throw std::runtime_error("Key not found: " + full_key);
    }

    std::istringstream iss(it->second);

    if constexpr (std::is_same_v<T, std::string>) {
        std::string quoted_value;
        iss >> quoted_value;
        // Remove quotes
        if (quoted_value.size() >= 2 && quoted_value.front() == '"' && quoted_value.back() == '"') {
            value = quoted_value.substr(1, quoted_value.size() - 2);
        } else {
            value = quoted_value;
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        std::string bool_str;
        iss >> bool_str;
        value = (bool_str == "true");
    } else {
        iss >> value;
    }
}

template<typename T>
void JsonArchive::write_vector(const std::string& key, const std::vector<T>& vec) {
    begin_array(key);

    for (std::size_t i = 0; i < vec.size(); ++i) {
        write_value(std::to_string(i), vec[i]);
    }

    end_array();
}

template<typename T>
void JsonArchive::read_vector(const std::string& key, std::vector<T>& vec) {
    // Simplified implementation - in a real JSON library, this would be more robust
    std::string array_prefix = make_key(key) + ".";

    vec.clear();
    std::size_t index = 0;

    while (true) {
        std::string element_key = array_prefix + std::to_string(index);
        auto it = key_value_pairs_.find(element_key);
        if (it == key_value_pairs_.end()) {
            break;
        }

        T element;
        std::istringstream iss(it->second);
        if constexpr (std::is_same_v<T, std::string>) {
            std::string quoted_value;
            iss >> quoted_value;
            if (quoted_value.size() >= 2 && quoted_value.front() == '"' && quoted_value.back() == '"') {
                element = quoted_value.substr(1, quoted_value.size() - 2);
            } else {
                element = quoted_value;
            }
        } else {
            iss >> element;
        }

        vec.push_back(element);
        ++index;
    }
}

// ComponentSerializerRegistry template implementations
template<typename T>
void ComponentSerializerRegistry::register_component() {
    static_assert(SerializationTraits<T>::is_serializable, "Component must be serializable");

    ComponentID component_id = ComponentRegistry::instance().register_component<T>();

    std::unique_lock<std::shared_mutex> lock(registry_mutex_);

    ComponentSerializationInfo info;

    if constexpr (SerializationTraits<T>::needs_custom_serialization) {
        // Component provides custom serialization
        info.binary_serialize = [](const void* component, BinaryArchive& archive) {
            const T* typed_component = static_cast<const T*>(component);
            const_cast<T*>(typed_component)->serialize(archive);
        };

        info.binary_deserialize = [](void* component, BinaryArchive& archive) {
            T* typed_component = static_cast<T*>(component);
            typed_component->deserialize(archive);
        };

        info.json_serialize = [](const void* component, JsonArchive& archive) {
            const T* typed_component = static_cast<const T*>(component);
            const_cast<T*>(typed_component)->serialize(archive);
        };

        info.json_deserialize = [](void* component, JsonArchive& archive) {
            T* typed_component = static_cast<T*>(component);
            typed_component->deserialize(archive);
        };
    } else {
        // Use default binary serialization for trivially copyable types
        info.binary_serialize = [](const void* component, BinaryArchive& archive) {
            const T* typed_component = static_cast<const T*>(component);
            archive.write_bytes(typed_component, sizeof(T));
        };

        info.binary_deserialize = [](void* component, BinaryArchive& archive) {
            T* typed_component = static_cast<T*>(component);
            archive.read_bytes(typed_component, sizeof(T));
        };

        // Default JSON serialization (simplified)
        info.json_serialize = []([[maybe_unused]] const void* component, JsonArchive& archive) {
            // For non-custom types, we'll serialize as binary data in JSON
            archive.write_value("binary_data", std::string("<binary>"));
            archive.write_value("size", sizeof(T));
        };

        info.json_deserialize = [](void* component, JsonArchive& archive) {
            // Simplified - in practice, we'd need better JSON handling
            std::size_t size;
            archive.read_value("size", size);
            if (size != sizeof(T)) {
                throw std::runtime_error("Size mismatch during JSON deserialization");
            }
            // Initialize with default values
            new(component) T{};
        };
    }

    info.is_registered = true;
    serializers_[component_id] = std::move(info);
}

template<typename T>
void ComponentSerializerRegistry::register_custom_component(SerializeFunc serialize_func,
                                                          DeserializeFunc deserialize_func,
                                                          JsonSerializeFunc json_serialize_func,
                                                          JsonDeserializeFunc json_deserialize_func) {
    ComponentID component_id = ComponentRegistry::instance().register_component<T>();

    std::unique_lock<std::shared_mutex> lock(registry_mutex_);

    ComponentSerializationInfo info;
    info.binary_serialize = std::move(serialize_func);
    info.binary_deserialize = std::move(deserialize_func);
    info.json_serialize = std::move(json_serialize_func);
    info.json_deserialize = std::move(json_deserialize_func);
    info.is_registered = true;

    serializers_[component_id] = std::move(info);
}

} // namespace lore::ecs