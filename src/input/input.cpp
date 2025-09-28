#include <lore/input/input.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <thread>

namespace lore::input {

// Global pointer for GLFW callbacks
static InputSystem* g_input_system = nullptr;

// SDL gamepad database for controller support
static const char* gamepad_db = R"(
03000000ba2200002010000001010000,Jess Technology USB Game Controller,a:b2,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:,leftshoulder:b4,leftstick:b10,lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:b7,rightx:a2,righty:a3,start:b9,x:b3,y:b0,
030000004c050000c2050000fff80000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,
030000005e040000130b0000ffff0000,Xbox Series Controller,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:a6,leftx:a0,lefty:a1,rightshoulder:b7,rightstick:b14,righttrigger:a7,rightx:a2,righty:a3,start:b11,x:b3,y:b4,
030000005e0400008e02000014010000,Xbox 360 Controller,a:b0,b:b1,back:b9,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b10,leftshoulder:b4,leftstick:b6,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b7,righttrigger:a5,rightx:a3,righty:a4,start:b8,x:b2,y:b3,
)";

// EventQueue implementation
EventQueue::EventQueue() {
    // Note: std::queue doesn't have reserve(), but we can set max_events_
}

EventQueue::~EventQueue() = default;

void EventQueue::push_event(std::unique_ptr<InputEvent> event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (events_.size() >= max_events_) {
        // Drop oldest event to prevent unbounded growth
        events_.pop();
    }

    events_.push(std::move(event));
}

std::vector<std::unique_ptr<InputEvent>> EventQueue::poll_events() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::unique_ptr<InputEvent>> result;
    result.reserve(events_.size());

    while (!events_.empty()) {
        result.push_back(std::move(events_.front()));
        events_.pop();
    }

    return result;
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!events_.empty()) {
        events_.pop();
    }
}

std::size_t EventQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
}

std::size_t EventQueue::capacity() const {
    return max_events_;
}

void EventQueue::reserve(std::size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_events_ = count;
}

// InputStateManager implementation
InputStateManager::InputStateManager() = default;

void InputStateManager::begin_frame() {
    // Copy current state to previous
    keys_previous_ = keys_current_;
    mouse_buttons_previous_ = mouse_buttons_current_;

    for (auto& gamepad : gamepads_) {
        if (gamepad.connected) {
            gamepad.buttons_previous = gamepad.buttons;
            gamepad.axes_previous = gamepad.axes;
        }
    }

    // Clear frame-specific flags
    keys_pressed_this_frame_.reset();
    keys_released_this_frame_.reset();
    mouse_buttons_pressed_this_frame_.reset();
    mouse_buttons_released_this_frame_.reset();

    for (auto& frame_flags : gamepads_pressed_this_frame_) {
        frame_flags.reset();
    }
    for (auto& frame_flags : gamepads_released_this_frame_) {
        frame_flags.reset();
    }

    // Reset per-frame values
    mouse_delta_ = glm::vec2(0.0f);
    scroll_delta_ = glm::vec2(0.0f);
}

void InputStateManager::end_frame() {
    ++frame_number_;
}

bool InputStateManager::is_key_pressed(Key key) const {
    return keys_current_[static_cast<std::size_t>(key)];
}

bool InputStateManager::is_key_held(Key key) const {
    const auto idx = static_cast<std::size_t>(key);
    return keys_current_[idx] && keys_previous_[idx];
}

bool InputStateManager::is_key_released(Key key) const {
    return !keys_current_[static_cast<std::size_t>(key)];
}

bool InputStateManager::was_key_pressed_this_frame(Key key) const {
    return keys_pressed_this_frame_[static_cast<std::size_t>(key)];
}

bool InputStateManager::was_key_released_this_frame(Key key) const {
    return keys_released_this_frame_[static_cast<std::size_t>(key)];
}

bool InputStateManager::is_mouse_button_pressed(MouseButton button) const {
    return mouse_buttons_current_[static_cast<std::size_t>(button)];
}

bool InputStateManager::is_mouse_button_held(MouseButton button) const {
    const auto idx = static_cast<std::size_t>(button);
    return mouse_buttons_current_[idx] && mouse_buttons_previous_[idx];
}

bool InputStateManager::is_mouse_button_released(MouseButton button) const {
    return !mouse_buttons_current_[static_cast<std::size_t>(button)];
}

bool InputStateManager::was_mouse_button_pressed_this_frame(MouseButton button) const {
    return mouse_buttons_pressed_this_frame_[static_cast<std::size_t>(button)];
}

bool InputStateManager::was_mouse_button_released_this_frame(MouseButton button) const {
    return mouse_buttons_released_this_frame_[static_cast<std::size_t>(button)];
}

bool InputStateManager::is_gamepad_connected(std::uint32_t gamepad_id) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    return gamepads_[gamepad_id].connected;
}

bool InputStateManager::is_gamepad_button_pressed(std::uint32_t gamepad_id, GamepadButton button) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    const auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return false;
    return gamepad.buttons[static_cast<std::size_t>(button)];
}

bool InputStateManager::is_gamepad_button_held(std::uint32_t gamepad_id, GamepadButton button) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    const auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return false;
    const auto idx = static_cast<std::size_t>(button);
    return gamepad.buttons[idx] && gamepad.buttons_previous[idx];
}

bool InputStateManager::is_gamepad_button_released(std::uint32_t gamepad_id, GamepadButton button) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    const auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return false;
    return !gamepad.buttons[static_cast<std::size_t>(button)];
}

bool InputStateManager::was_gamepad_button_pressed_this_frame(std::uint32_t gamepad_id, GamepadButton button) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    return gamepads_pressed_this_frame_[static_cast<std::size_t>(button)][gamepad_id];
}

bool InputStateManager::was_gamepad_button_released_this_frame(std::uint32_t gamepad_id, GamepadButton button) const {
    if (gamepad_id >= MAX_GAMEPADS) return false;
    return gamepads_released_this_frame_[static_cast<std::size_t>(button)][gamepad_id];
}

