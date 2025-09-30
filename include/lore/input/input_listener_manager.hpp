#pragma once

#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <bitset>

namespace lore::input {

// Forward declarations
class ListenerGroup;

// Listener creation parameters
struct ListenerConfig {
    EventPriority priority = EventPriority::Normal;
    std::string name;           // Optional name for debugging
    std::string group;          // Optional group name for batch management
    bool auto_remove = true;    // Remove when handle goes out of scope
    std::size_t max_invocations = 0;  // 0 = unlimited
    std::chrono::milliseconds timeout{0};  // 0 = no timeout
};

// Enhanced listener handle with additional management features
class ManagedListenerHandle {
public:
    ManagedListenerHandle() = default;
    explicit ManagedListenerHandle(ListenerHandle handle, const ListenerConfig& config = {});
    ~ManagedListenerHandle();

    // Non-copyable, movable
    ManagedListenerHandle(const ManagedListenerHandle&) = delete;
    ManagedListenerHandle& operator=(const ManagedListenerHandle&) = delete;
    ManagedListenerHandle(ManagedListenerHandle&& other) noexcept;
    ManagedListenerHandle& operator=(ManagedListenerHandle&& other) noexcept;

    // Connection management
    void disconnect();
    bool is_connected() const;

    // Configuration
    const ListenerConfig& get_config() const { return config_; }
    void set_name(const std::string& name) { config_.name = name; }
    const std::string& get_name() const { return config_.name; }

    // Statistics
    std::size_t get_invocation_count() const { return invocation_count_.load(); }
    std::chrono::high_resolution_clock::time_point get_creation_time() const { return creation_time_; }
    std::chrono::high_resolution_clock::time_point get_last_invocation_time() const { return last_invocation_time_; }

    // Internal use
    void increment_invocation_count() { ++invocation_count_; }
    void update_last_invocation_time() { last_invocation_time_ = std::chrono::high_resolution_clock::now(); }
    bool should_auto_remove() const;

private:
    ListenerHandle handle_;
    ListenerConfig config_;
    std::atomic<std::size_t> invocation_count_{0};
    std::chrono::high_resolution_clock::time_point creation_time_;
    std::chrono::high_resolution_clock::time_point last_invocation_time_;
};

// Listener group for batch management
class ListenerGroup {
public:
    explicit ListenerGroup(std::string name);
    ~ListenerGroup();

    // Non-copyable, movable
    ListenerGroup(const ListenerGroup&) = delete;
    ListenerGroup& operator=(const ListenerGroup&) = delete;
    ListenerGroup(ListenerGroup&&) = default;
    ListenerGroup& operator=(ListenerGroup&&) = default;

    // Group management
    const std::string& get_name() const { return name_; }
    std::size_t size() const;
    bool empty() const;

    // Listener operations
    void add_listener(std::shared_ptr<ManagedListenerHandle> handle);
    void remove_listener(const std::shared_ptr<ManagedListenerHandle>& handle);
    void clear();

    // Group operations
    void disconnect_all();
    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_.load(); }

    // Priority management
    void set_group_priority(EventPriority priority);
    EventPriority get_group_priority() const { return group_priority_; }

    // Statistics
    std::size_t get_total_invocations() const;
    std::vector<std::shared_ptr<ManagedListenerHandle>> get_listeners() const;

private:
    std::string name_;
    mutable std::mutex mutex_;
    mutable std::vector<std::weak_ptr<ManagedListenerHandle>> listeners_;
    std::atomic<bool> enabled_{true};
    EventPriority group_priority_{EventPriority::Normal};

    void cleanup_expired_listeners() const;
};

// Conditional listener wrapper
template<typename EventType>
class ConditionalListener {
public:
    using HandlerFunction = std::function<void(const EventType&)>;
    using ConditionFunction = std::function<bool(const EventType&)>;

    ConditionalListener(HandlerFunction handler, ConditionFunction condition)
        : handler_(std::move(handler)), condition_(std::move(condition)) {}

    void operator()(const EventType& event) {
        if (condition_(event)) {
            handler_(event);
        }
    }

private:
    HandlerFunction handler_;
    ConditionFunction condition_;
};

// Timed listener wrapper
template<typename EventType>
class TimedListener {
public:
    using HandlerFunction = std::function<void(const EventType&)>;

    TimedListener(HandlerFunction handler, std::chrono::milliseconds duration)
        : handler_(std::move(handler))
        , expiry_time_(std::chrono::high_resolution_clock::now() + duration) {}

    bool operator()(const EventType& event) {
        if (std::chrono::high_resolution_clock::now() > expiry_time_) {
            return false;  // Expired
        }
        handler_(event);
        return true;
    }

