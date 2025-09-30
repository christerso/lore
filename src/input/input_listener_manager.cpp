#include <lore/input/input_listener_manager.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace lore::input {

// Predefined configurations
namespace listener_configs {
    const ListenerConfig HIGH_PRIORITY = {
        .priority = EventPriority::High,
        .name = "HighPriority",
        .auto_remove = true
    };

    const ListenerConfig LOW_PRIORITY = {
        .priority = EventPriority::Low,
        .name = "LowPriority",
        .auto_remove = true
    };

    const ListenerConfig UI_LISTENER = {
        .priority = EventPriority::High,
        .name = "UI",
        .group = "UI",
        .auto_remove = true
    };

    const ListenerConfig GAMEPLAY_LISTENER = {
        .priority = EventPriority::Normal,
        .name = "Gameplay",
        .group = "Gameplay",
        .auto_remove = true
    };

    const ListenerConfig DEBUG_LISTENER = {
        .priority = EventPriority::Low,
        .name = "Debug",
        .group = "Debug",
        .auto_remove = true
    };

    const ListenerConfig ONE_SHOT = {
        .priority = EventPriority::Normal,
        .name = "OneShot",
        .auto_remove = true,
        .max_invocations = 1
    };

    const ListenerConfig TEMPORARY = {
        .priority = EventPriority::Normal,
        .name = "Temporary",
        .auto_remove = true,
        .timeout = std::chrono::milliseconds(5000)
    };
}

// ManagedListenerHandle implementation
ManagedListenerHandle::ManagedListenerHandle(ListenerHandle handle, const ListenerConfig& config)
    : handle_(std::move(handle))
    , config_(config)
    , creation_time_(std::chrono::high_resolution_clock::now())
    , last_invocation_time_(creation_time_)
{}

ManagedListenerHandle::~ManagedListenerHandle() {
    if (config_.auto_remove) {
        disconnect();
    }
}

ManagedListenerHandle::ManagedListenerHandle(ManagedListenerHandle&& other) noexcept
    : handle_(std::move(other.handle_))
    , config_(std::move(other.config_))
    , creation_time_(other.creation_time_)
    , last_invocation_time_(other.last_invocation_time_)
{
    invocation_count_.store(other.invocation_count_.load());
}

ManagedListenerHandle& ManagedListenerHandle::operator=(ManagedListenerHandle&& other) noexcept {
    if (this != &other) {
        if (config_.auto_remove) {
            disconnect();
        }

        handle_ = std::move(other.handle_);
        config_ = std::move(other.config_);
        invocation_count_.store(other.invocation_count_.load());
        creation_time_ = other.creation_time_;
        last_invocation_time_ = other.last_invocation_time_;
    }
    return *this;
}

void ManagedListenerHandle::disconnect() {
    handle_.disconnect();
}

bool ManagedListenerHandle::is_connected() const {
    return handle_.is_connected();
}

bool ManagedListenerHandle::should_auto_remove() const {
    // Check invocation limit
    if (config_.max_invocations > 0 && invocation_count_.load() >= config_.max_invocations) {
        return true;
    }

    // Check timeout
    if (config_.timeout.count() > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time_);
        if (elapsed >= config_.timeout) {
            return true;
        }
    }

    return false;
}

// ListenerGroup implementation
ListenerGroup::ListenerGroup(std::string name)
    : name_(std::move(name))
{}

ListenerGroup::~ListenerGroup() {
    clear();
}

std::size_t ListenerGroup::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_expired_listeners();
    return listeners_.size();
}

bool ListenerGroup::empty() const {
    return size() == 0;
}

void ListenerGroup::add_listener(std::shared_ptr<ManagedListenerHandle> handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.emplace_back(handle);
}

void ListenerGroup::remove_listener(const std::shared_ptr<ManagedListenerHandle>& handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
                      [&handle](const std::weak_ptr<ManagedListenerHandle>& weak_handle) {
                          auto shared_handle = weak_handle.lock();
                          return !shared_handle || shared_handle.get() == handle.get();
                      }),
        listeners_.end());
}

void ListenerGroup::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Disconnect all listeners before clearing
    for (auto& weak_handle : listeners_) {
        if (auto handle = weak_handle.lock()) {
            handle->disconnect();
        }
    }

    listeners_.clear();
}