float InputStateManager::get_gamepad_axis(std::uint32_t gamepad_id, GamepadAxis axis) const {
    if (gamepad_id >= MAX_GAMEPADS) return 0.0f;
    const auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return 0.0f;
    return util::apply_deadzone(gamepad.axes[static_cast<std::size_t>(axis)], gamepad.deadzone);
}

float InputStateManager::get_gamepad_axis_delta(std::uint32_t gamepad_id, GamepadAxis axis) const {
    if (gamepad_id >= MAX_GAMEPADS) return 0.0f;
    const auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return 0.0f;
    const auto idx = static_cast<std::size_t>(axis);
    return gamepad.axes[idx] - gamepad.axes_previous[idx];
}

void InputStateManager::set_gamepad_deadzone(std::uint32_t gamepad_id, float deadzone) {
    if (gamepad_id >= MAX_GAMEPADS) return;
    gamepads_[gamepad_id].deadzone = std::clamp(deadzone, 0.0f, 1.0f);
}

const GamepadState& InputStateManager::get_gamepad_state(std::uint32_t gamepad_id) const {
    static GamepadState invalid_state{};
    if (gamepad_id >= MAX_GAMEPADS) return invalid_state;
    return gamepads_[gamepad_id];
}

std::vector<std::uint32_t> InputStateManager::get_connected_gamepads() const {
    std::vector<std::uint32_t> result;
    for (std::uint32_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (gamepads_[i].connected) {
            result.push_back(i);
        }
    }
    return result;
}

void InputStateManager::update_key_state(Key key, InputState state) {
    const auto idx = static_cast<std::size_t>(key);
    if (idx >= MAX_KEYS) return;

    const bool was_pressed = keys_current_[idx];
    const bool is_pressed = (state == InputState::Pressed || state == InputState::Held);

    keys_current_[idx] = is_pressed;

    if (is_pressed && !was_pressed) {
        keys_pressed_this_frame_[idx] = true;
    } else if (!is_pressed && was_pressed) {
        keys_released_this_frame_[idx] = true;
    }
}

void InputStateManager::update_mouse_button_state(MouseButton button, InputState state) {
    const auto idx = static_cast<std::size_t>(button);
    if (idx >= MAX_MOUSE_BUTTONS) return;

    const bool was_pressed = mouse_buttons_current_[idx];
    const bool is_pressed = (state == InputState::Pressed || state == InputState::Held);

    mouse_buttons_current_[idx] = is_pressed;

    if (is_pressed && !was_pressed) {
        mouse_buttons_pressed_this_frame_[idx] = true;
    } else if (!is_pressed && was_pressed) {
        mouse_buttons_released_this_frame_[idx] = true;
    }
}

void InputStateManager::update_mouse_position(glm::vec2 position, glm::vec2 delta) {
    mouse_position_ = position;
    mouse_delta_ += delta;  // Accumulate delta for frame
}

void InputStateManager::update_scroll(glm::vec2 delta) {
    scroll_delta_ += delta;  // Accumulate scroll for frame
}

void InputStateManager::update_gamepad_button_state(std::uint32_t gamepad_id, GamepadButton button, InputState state) {
    if (gamepad_id >= MAX_GAMEPADS) return;
    auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return;

    const auto idx = static_cast<std::size_t>(button);
    if (idx >= gamepad.buttons.size()) return;

    const bool was_pressed = gamepad.buttons[idx];
    const bool is_pressed = (state == InputState::Pressed || state == InputState::Held);

    gamepad.buttons[idx] = is_pressed;

    if (is_pressed && !was_pressed) {
        gamepads_pressed_this_frame_[idx][gamepad_id] = true;
    } else if (!is_pressed && was_pressed) {
        gamepads_released_this_frame_[idx][gamepad_id] = true;
    }
}

void InputStateManager::update_gamepad_axis_state(std::uint32_t gamepad_id, GamepadAxis axis, float value) {
    if (gamepad_id >= MAX_GAMEPADS) return;
    auto& gamepad = gamepads_[gamepad_id];
    if (!gamepad.connected) return;

    const auto idx = static_cast<std::size_t>(axis);
    if (idx >= gamepad.axes.size()) return;

    gamepad.axes[idx] = std::clamp(value, -1.0f, 1.0f);
    gamepad.last_update = std::chrono::high_resolution_clock::now();
}

void InputStateManager::update_gamepad_connection(std::uint32_t gamepad_id, bool connected, const std::string& name) {
    if (gamepad_id >= MAX_GAMEPADS) return;
    auto& gamepad = gamepads_[gamepad_id];

    gamepad.connected = connected;
    gamepad.name = name;

    if (!connected) {
        // Reset all state when disconnected
        gamepad.buttons.fill(false);
        gamepad.buttons_previous.fill(false);
        gamepad.axes.fill(0.0f);
        gamepad.axes_previous.fill(0.0f);
    }
}

// ActionMapper implementation
ActionMapper::ActionMapper() = default;

void ActionMapper::add_context(const InputContext& context) {
    // Remove existing context with same name
    remove_context(context.name);

    contexts_.push_back(context);

    // Sort by priority (higher priority first)
    std::sort(contexts_.begin(), contexts_.end(),
        [](const InputContext& a, const InputContext& b) {
            return a.priority > b.priority;
        });
}

void ActionMapper::remove_context(const std::string& name) {
    contexts_.erase(std::remove_if(contexts_.begin(), contexts_.end(),
        [&name](const InputContext& ctx) { return ctx.name == name; }),
        contexts_.end());
}

void ActionMapper::set_context_active(const std::string& name, bool active) {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&name](InputContext& ctx) { return ctx.name == name; });

    if (it != contexts_.end()) {
        it->active = active;
    }
}

void ActionMapper::set_context_priority(const std::string& name, std::uint32_t priority) {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&name](InputContext& ctx) { return ctx.name == name; });

    if (it != contexts_.end()) {
        it->priority = priority;

        // Re-sort contexts by priority
        std::sort(contexts_.begin(), contexts_.end(),
            [](const InputContext& a, const InputContext& b) {
                return a.priority > b.priority;
            });
    }
}