    bool is_expired() const {
        return std::chrono::high_resolution_clock::now() > expiry_time_;
    }

private:
    HandlerFunction handler_;
    std::chrono::high_resolution_clock::time_point expiry_time_;
};

// Input-specific listener manager
class InputListenerManager {
public:
    InputListenerManager(EventDispatcher& dispatcher);
    ~InputListenerManager();

    // Non-copyable, movable
    InputListenerManager(const InputListenerManager&) = delete;
    InputListenerManager& operator=(const InputListenerManager&) = delete;
    InputListenerManager(InputListenerManager&&) = default;
    InputListenerManager& operator=(InputListenerManager&&) = default;

    // Basic listener registration
    template<typename EventType>
    ManagedListenerHandle subscribe(std::function<void(const EventType&)> handler,
                                   const ListenerConfig& config = {});

    // Conditional listeners
    template<typename EventType>
    ManagedListenerHandle subscribe_conditional(std::function<void(const EventType&)> handler,
                                               std::function<bool(const EventType&)> condition,
                                               const ListenerConfig& config = {});

    // Timed listeners
    template<typename EventType>
    ManagedListenerHandle subscribe_timed(std::function<void(const EventType&)> handler,
                                         std::chrono::milliseconds duration,
                                         const ListenerConfig& config = {});

    // One-shot listeners
    template<typename EventType>
    ManagedListenerHandle subscribe_once(std::function<void(const EventType&)> handler,
                                        const ListenerConfig& config = {});

    // Group management
    std::shared_ptr<ListenerGroup> create_group(const std::string& name);
    std::shared_ptr<ListenerGroup> get_group(const std::string& name);
    void remove_group(const std::string& name);
    std::vector<std::string> get_group_names() const;

    // Batch operations
    template<typename EventType>
    std::vector<ManagedListenerHandle> subscribe_to_group(const std::string& group_name,
                                                         std::vector<std::function<void(const EventType&)>> handlers,
                                                         const ListenerConfig& base_config = {});

    void disconnect_group(const std::string& group_name);
    void disconnect_all();

    // Input-specific convenience methods
    ManagedListenerHandle on_key_pressed(KeyCode key, std::function<void()> handler,
                                        const ListenerConfig& config = {});
    ManagedListenerHandle on_key_released(KeyCode key, std::function<void()> handler,
                                         const ListenerConfig& config = {});
    ManagedListenerHandle on_mouse_clicked(MouseButton button, std::function<void(glm::vec2)> handler,
                                          const ListenerConfig& config = {});
    ManagedListenerHandle on_gamepad_button(std::uint32_t gamepad_id, GamepadButton button,
                                           std::function<void()> handler,
                                           const ListenerConfig& config = {});

    // Key combination listeners
    ManagedListenerHandle on_key_combination(std::vector<KeyCode> keys,
                                            std::function<void()> handler,
                                            const ListenerConfig& config = {});

    // Action-based listeners
    ManagedListenerHandle on_input_action(InputAction action,
                                         std::function<void(float)> handler,
                                         const ListenerConfig& config = {});

    // State tracking
    void enable_state_tracking(bool enabled) { state_tracking_enabled_ = enabled; }
    bool is_state_tracking_enabled() const { return state_tracking_enabled_; }

    // Cleanup and maintenance
    void cleanup_expired_listeners();
    void cleanup_unused_groups();

    // Statistics and debugging
    struct Statistics {
        std::size_t total_listeners = 0;
        std::size_t active_listeners = 0;
        std::size_t total_groups = 0;
        std::size_t active_groups = 0;
        std::size_t total_invocations = 0;
    };

    Statistics get_statistics() const;
    void reset_statistics();

    // Debug information
    std::vector<std::string> get_listener_names() const;
    void print_listener_summary() const;

private:
    EventDispatcher& event_dispatcher_;
    mutable std::mutex groups_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ListenerGroup>> groups_;
    mutable std::mutex listeners_mutex_;
    std::vector<std::weak_ptr<ManagedListenerHandle>> managed_listeners_;

    bool state_tracking_enabled_ = false;
    mutable std::atomic<std::size_t> total_invocations_{0};

    // Internal helpers
    std::string generate_listener_name(const std::string& event_type, const ListenerConfig& config);
    void register_managed_listener(std::shared_ptr<ManagedListenerHandle> handle);
    void update_listener_statistics(const ManagedListenerHandle& handle);

    // Key combination state tracking
    struct KeyCombinationTracker {
        std::vector<KeyCode> required_keys;
        std::bitset<512> current_keys;
        std::function<void()> handler;
        bool all_keys_pressed() const;
    };
    std::vector<KeyCombinationTracker> key_combinations_;
    void update_key_combination_state(KeyCode key, bool pressed);
};