void ListenerGroup::disconnect_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weak_handle : listeners_) {
        if (auto handle = weak_handle.lock()) {
            handle->disconnect();
        }
    }

    cleanup_expired_listeners();
}

void ListenerGroup::set_enabled(bool enabled) {
    enabled_.store(enabled);

    if (!enabled) {
        // Temporarily disconnect all listeners
        disconnect_all();
    }
}

void ListenerGroup::set_group_priority(EventPriority priority) {
    group_priority_ = priority;
    // Note: This doesn't change existing listener priorities
    // New listeners in this group would use this priority
}

std::size_t ListenerGroup::get_total_invocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_expired_listeners();

    std::size_t total = 0;
    for (const auto& weak_handle : listeners_) {
        if (auto handle = weak_handle.lock()) {
            total += handle->get_invocation_count();
        }
    }

    return total;
}

std::vector<std::shared_ptr<ManagedListenerHandle>> ListenerGroup::get_listeners() const {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_expired_listeners();

    std::vector<std::shared_ptr<ManagedListenerHandle>> result;
    result.reserve(listeners_.size());

    for (const auto& weak_handle : listeners_) {
        if (auto handle = weak_handle.lock()) {
            result.push_back(handle);
        }
    }

    return result;
}

void ListenerGroup::cleanup_expired_listeners() const {
    auto new_end = std::remove_if(listeners_.begin(), listeners_.end(),
                                 [](const std::weak_ptr<ManagedListenerHandle>& weak_handle) {
                                     auto handle = weak_handle.lock();
                                     return !handle || !handle->is_connected();
                                 });
    listeners_.erase(new_end, listeners_.end());
}

// InputListenerManager implementation
InputListenerManager::InputListenerManager(EventDispatcher& dispatcher)
    : event_dispatcher_(dispatcher)
{}

InputListenerManager::~InputListenerManager() {
    disconnect_all();
}

std::shared_ptr<ListenerGroup> InputListenerManager::create_group(const std::string& name) {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto group = std::make_shared<ListenerGroup>(name);
    groups_[name] = group;
    return group;
}

std::shared_ptr<ListenerGroup> InputListenerManager::get_group(const std::string& name) {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto it = groups_.find(name);
    if (it != groups_.end()) {
        return it->second;
    }
    return nullptr;
}

void InputListenerManager::remove_group(const std::string& name) {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto it = groups_.find(name);
    if (it != groups_.end()) {
        it->second->clear();
        groups_.erase(it);
    }
}

std::vector<std::string> InputListenerManager::get_group_names() const {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    std::vector<std::string> names;
    names.reserve(groups_.size());

    for (const auto& [name, group] : groups_) {
        names.push_back(name);
    }

    return names;
}

void InputListenerManager::disconnect_group(const std::string& group_name) {
    auto group = get_group(group_name);
    if (group) {
        group->disconnect_all();
    }
}

void InputListenerManager::disconnect_all() {
    // Disconnect all group listeners
    {
        std::lock_guard<std::mutex> lock(groups_mutex_);
        for (auto& [name, group] : groups_) {
            group->disconnect_all();
        }
        groups_.clear();
    }

    // Disconnect all managed listeners
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        for (auto& weak_handle : managed_listeners_) {
            if (auto handle = weak_handle.lock()) {
                handle->disconnect();
            }
        }
        managed_listeners_.clear();
    }
}

// Input-specific convenience methods
ManagedListenerHandle InputListenerManager::on_key_pressed(KeyCode key, std::function<void()> handler,
                                                          const ListenerConfig& config) {
    auto key_handler = [key, handler = std::move(handler)](const KeyPressedEvent& event) {
        if (event.key == key && !event.is_repeat) {
            handler();
        }
    };

    auto key_config = config;
    if (key_config.name.empty()) {
        key_config.name = "KeyPressed_" + event_utils::keycode_to_string(key);
    }

    return subscribe<KeyPressedEvent>(std::move(key_handler), key_config);
}