bool ActionMapper::is_context_active(const std::string& name) const {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&name](const InputContext& ctx) { return ctx.name == name; });

    return it != contexts_.end() && it->active;
}

void ActionMapper::bind_action(const std::string& context_name, InputAction action, const InputBinding& binding) {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&context_name](InputContext& ctx) { return ctx.name == context_name; });

    if (it == contexts_.end()) return;

    // Find existing action config or create new one
    auto action_it = std::find_if(it->actions.begin(), it->actions.end(),
        [action](const ActionConfig& cfg) { return cfg.action == action; });

    if (action_it == it->actions.end()) {
        // Create new action config
        ActionConfig config;
        config.action = action;
        config.bindings.push_back(binding);
        it->actions.push_back(config);
    } else {
        // Add binding to existing action
        auto binding_it = std::find_if(action_it->bindings.begin(), action_it->bindings.end(),
            [&binding](const InputBinding& b) {
                if (b.device != binding.device) return false;
                switch (b.device) {
                    case InputDevice::Keyboard: return b.key == binding.key;
                    case InputDevice::Mouse: return b.mouse_button == binding.mouse_button;
                    case InputDevice::Gamepad:
                        return b.gamepad_button.button == binding.gamepad_button.button &&
                               b.gamepad_button.gamepad_id == binding.gamepad_button.gamepad_id;
                }
                return false;
            });

        if (binding_it == action_it->bindings.end()) {
            action_it->bindings.push_back(binding);
        }
    }
}

void ActionMapper::unbind_action(const std::string& context_name, InputAction action, const InputBinding& binding) {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&context_name](InputContext& ctx) { return ctx.name == context_name; });

    if (it == contexts_.end()) return;

    auto action_it = std::find_if(it->actions.begin(), it->actions.end(),
        [action](const ActionConfig& cfg) { return cfg.action == action; });

    if (action_it != it->actions.end()) {
        action_it->bindings.erase(
            std::remove_if(action_it->bindings.begin(), action_it->bindings.end(),
                [&binding](const InputBinding& b) {
                    if (b.device != binding.device) return false;
                    switch (b.device) {
                        case InputDevice::Keyboard: return b.key == binding.key;
                        case InputDevice::Mouse: return b.mouse_button == binding.mouse_button;
                        case InputDevice::Gamepad:
                            return b.gamepad_button.button == binding.gamepad_button.button &&
                                   b.gamepad_button.gamepad_id == binding.gamepad_button.gamepad_id;
                    }
                    return false;
                }),
            action_it->bindings.end());
    }
}

void ActionMapper::clear_action_bindings(const std::string& context_name, InputAction action) {
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
        [&context_name](InputContext& ctx) { return ctx.name == context_name; });

    if (it == contexts_.end()) return;

    auto action_it = std::find_if(it->actions.begin(), it->actions.end(),
        [action](const ActionConfig& cfg) { return cfg.action == action; });

    if (action_it != it->actions.end()) {
        action_it->bindings.clear();
    }
}

bool ActionMapper::is_action_active(InputAction action) const {
    auto it = action_states_.find(action);
    return it != action_states_.end() && it->second.value > 0.0f;
}

float ActionMapper::get_action_value(InputAction action) const {
    auto it = action_states_.find(action);
    return it != action_states_.end() ? it->second.value : 0.0f;
}

bool ActionMapper::was_action_triggered_this_frame(InputAction action) const {
    auto it = action_states_.find(action);
    return it != action_states_.end() && it->second.triggered_this_frame;
}

bool ActionMapper::was_action_released_this_frame(InputAction action) const {
    auto it = action_states_.find(action);
    return it != action_states_.end() && it->second.released_this_frame;
}

void ActionMapper::update(const InputStateManager& state_manager) {
    // Clear frame-specific flags
    for (auto& [action, state] : action_states_) {
        state.previous_value = state.value;
        state.triggered_this_frame = false;
        state.released_this_frame = false;
        state.value = 0.0f;
    }

    // Process contexts in priority order
    for (const auto& context : contexts_) {
        if (!context.active) continue;

        for (const auto& action_config : context.actions) {
            float max_value = 0.0f;

            // Evaluate all bindings for this action
            for (const auto& binding : action_config.bindings) {
                float value = evaluate_binding(binding, state_manager);

                // Apply action configuration
                value *= action_config.sensitivity;
                if (action_config.invert) {
                    value = -value;
                }

                // Take maximum absolute value
                if (std::abs(value) > std::abs(max_value)) {
                    max_value = value;
                }
            }

            // Apply deadzone
            if (std::abs(max_value) <= action_config.deadzone) {
                max_value = 0.0f;
            }

            update_action_state(action_config.action, max_value);
        }
    }

    // Invoke callbacks for triggered actions
    for (const auto& [action, state] : action_states_) {
        if (state.triggered_this_frame || state.released_this_frame) {
            auto callback_it = action_callbacks_.find(action);
            if (callback_it != action_callbacks_.end() && callback_it->second) {
                callback_it->second(action, state.value);
            }
        }
    }
}

float ActionMapper::evaluate_binding(const InputBinding& binding, const InputStateManager& state_manager) const {
    switch (binding.device) {
        case InputDevice::Keyboard:
            return state_manager.is_key_pressed(binding.key) ? 1.0f : 0.0f;

        case InputDevice::Mouse:
            return state_manager.is_mouse_button_pressed(binding.mouse_button) ? 1.0f : 0.0f;

        case InputDevice::Gamepad: {
            const auto gamepad_id = binding.gamepad_button.gamepad_id;
            if (!state_manager.is_gamepad_connected(gamepad_id)) return 0.0f;

            // Handle gamepad button
            if (binding.gamepad_button.button != GamepadButton::A) { // Hack: axis binding uses different field
                return state_manager.is_gamepad_button_pressed(gamepad_id, binding.gamepad_button.button) ? 1.0f : 0.0f;
            } else {
                // Handle gamepad axis
                float value = state_manager.get_gamepad_axis(gamepad_id, binding.gamepad_axis.axis);

                // Apply threshold and direction
                if (binding.gamepad_axis.positive_direction) {
                    return (value > binding.gamepad_axis.threshold) ? value : 0.0f;
                } else {
                    return (value < -binding.gamepad_axis.threshold) ? -value : 0.0f;
                }
            }
        }
    }

    return 0.0f;
}