// RAII listener scope guard
template<typename EventType>
class ScopedInputListener {
public:
    ScopedInputListener(InputListenerManager& manager,
                       std::function<void(const EventType&)> handler,
                       const ListenerConfig& config = {})
        : handle_(manager.subscribe<EventType>(std::move(handler), config)) {}

    ~ScopedInputListener() = default;

    // Non-copyable, movable
    ScopedInputListener(const ScopedInputListener&) = delete;
    ScopedInputListener& operator=(const ScopedInputListener&) = delete;
    ScopedInputListener(ScopedInputListener&&) = default;
    ScopedInputListener& operator=(ScopedInputListener&&) = default;

    void disconnect() { handle_.disconnect(); }
    bool is_connected() const { return handle_.is_connected(); }
    const ListenerConfig& get_config() const { return handle_.get_config(); }

private:
    ManagedListenerHandle handle_;
};

// Predefined listener configurations
namespace listener_configs {
    extern const ListenerConfig HIGH_PRIORITY;
    extern const ListenerConfig LOW_PRIORITY;
    extern const ListenerConfig UI_LISTENER;
    extern const ListenerConfig GAMEPLAY_LISTENER;
    extern const ListenerConfig DEBUG_LISTENER;
    extern const ListenerConfig ONE_SHOT;
    extern const ListenerConfig TEMPORARY;
}

// Template implementations

template<typename EventType>
ManagedListenerHandle InputListenerManager::subscribe(std::function<void(const EventType&)> handler,
                                                      const ListenerConfig& config) {
    // Wrap handler to update statistics
    auto wrapped_handler = [this, original_handler = std::move(handler)]
                          (const EventType& event) mutable {
        original_handler(event);
        total_invocations_.fetch_add(1);
    };

    auto listener_handle = event_dispatcher_.subscribe<EventType>(std::move(wrapped_handler), config.priority);
    auto managed_config = config;
    managed_config.name = managed_config.name.empty() ?
        generate_listener_name(typeid(EventType).name(), config) : managed_config.name;

    auto managed_handle = std::make_shared<ManagedListenerHandle>(std::move(listener_handle), managed_config);
    register_managed_listener(managed_handle);

    // Add to group if specified
    if (!config.group.empty()) {
        auto group = get_group(config.group);
        if (!group) {
            group = create_group(config.group);
        }
        group->add_listener(managed_handle);
    }

    return ManagedListenerHandle(std::move(*managed_handle));
}

template<typename EventType>
ManagedListenerHandle InputListenerManager::subscribe_conditional(std::function<void(const EventType&)> handler,
                                                                  std::function<bool(const EventType&)> condition,
                                                                  const ListenerConfig& config) {
    auto conditional_handler = ConditionalListener<EventType>(std::move(handler), std::move(condition));
    return subscribe<EventType>(std::move(conditional_handler), config);
}

template<typename EventType>
ManagedListenerHandle InputListenerManager::subscribe_timed(std::function<void(const EventType&)> handler,
                                                           std::chrono::milliseconds duration,
                                                           const ListenerConfig& config) {
    auto timed_handler = std::make_shared<TimedListener<EventType> >(std::move(handler), duration);

    auto wrapper = [timed_handler](const EventType& event) {
        if (!(*timed_handler)(event)) {
            // Listener expired - would need access to handle to disconnect
            // This is a limitation that could be addressed with more complex design
        }
    };

    return subscribe<EventType>(std::move(wrapper), config);
}

template<typename EventType>
ManagedListenerHandle InputListenerManager::subscribe_once(std::function<void(const EventType&)> handler,
                                                          const ListenerConfig& config) {
    auto once_config = config;
    once_config.max_invocations = 1;
    return subscribe<EventType>(std::move(handler), once_config);
}

template<typename EventType>
std::vector<ManagedListenerHandle> InputListenerManager::subscribe_to_group(const std::string& group_name,
                                                                           std::vector<std::function<void(const EventType&)>> handlers,
                                                                           const ListenerConfig& base_config) {
    std::vector<ManagedListenerHandle> handles;
    handles.reserve(handlers.size());

    auto group = get_group(group_name);
    if (!group) {
        group = create_group(group_name);
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        auto config = base_config;
        config.group = group_name;
        if (config.name.empty()) {
            config.name = group_name + "_listener_" + std::to_string(i);
        }

        handles.emplace_back(subscribe<EventType>(std::move(handlers[i]), config));
    }

    return handles;
}

} // namespace lore::input