ManagedListenerHandle InputListenerManager::on_key_released(KeyCode key, std::function<void()> handler,
                                                           const ListenerConfig& config) {
    auto key_handler = [key, handler = std::move(handler)](const KeyReleasedEvent& event) {
        if (event.key == key) {
            handler();
        }
    };

    auto key_config = config;
    if (key_config.name.empty()) {
        key_config.name = "KeyReleased_" + event_utils::keycode_to_string(key);
    }

    return subscribe<KeyReleasedEvent>(std::move(key_handler), key_config);
}

ManagedListenerHandle InputListenerManager::on_mouse_clicked(MouseButton button, std::function<void(glm::vec2)> handler,
                                                            const ListenerConfig& config) {
    auto mouse_handler = [button, handler = std::move(handler)](const MouseButtonPressedEvent& event) {
        if (event.button == button) {
            handler(event.position);
        }
    };

    auto mouse_config = config;
    if (mouse_config.name.empty()) {
        mouse_config.name = "MouseClicked_" + event_utils::mouse_button_to_string(button);
    }

    return subscribe<MouseButtonPressedEvent>(std::move(mouse_handler), mouse_config);
}

ManagedListenerHandle InputListenerManager::on_gamepad_button(std::uint32_t gamepad_id, GamepadButton button,
                                                             std::function<void()> handler,
                                                             const ListenerConfig& config) {
    auto gamepad_handler = [gamepad_id, button, handler = std::move(handler)](const GamepadButtonPressedEvent& event) {
        if (event.gamepad_id == gamepad_id && event.button == button) {
            handler();
        }
    };

    auto gamepad_config = config;
    if (gamepad_config.name.empty()) {
        gamepad_config.name = "GamepadButton_" + std::to_string(gamepad_id) + "_" +
                             event_utils::gamepad_button_to_string(button);
    }

    return subscribe<GamepadButtonPressedEvent>(std::move(gamepad_handler), gamepad_config);
}

ManagedListenerHandle InputListenerManager::on_key_combination(std::vector<KeyCode> keys,
                                                              std::function<void()> handler,
                                                              const ListenerConfig& config) {
    if (keys.empty()) {
        return ManagedListenerHandle{};
    }

    // Create combination tracker
    KeyCombinationTracker tracker;
    tracker.required_keys = std::move(keys);
    tracker.handler = std::move(handler);

    auto key_pressed_handler = [this](const KeyPressedEvent& event) {
        if (state_tracking_enabled_) {
            update_key_combination_state(event.key, true);
        }
    };

    auto key_released_handler = [this](const KeyReleasedEvent& event) {
        if (state_tracking_enabled_) {
            update_key_combination_state(event.key, false);
        }
    };

    // Store the tracker
    key_combinations_.push_back(std::move(tracker));

    // Subscribe to key events
    auto combo_config = config;
    if (combo_config.name.empty()) {
        std::string key_names;
        for (const auto& key : tracker.required_keys) {
            if (!key_names.empty()) key_names += "+";
            key_names += event_utils::keycode_to_string(key);
        }
        combo_config.name = "KeyCombo_" + key_names;
    }

    // Return handle for the pressed event (both handlers will be managed together)
    return subscribe<KeyPressedEvent>(std::move(key_pressed_handler), combo_config);
}

ManagedListenerHandle InputListenerManager::on_input_action(InputAction action,
                                                           std::function<void(float)> handler,
                                                           const ListenerConfig& config) {
    auto action_handler = [action, handler = std::move(handler)](const InputActionEvent& event) {
        if (event.action == action) {
            handler(event.value);
        }
    };

    auto action_config = config;
    if (action_config.name.empty()) {
        action_config.name = "InputAction_" + event_utils::input_action_to_string(action);
    }

    return subscribe<InputActionEvent>(std::move(action_handler), action_config);
}

void InputListenerManager::cleanup_expired_listeners() {
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        managed_listeners_.erase(
            std::remove_if(managed_listeners_.begin(), managed_listeners_.end(),
                          [](const std::weak_ptr<ManagedListenerHandle>& weak_handle) {
                              auto handle = weak_handle.lock();
                              return !handle || !handle->is_connected() || handle->should_auto_remove();
                          }),
            managed_listeners_.end());
    }

    {
        std::lock_guard<std::mutex> lock(groups_mutex_);
        for (auto& [name, group] : groups_) {
            group->disconnect_all();  // This will cleanup expired listeners
        }
    }
}