void ActionMapper::update_action_state(InputAction action, float value) {
    auto& state = action_states_[action];

    // Check for state changes
    const bool was_active = state.previous_value > 0.0f;
    const bool is_active = value > 0.0f;

    if (is_active && !was_active) {
        state.triggered_this_frame = true;
    } else if (!is_active && was_active) {
        state.released_this_frame = true;
    }

    state.value = value;
}

void ActionMapper::set_action_callback(InputAction action, ActionCallback callback) {
    action_callbacks_[action] = std::move(callback);
}

void ActionMapper::remove_action_callback(InputAction action) {
    action_callbacks_.erase(action);
}

void ActionMapper::load_config(const std::string& /*config_data*/) {
    // TODO: Implement JSON/XML configuration loading
    // This is a simplified implementation
    reset_to_defaults();
}

std::string ActionMapper::save_config() const {
    // TODO: Implement JSON/XML configuration saving
    return "{}";
}

void ActionMapper::reset_to_defaults() {
    contexts_.clear();
    action_states_.clear();
    action_callbacks_.clear();

    // Create default gameplay context
    InputContext gameplay_context;
    gameplay_context.name = "Gameplay";
    gameplay_context.priority = 100;
    gameplay_context.active = true;

    // Add default bindings
    ActionConfig move_forward;
    move_forward.action = InputAction::MoveForward;
    move_forward.bindings.push_back(InputBinding::keyboard(Key::W));
    move_forward.description = "Move Forward";
    gameplay_context.actions.push_back(move_forward);

    ActionConfig move_backward;
    move_backward.action = InputAction::MoveBackward;
    move_backward.bindings.push_back(InputBinding::keyboard(Key::S));
    move_backward.description = "Move Backward";
    gameplay_context.actions.push_back(move_backward);

    ActionConfig move_left;
    move_left.action = InputAction::MoveLeft;
    move_left.bindings.push_back(InputBinding::keyboard(Key::A));
    move_left.description = "Move Left";
    gameplay_context.actions.push_back(move_left);

    ActionConfig move_right;
    move_right.action = InputAction::MoveRight;
    move_right.bindings.push_back(InputBinding::keyboard(Key::D));
    move_right.description = "Move Right";
    gameplay_context.actions.push_back(move_right);

    ActionConfig jump;
    jump.action = InputAction::Jump;
    jump.bindings.push_back(InputBinding::keyboard(Key::Space));
    jump.description = "Jump";
    gameplay_context.actions.push_back(jump);

    ActionConfig interact;
    interact.action = InputAction::Interact;
    interact.bindings.push_back(InputBinding::keyboard(Key::E));
    interact.bindings.push_back(InputBinding::mouse(MouseButton::Left));
    interact.description = "Interact";
    gameplay_context.actions.push_back(interact);

    add_context(gameplay_context);

    // Create default menu context
    InputContext menu_context;
    menu_context.name = "Menu";
    menu_context.priority = 200;
    menu_context.active = false;

    ActionConfig menu_accept;
    menu_accept.action = InputAction::Accept;
    menu_accept.bindings.push_back(InputBinding::keyboard(Key::Enter));
    menu_accept.bindings.push_back(InputBinding::mouse(MouseButton::Left));
    menu_accept.description = "Accept/Select";
    menu_context.actions.push_back(menu_accept);

    ActionConfig menu_cancel;
    menu_cancel.action = InputAction::Cancel;
    menu_cancel.bindings.push_back(InputBinding::keyboard(Key::Escape));
    menu_cancel.bindings.push_back(InputBinding::mouse(MouseButton::Right));
    menu_cancel.description = "Cancel/Back";
    menu_context.actions.push_back(menu_cancel);

    add_context(menu_context);
}

// InputSystem implementation
InputSystem::InputSystem() = default;

InputSystem::~InputSystem() {
    if (g_input_system == this) {
        g_input_system = nullptr;
    }
}

void InputSystem::init(lore::ecs::World& world) {
    (void)world; // Unused parameter

    g_input_system = this;

    // Initialize default action mappings
    action_mapper_.reset_to_defaults();

    // Initialize gamepad database
    initialize_gamepad_database();

    // Reserve event queue space
    event_queue_.reserve(1000);

    std::cout << "Input System initialized" << std::endl;
}

void InputSystem::update(lore::ecs::World& /*world*/, float delta_time) {
    (void)delta_time; // Unused parameter

    events_processed_this_frame_ = 0;

    // Begin new frame
    state_manager_.begin_frame();

    // Poll GLFW events
    if (window_) {
        poll_glfw_events();
    }

    // Update gamepad states
    update_gamepad_states();

    // Process queued events
    process_events();

    // Update action mapper
    action_mapper_.update(state_manager_);

    // Process ECS entities with input components
    // Note: This would typically iterate over entities with InputComponent
    // For now, we'll skip ECS integration as it's not in the current task scope

    // End frame
    state_manager_.end_frame();

    total_events_processed_ += events_processed_this_frame_;
}

void InputSystem::shutdown(lore::ecs::World& world) {
    (void)world; // Unused parameter

    if (window_) {
        // Restore GLFW callbacks
        glfwSetKeyCallback(window_, nullptr);
        glfwSetMouseButtonCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetWindowFocusCallback(window_, nullptr);
        glfwSetJoystickCallback(nullptr);
    }

    event_queue_.clear();
    g_input_system = nullptr;

    std::cout << "Input System shut down" << std::endl;
}

void InputSystem::initialize_glfw(GLFWwindow* window) {
    window_ = window;

    if (!window_) {
        std::cerr << "Warning: InputSystem initialized with null GLFW window" << std::endl;
        return;
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);

    // Set GLFW callbacks
    glfwSetKeyCallback(window_, glfw_key_callback);
    glfwSetMouseButtonCallback(window_, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window_, glfw_cursor_position_callback);
    glfwSetScrollCallback(window_, glfw_scroll_callback);
    glfwSetWindowFocusCallback(window_, glfw_window_focus_callback);
    glfwSetJoystickCallback(glfw_joystick_callback);

    std::cout << "GLFW input callbacks initialized" << std::endl;
}

void InputSystem::poll_glfw_events() {
    // GLFW events are processed automatically via callbacks
    // This function can be used for additional polling if needed
}

void InputSystem::process_events() {
    auto events = event_queue_.poll_events();

    for (auto& event : events) {
        event->frame_number = state_manager_.get_frame_number();

        // Update state manager based on event type
        switch (event->type) {
            case EventType::KeyPressed: {
                auto* key_event = static_cast<KeyEvent*>(event.get());
                state_manager_.update_key_state(key_event->key, InputState::Pressed);
                break;
            }
            case EventType::KeyReleased: {
                auto* key_event = static_cast<KeyEvent*>(event.get());
                state_manager_.update_key_state(key_event->key, InputState::Released);
                break;
            }
            case EventType::MouseButtonPressed: {
                auto* mouse_event = static_cast<MouseButtonEvent*>(event.get());
                state_manager_.update_mouse_button_state(mouse_event->button, InputState::Pressed);
                break;
            }
            case EventType::MouseButtonReleased: {
                auto* mouse_event = static_cast<MouseButtonEvent*>(event.get());
                state_manager_.update_mouse_button_state(mouse_event->button, InputState::Released);
                break;
            }
            case EventType::MouseMoved: {
                auto* move_event = static_cast<MouseMoveEvent*>(event.get());
                state_manager_.update_mouse_position(move_event->position, move_event->delta);
                break;
            }
            case EventType::MouseScrolled: {
                auto* scroll_event = static_cast<MouseScrollEvent*>(event.get());
                state_manager_.update_scroll(scroll_event->offset);
                break;
            }
            case EventType::GamepadButtonPressed: {
                auto* gamepad_event = static_cast<GamepadButtonEvent*>(event.get());
                state_manager_.update_gamepad_button_state(gamepad_event->gamepad_id,
                    gamepad_event->button, InputState::Pressed);
                break;
            }
            case EventType::GamepadButtonReleased: {
                auto* gamepad_event = static_cast<GamepadButtonEvent*>(event.get());
                state_manager_.update_gamepad_button_state(gamepad_event->gamepad_id,
                    gamepad_event->button, InputState::Released);
                break;
            }
            case EventType::GamepadAxisMoved: {
                auto* axis_event = static_cast<GamepadAxisEvent*>(event.get());
                state_manager_.update_gamepad_axis_state(axis_event->gamepad_id,
                    axis_event->axis, axis_event->value);
                break;
            }
            case EventType::GamepadConnected:
            case EventType::GamepadDisconnected: {
                auto* connection_event = static_cast<GamepadConnectionEvent*>(event.get());
                state_manager_.update_gamepad_connection(connection_event->gamepad_id,
                    event->type == EventType::GamepadConnected, connection_event->name);
                break;
            }
            default:
                break;
        }

        // Invoke callbacks
        invoke_callbacks(*event);

        ++events_processed_this_frame_;
    }
}

void InputSystem::update_gamepad_states() {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) && is_gamepad_supported(jid)) {
            auto gamepad_id = static_cast<std::uint32_t>(jid);

            // Check if this is a new connection
            if (!state_manager_.is_gamepad_connected(gamepad_id)) {
                std::string name = get_gamepad_name(jid);
                auto event = std::make_unique<GamepadConnectionEvent>(EventType::GamepadConnected, gamepad_id, name);
                event_queue_.push_event(std::move(event));
                continue;
            }

            // Update button states
            int button_count;
            const unsigned char* buttons = glfwGetJoystickButtons(jid, &button_count);

            for (int i = 0; i < std::min(button_count, 15); ++i) {
                auto button = static_cast<GamepadButton>(i);
                bool is_pressed = buttons[i] == GLFW_PRESS;
                bool was_pressed = state_manager_.is_gamepad_button_pressed(gamepad_id, button);

                if (is_pressed != was_pressed) {
                    auto event_type = is_pressed ? EventType::GamepadButtonPressed : EventType::GamepadButtonReleased;
                    auto event = std::make_unique<GamepadButtonEvent>(event_type, gamepad_id, button);
                    event_queue_.push_event(std::move(event));
                }
            }

            // Update axis states
            int axis_count;
            const float* axes = glfwGetJoystickAxes(jid, &axis_count);

            for (int i = 0; i < std::min(axis_count, 6); ++i) {
                auto axis = static_cast<GamepadAxis>(i);
                float value = axes[i];
                float old_value = state_manager_.get_gamepad_axis(gamepad_id, axis);

                // Only generate event if axis changed significantly
                if (std::abs(value - old_value) > 0.01f) {
                    auto event = std::make_unique<GamepadAxisEvent>(gamepad_id, axis, value, value - old_value);
                    event_queue_.push_event(std::move(event));
                }
            }
        } else if (state_manager_.is_gamepad_connected(static_cast<std::uint32_t>(jid))) {
            // Gamepad was disconnected
            auto gamepad_id = static_cast<std::uint32_t>(jid);
            auto event = std::make_unique<GamepadConnectionEvent>(EventType::GamepadDisconnected, gamepad_id, "");
            event_queue_.push_event(std::move(event));
        }
    }
}