void InputListenerManager::cleanup_unused_groups() {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    for (auto it = groups_.begin(); it != groups_.end();) {
        if (it->second->empty()) {
            it = groups_.erase(it);
        } else {
            ++it;
        }
    }
}

InputListenerManager::Statistics InputListenerManager::get_statistics() const {
    Statistics stats;

    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        stats.total_listeners = managed_listeners_.size();

        for (const auto& weak_handle : managed_listeners_) {
            if (auto handle = weak_handle.lock()) {
                if (handle->is_connected()) {
                    ++stats.active_listeners;
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(groups_mutex_);
        stats.total_groups = groups_.size();

        for (const auto& [name, group] : groups_) {
            if (!group->empty()) {
                ++stats.active_groups;
            }
        }
    }

    stats.total_invocations = total_invocations_.load();

    return stats;
}

void InputListenerManager::reset_statistics() {
    total_invocations_.store(0);
}

std::vector<std::string> InputListenerManager::get_listener_names() const {
    std::vector<std::string> names;

    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        names.reserve(managed_listeners_.size());

        for (const auto& weak_handle : managed_listeners_) {
            if (auto handle = weak_handle.lock()) {
                names.push_back(handle->get_name());
            }
        }
    }

    return names;
}

void InputListenerManager::print_listener_summary() const {
    auto stats = get_statistics();

    std::cout << "=== Input Listener Manager Summary ===" << std::endl;
    std::cout << "Total Listeners: " << stats.total_listeners << std::endl;
    std::cout << "Active Listeners: " << stats.active_listeners << std::endl;
    std::cout << "Total Groups: " << stats.total_groups << std::endl;
    std::cout << "Active Groups: " << stats.active_groups << std::endl;
    std::cout << "Total Invocations: " << stats.total_invocations << std::endl;

    std::cout << "\nGroups:" << std::endl;
    {
        std::lock_guard<std::mutex> lock(groups_mutex_);
        for (const auto& [name, group] : groups_) {
            std::cout << "  " << name << ": " << group->size() << " listeners, "
                     << group->get_total_invocations() << " invocations" << std::endl;
        }
    }

    std::cout << "\nActive Listeners:" << std::endl;
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        for (const auto& weak_handle : managed_listeners_) {
            if (auto handle = weak_handle.lock()) {
                if (handle->is_connected()) {
                    auto creation_time = handle->get_creation_time();
                    auto now = std::chrono::high_resolution_clock::now();
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - creation_time);

                    std::cout << "  " << handle->get_name()
                             << " (age: " << age.count() << "s, invocations: "
                             << handle->get_invocation_count() << ")" << std::endl;
                }
            }
        }
    }

    std::cout << "=====================================\n" << std::endl;
}

// Internal helper methods
std::string InputListenerManager::generate_listener_name([[maybe_unused]] const std::string& event_type, const ListenerConfig& config) {
    static std::atomic<std::size_t> counter{0};

    std::string base_name = config.name.empty() ? "Listener" : config.name;
    return base_name + "_" + std::to_string(++counter);
}

void InputListenerManager::register_managed_listener(std::shared_ptr<ManagedListenerHandle> handle) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    managed_listeners_.emplace_back(handle);
}

void InputListenerManager::update_listener_statistics([[maybe_unused]] const ManagedListenerHandle& handle) {
    // This would be called by wrapped handlers to update statistics
    total_invocations_.fetch_add(1);
}

bool InputListenerManager::KeyCombinationTracker::all_keys_pressed() const {
    for (const auto& key : required_keys) {
        if (!current_keys[static_cast<std::size_t>(key)]) {
            return false;
        }
    }
    return true;
}

void InputListenerManager::update_key_combination_state(KeyCode key, bool pressed) {
    std::size_t key_index = static_cast<std::size_t>(key);
    if (key_index >= 512) return;

    for (auto& tracker : key_combinations_) {
        // Check if this key is part of the combination
        auto it = std::find(tracker.required_keys.begin(), tracker.required_keys.end(), key);
        if (it != tracker.required_keys.end()) {
            tracker.current_keys[key_index] = pressed;

            // Check if all keys are now pressed
            if (pressed && tracker.all_keys_pressed()) {
                tracker.handler();
                // Reset state to prevent repeated triggering
                tracker.current_keys.reset();
            }
        }
    }
}

} // namespace lore::input