void InputSystem::invoke_callbacks(const InputEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }

    switch (event.type) {
        case EventType::KeyPressed:
        case EventType::KeyReleased:
        case EventType::KeyRepeated:
            if (key_callback_) {
                key_callback_(static_cast<const KeyEvent&>(event));
            }
            break;

        case EventType::MouseButtonPressed:
        case EventType::MouseButtonReleased:
            if (mouse_button_callback_) {
                mouse_button_callback_(static_cast<const MouseButtonEvent&>(event));
            }
            break;

        case EventType::MouseMoved:
            if (mouse_move_callback_) {
                mouse_move_callback_(static_cast<const MouseMoveEvent&>(event));
            }
            break;

        case EventType::MouseScrolled:
            if (mouse_scroll_callback_) {
                mouse_scroll_callback_(static_cast<const MouseScrollEvent&>(event));
            }
            break;

        case EventType::GamepadButtonPressed:
        case EventType::GamepadButtonReleased:
            if (gamepad_callback_) {
                gamepad_callback_(static_cast<const GamepadButtonEvent&>(event));
            }
            break;

        default:
            break;
    }
}

void InputSystem::initialize_gamepad_database() {
    // Initialize SDL gamepad database for controller support
    glfwUpdateGamepadMappings(gamepad_db);
    std::cout << "Gamepad database initialized" << std::endl;
}

bool InputSystem::is_gamepad_supported(int jid) const {
    return glfwJoystickIsGamepad(jid) == GLFW_TRUE;
}

std::string InputSystem::get_gamepad_name(int jid) const {
    const char* name = glfwGetGamepadName(jid);
    return name ? std::string(name) : "Unknown Gamepad";
}

void InputSystem::set_cursor_mode(bool visible, bool captured) {
    if (!window_) return;

    cursor_visible_ = visible;
    cursor_captured_ = captured;

    int mode = GLFW_CURSOR_NORMAL;
    if (!visible) {
        mode = captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_HIDDEN;
    }

    glfwSetInputMode(window_, GLFW_CURSOR, mode);

    auto event = std::make_unique<CursorModeEvent>(visible, captured);
    event_queue_.push_event(std::move(event));
}

void InputSystem::get_cursor_mode(bool& visible, bool& captured) const {
    visible = cursor_visible_;
    captured = cursor_captured_;
}

void InputSystem::load_input_config(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open input config file: " << config_path << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    action_mapper_.load_config(buffer.str());

    std::cout << "Input configuration loaded from: " << config_path << std::endl;
}

void InputSystem::save_input_config(const std::string& config_path) const {
    std::ofstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to create input config file: " << config_path << std::endl;
        return;
    }

    file << action_mapper_.save_config();
    std::cout << "Input configuration saved to: " << config_path << std::endl;
}

// GLFW callback implementations
void InputSystem::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* input_system = static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
    if (!input_system) return;

    // Convert GLFW key to our key enum
    auto our_key = static_cast<Key>(key);

    // Determine event type
    EventType event_type;
    switch (action) {
        case GLFW_PRESS: event_type = EventType::KeyPressed; break;
        case GLFW_RELEASE: event_type = EventType::KeyReleased; break;
        case GLFW_REPEAT: event_type = EventType::KeyRepeated; break;
        default: return;
    }

    // Extract modifier keys
    bool alt = (mods & GLFW_MOD_ALT) != 0;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    bool super = (mods & GLFW_MOD_SUPER) != 0;

    auto event = std::make_unique<KeyEvent>(event_type, our_key, static_cast<std::uint32_t>(scancode),
                                           alt, ctrl, shift, super);
    input_system->event_queue_.push_event(std::move(event));
}

void InputSystem::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto* input_system = static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
    if (!input_system) return;

    // Convert GLFW button to our button enum
    auto our_button = static_cast<MouseButton>(button);

    // Determine event type
    EventType event_type = (action == GLFW_PRESS) ? EventType::MouseButtonPressed : EventType::MouseButtonReleased;

    // Get mouse position
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 position(static_cast<float>(xpos), static_cast<float>(ypos));

    // Extract modifier keys
    bool alt = (mods & GLFW_MOD_ALT) != 0;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    bool super = (mods & GLFW_MOD_SUPER) != 0;

    auto event = std::make_unique<MouseButtonEvent>(event_type, our_button, position, alt, ctrl, shift, super);
    input_system->event_queue_.push_event(std::move(event));
}

void InputSystem::glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* input_system = static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
    if (!input_system) return;

    glm::vec2 new_position(static_cast<float>(xpos), static_cast<float>(ypos));
    glm::vec2 old_position = input_system->state_manager_.get_mouse_position();
    glm::vec2 delta = new_position - old_position;

    auto event = std::make_unique<MouseMoveEvent>(new_position, delta);
    input_system->event_queue_.push_event(std::move(event));
}

void InputSystem::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* input_system = static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
    if (!input_system) return;

    glm::vec2 offset(static_cast<float>(xoffset), static_cast<float>(yoffset));
    auto event = std::make_unique<MouseScrollEvent>(offset);
    input_system->event_queue_.push_event(std::move(event));
}

void InputSystem::glfw_joystick_callback(int jid, int event) {
    if (!g_input_system) return;

    auto gamepad_id = static_cast<std::uint32_t>(jid);
    EventType event_type = (event == GLFW_CONNECTED) ? EventType::GamepadConnected : EventType::GamepadDisconnected;

    std::string name;
    if (event == GLFW_CONNECTED && g_input_system->is_gamepad_supported(jid)) {
        name = g_input_system->get_gamepad_name(jid);
    }

    auto connection_event = std::make_unique<GamepadConnectionEvent>(event_type, gamepad_id, name);
    g_input_system->event_queue_.push_event(std::move(connection_event));
}

void InputSystem::glfw_window_focus_callback(GLFWwindow* window, int focused) {
    auto* input_system = static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
    if (!input_system) return;

    auto event = std::make_unique<WindowFocusEvent>(focused == GLFW_TRUE);
    input_system->event_queue_.push_event(std::move(event));
}

// Utility functions implementation
namespace util {

std::string key_to_string(Key key) {
    switch (key) {
        case Key::Space: return "Space";
        case Key::A: return "A"; case Key::B: return "B"; case Key::C: return "C"; case Key::D: return "D";
        case Key::E: return "E"; case Key::F: return "F"; case Key::G: return "G"; case Key::H: return "H";
        case Key::I: return "I"; case Key::J: return "J"; case Key::K: return "K"; case Key::L: return "L";
        case Key::M: return "M"; case Key::N: return "N"; case Key::O: return "O"; case Key::P: return "P";
        case Key::Q: return "Q"; case Key::R: return "R"; case Key::S: return "S"; case Key::T: return "T";
        case Key::U: return "U"; case Key::V: return "V"; case Key::W: return "W"; case Key::X: return "X";
        case Key::Y: return "Y"; case Key::Z: return "Z";
        case Key::Key0: return "0"; case Key::Key1: return "1"; case Key::Key2: return "2";
        case Key::Key3: return "3"; case Key::Key4: return "4"; case Key::Key5: return "5";
        case Key::Key6: return "6"; case Key::Key7: return "7"; case Key::Key8: return "8"; case Key::Key9: return "9";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";
        case Key::F1: return "F1"; case Key::F2: return "F2"; case Key::F3: return "F3"; case Key::F4: return "F4";
        case Key::F5: return "F5"; case Key::F6: return "F6"; case Key::F7: return "F7"; case Key::F8: return "F8";
        case Key::F9: return "F9"; case Key::F10: return "F10"; case Key::F11: return "F11"; case Key::F12: return "F12";
        case Key::LeftShift: return "LeftShift";
        case Key::LeftControl: return "LeftControl";
        case Key::LeftAlt: return "LeftAlt";
        default: return "Unknown";
    }
}

Key string_to_key(const std::string& str) {
    if (str == "Space") return Key::Space;
    if (str == "A") return Key::A; if (str == "B") return Key::B; if (str == "C") return Key::C; if (str == "D") return Key::D;
    if (str == "E") return Key::E; if (str == "F") return Key::F; if (str == "G") return Key::G; if (str == "H") return Key::H;
    if (str == "I") return Key::I; if (str == "J") return Key::J; if (str == "K") return Key::K; if (str == "L") return Key::L;
    if (str == "M") return Key::M; if (str == "N") return Key::N; if (str == "O") return Key::O; if (str == "P") return Key::P;
    if (str == "Q") return Key::Q; if (str == "R") return Key::R; if (str == "S") return Key::S; if (str == "T") return Key::T;
    if (str == "U") return Key::U; if (str == "V") return Key::V; if (str == "W") return Key::W; if (str == "X") return Key::X;
    if (str == "Y") return Key::Y; if (str == "Z") return Key::Z;
    if (str == "0") return Key::Key0; if (str == "1") return Key::Key1; if (str == "2") return Key::Key2;
    if (str == "3") return Key::Key3; if (str == "4") return Key::Key4; if (str == "5") return Key::Key5;
    if (str == "6") return Key::Key6; if (str == "7") return Key::Key7; if (str == "8") return Key::Key8; if (str == "9") return Key::Key9;
    if (str == "Escape") return Key::Escape;
    if (str == "Enter") return Key::Enter;
    if (str == "Tab") return Key::Tab;
    if (str == "Backspace") return Key::Backspace;
    if (str == "Insert") return Key::Insert;
    if (str == "Delete") return Key::Delete;
    if (str == "Right") return Key::Right;
    if (str == "Left") return Key::Left;
    if (str == "Down") return Key::Down;
    if (str == "Up") return Key::Up;
    if (str == "F1") return Key::F1; if (str == "F2") return Key::F2; if (str == "F3") return Key::F3; if (str == "F4") return Key::F4;
    if (str == "F5") return Key::F5; if (str == "F6") return Key::F6; if (str == "F7") return Key::F7; if (str == "F8") return Key::F8;
    if (str == "F9") return Key::F9; if (str == "F10") return Key::F10; if (str == "F11") return Key::F11; if (str == "F12") return Key::F12;
    if (str == "LeftShift") return Key::LeftShift;
    if (str == "LeftControl") return Key::LeftControl;
    if (str == "LeftAlt") return Key::LeftAlt;
    return Key::Unknown;
}

std::string mouse_button_to_string(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Right: return "Right";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Button4: return "Button4";
        case MouseButton::Button5: return "Button5";
        case MouseButton::Button6: return "Button6";
        case MouseButton::Button7: return "Button7";
        case MouseButton::Button8: return "Button8";
        default: return "Unknown";
    }
}

MouseButton string_to_mouse_button(const std::string& str) {
    if (str == "Left") return MouseButton::Left;
    if (str == "Right") return MouseButton::Right;
    if (str == "Middle") return MouseButton::Middle;
    if (str == "Button4") return MouseButton::Button4;
    if (str == "Button5") return MouseButton::Button5;
    if (str == "Button6") return MouseButton::Button6;
    if (str == "Button7") return MouseButton::Button7;
    if (str == "Button8") return MouseButton::Button8;
    return MouseButton::Left; // Default
}

std::string gamepad_button_to_string(GamepadButton button) {
    switch (button) {
        case GamepadButton::A: return "A";
        case GamepadButton::B: return "B";
        case GamepadButton::X: return "X";
        case GamepadButton::Y: return "Y";
        case GamepadButton::LeftBumper: return "LeftBumper";
        case GamepadButton::RightBumper: return "RightBumper";
        case GamepadButton::Back: return "Back";
        case GamepadButton::Start: return "Start";
        case GamepadButton::Guide: return "Guide";
        case GamepadButton::LeftThumb: return "LeftThumb";
        case GamepadButton::RightThumb: return "RightThumb";
        case GamepadButton::DpadUp: return "DpadUp";
        case GamepadButton::DpadRight: return "DpadRight";
        case GamepadButton::DpadDown: return "DpadDown";
        case GamepadButton::DpadLeft: return "DpadLeft";
        default: return "Unknown";
    }
}

GamepadButton string_to_gamepad_button(const std::string& str) {
    if (str == "A") return GamepadButton::A;
    if (str == "B") return GamepadButton::B;
    if (str == "X") return GamepadButton::X;
    if (str == "Y") return GamepadButton::Y;
    if (str == "LeftBumper") return GamepadButton::LeftBumper;
    if (str == "RightBumper") return GamepadButton::RightBumper;
    if (str == "Back") return GamepadButton::Back;
    if (str == "Start") return GamepadButton::Start;
    if (str == "Guide") return GamepadButton::Guide;
    if (str == "LeftThumb") return GamepadButton::LeftThumb;
    if (str == "RightThumb") return GamepadButton::RightThumb;
    if (str == "DpadUp") return GamepadButton::DpadUp;
    if (str == "DpadRight") return GamepadButton::DpadRight;
    if (str == "DpadDown") return GamepadButton::DpadDown;
    if (str == "DpadLeft") return GamepadButton::DpadLeft;
    return GamepadButton::A; // Default
}

std::string gamepad_axis_to_string(GamepadAxis axis) {
    switch (axis) {
        case GamepadAxis::LeftX: return "LeftX";
        case GamepadAxis::LeftY: return "LeftY";
        case GamepadAxis::RightX: return "RightX";
        case GamepadAxis::RightY: return "RightY";
        case GamepadAxis::LeftTrigger: return "LeftTrigger";
        case GamepadAxis::RightTrigger: return "RightTrigger";
        default: return "Unknown";
    }
}

GamepadAxis string_to_gamepad_axis(const std::string& str) {
    if (str == "LeftX") return GamepadAxis::LeftX;
    if (str == "LeftY") return GamepadAxis::LeftY;
    if (str == "RightX") return GamepadAxis::RightX;
    if (str == "RightY") return GamepadAxis::RightY;
    if (str == "LeftTrigger") return GamepadAxis::LeftTrigger;
    if (str == "RightTrigger") return GamepadAxis::RightTrigger;
    return GamepadAxis::LeftX; // Default
}

std::string action_to_string(InputAction action) {
    switch (action) {
        case InputAction::None: return "None";
        case InputAction::MoveForward: return "MoveForward";
        case InputAction::MoveBackward: return "MoveBackward";
        case InputAction::MoveLeft: return "MoveLeft";
        case InputAction::MoveRight: return "MoveRight";
        case InputAction::Jump: return "Jump";
        case InputAction::Crouch: return "Crouch";
        case InputAction::Sprint: return "Sprint";
        case InputAction::LookUp: return "LookUp";
        case InputAction::LookDown: return "LookDown";
        case InputAction::LookLeft: return "LookLeft";
        case InputAction::LookRight: return "LookRight";
        case InputAction::Interact: return "Interact";
        case InputAction::Attack: return "Attack";
        case InputAction::SecondaryAttack: return "SecondaryAttack";
        case InputAction::Block: return "Block";
        case InputAction::Reload: return "Reload";
        case InputAction::MenuToggle: return "MenuToggle";
        case InputAction::Inventory: return "Inventory";
        case InputAction::Map: return "Map";
        case InputAction::Accept: return "Accept";
        case InputAction::Cancel: return "Cancel";
        default: return "Custom";
    }
}

InputAction string_to_action(const std::string& str) {
    if (str == "None") return InputAction::None;
    if (str == "MoveForward") return InputAction::MoveForward;
    if (str == "MoveBackward") return InputAction::MoveBackward;
    if (str == "MoveLeft") return InputAction::MoveLeft;
    if (str == "MoveRight") return InputAction::MoveRight;
    if (str == "Jump") return InputAction::Jump;
    if (str == "Crouch") return InputAction::Crouch;
    if (str == "Sprint") return InputAction::Sprint;
    if (str == "LookUp") return InputAction::LookUp;
    if (str == "LookDown") return InputAction::LookDown;
    if (str == "LookLeft") return InputAction::LookLeft;
    if (str == "LookRight") return InputAction::LookRight;
    if (str == "Interact") return InputAction::Interact;
    if (str == "Attack") return InputAction::Attack;
    if (str == "SecondaryAttack") return InputAction::SecondaryAttack;
    if (str == "Block") return InputAction::Block;
    if (str == "Reload") return InputAction::Reload;
    if (str == "MenuToggle") return InputAction::MenuToggle;
    if (str == "Inventory") return InputAction::Inventory;
    if (str == "Map") return InputAction::Map;
    if (str == "Accept") return InputAction::Accept;
    if (str == "Cancel") return InputAction::Cancel;
    return InputAction::None; // Default
}

bool is_modifier_key(Key key) {
    return key == Key::LeftShift || key == Key::RightShift ||
           key == Key::LeftControl || key == Key::RightControl ||
           key == Key::LeftAlt || key == Key::RightAlt ||
           key == Key::LeftSuper || key == Key::RightSuper;
}

bool is_function_key(Key key) {
    return key >= Key::F1 && key <= Key::F25;
}

bool is_arrow_key(Key key) {
    return key == Key::Up || key == Key::Down || key == Key::Left || key == Key::Right;
}

bool is_number_key(Key key) {
    return key >= Key::Key0 && key <= Key::Key9;
}

bool is_letter_key(Key key) {
    return key >= Key::A && key <= Key::Z;
}

float apply_deadzone(float value, float deadzone) {
    if (std::abs(value) <= deadzone) {
        return 0.0f;
    }

    // Scale the remaining range to [0, 1]
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    float abs_value = std::abs(value);

    return sign * ((abs_value - deadzone) / (1.0f - deadzone));
}

glm::vec2 apply_circular_deadzone(glm::vec2 input, float deadzone) {
    float magnitude = glm::length(input);

    if (magnitude <= deadzone) {
        return glm::vec2(0.0f);
    }

    // Scale the remaining range to [0, 1]
    float normalized_magnitude = (magnitude - deadzone) / (1.0f - deadzone);
    return glm::normalize(input) * normalized_magnitude;
}

} // namespace util

} // namespace lore::input