#include <lore/input/glfw_input_handler.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace lore::input {

// Static storage for window-to-handler mapping
static std::unordered_map<GLFWwindow*, GLFWInputHandler*> g_window_handlers;

// GLFWInputHandler implementation
GLFWInputHandler::GLFWInputHandler(EventDispatcher& dispatcher)
    : event_dispatcher_(dispatcher)
{
    // Initialize gamepad states
    for (auto& gamepad : gamepads_) {
        gamepad = GLFWGamepadState{};
    }
}

GLFWInputHandler::~GLFWInputHandler() {
    shutdown();
}

void GLFWInputHandler::initialize(GLFWwindow* window) {
    if (!window) {
        std::cerr << "GLFWInputHandler::initialize: Invalid window pointer" << std::endl;
        return;
    }

    window_ = window;
    set_handler_for_window(window, this);

    // Set up all GLFW callbacks
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCharCallback(window, glfw_char_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window, glfw_cursor_position_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetCursorEnterCallback(window, glfw_cursor_enter_callback);
    glfwSetWindowSizeCallback(window, glfw_window_size_callback);
    glfwSetWindowCloseCallback(window, glfw_window_close_callback);
    glfwSetWindowFocusCallback(window, glfw_window_focus_callback);
    glfwSetWindowIconifyCallback(window, glfw_window_iconify_callback);
    glfwSetWindowMaximizeCallback(window, glfw_window_maximize_callback);
    glfwSetWindowPosCallback(window, glfw_window_pos_callback);
    glfwSetWindowRefreshCallback(window, glfw_window_refresh_callback);
    glfwSetDropCallback(window, glfw_drop_callback);

    // Set global joystick callback
    glfwSetJoystickCallback(glfw_joystick_callback);

    // Initialize mouse position
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    mouse_state_.position = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
    mouse_state_.last_position = mouse_state_.position;

    // Check for initially connected gamepads
    check_gamepad_connections();

    std::cout << "GLFWInputHandler initialized successfully" << std::endl;
}

void GLFWInputHandler::shutdown() {
    if (window_) {
        // Clear callbacks
        glfwSetKeyCallback(window_, nullptr);
        glfwSetCharCallback(window_, nullptr);
        glfwSetMouseButtonCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetCursorEnterCallback(window_, nullptr);
        glfwSetWindowSizeCallback(window_, nullptr);
        glfwSetWindowCloseCallback(window_, nullptr);
        glfwSetWindowFocusCallback(window_, nullptr);
        glfwSetWindowIconifyCallback(window_, nullptr);
        glfwSetWindowMaximizeCallback(window_, nullptr);
        glfwSetWindowPosCallback(window_, nullptr);
        glfwSetWindowRefreshCallback(window_, nullptr);
        glfwSetDropCallback(window_, nullptr);

        // Remove from handler map
        g_window_handlers.erase(window_);
        window_ = nullptr;
    }

    std::cout << "GLFWInputHandler shut down" << std::endl;
}

void GLFWInputHandler::update_frame() {
    events_generated_this_frame_ = 0;

    // Update previous states
    keyboard_state_previous_ = keyboard_state_;
    mouse_state_.buttons_previous = mouse_state_.buttons;

    for (auto& gamepad : gamepads_) {
        if (gamepad.connected) {
            gamepad.buttons_previous = gamepad.buttons;
            gamepad.axes_previous = gamepad.axes;
        }
    }

    // Update gamepad states
    update_gamepad_states();
}

void GLFWInputHandler::poll_events() {
    glfwPollEvents();
}

void GLFWInputHandler::set_cursor_mode(bool visible, bool locked) {
    if (!window_) return;

    cursor_visible_ = visible;
    cursor_locked_ = locked;

    if (locked) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (visible) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
}

void GLFWInputHandler::get_cursor_mode(bool& visible, bool& locked) const {
    visible = cursor_visible_;
    locked = cursor_locked_;
}

bool GLFWInputHandler::is_key_pressed(KeyCode key) const {
    int key_index = static_cast<int>(key);
    if (key_index < 0 || key_index >= 512) return false;
    return keyboard_state_[key_index];
}

bool GLFWInputHandler::is_mouse_button_pressed(MouseButton button) const {
    int button_index = static_cast<int>(button);
    if (button_index < 0 || button_index >= 8) return false;
    return mouse_state_.buttons[button_index];
}

bool GLFWInputHandler::is_gamepad_connected(std::uint32_t gamepad_id) const {
    if (gamepad_id >= gamepads_.size()) return false;
    return gamepads_[gamepad_id].connected;
}

// GLFW Callback Implementations
void GLFWInputHandler::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_keyboard_event(key, scancode, action, mods);
    }
}

void GLFWInputHandler::glfw_char_callback(GLFWwindow* window, unsigned int codepoint) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_text_input_event(codepoint);
    }
}

void GLFWInputHandler::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_mouse_button_event(button, action, mods);
    }
}

void GLFWInputHandler::glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_mouse_move_event(xpos, ypos);
    }
}

void GLFWInputHandler::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_scroll_event(xoffset, yoffset);
    }
}

void GLFWInputHandler::glfw_cursor_enter_callback(GLFWwindow* window, int entered) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_mouse_enter_event(entered == GLFW_TRUE);
    }
}

void GLFWInputHandler::glfw_window_size_callback(GLFWwindow* window, int width, int height) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_size_event(width, height);
    }
}

void GLFWInputHandler::glfw_window_close_callback(GLFWwindow* window) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_close_event();
    }
}

void GLFWInputHandler::glfw_window_focus_callback(GLFWwindow* window, int focused) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_focus_event(focused == GLFW_TRUE);
    }
}

void GLFWInputHandler::glfw_window_iconify_callback(GLFWwindow* window, int iconified) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_iconify_event(iconified == GLFW_TRUE);
    }
}

void GLFWInputHandler::glfw_window_maximize_callback(GLFWwindow* window, int maximized) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_maximize_event(maximized == GLFW_TRUE);
    }
}

void GLFWInputHandler::glfw_window_pos_callback(GLFWwindow* window, int xpos, int ypos) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_move_event(xpos, ypos);
    }
}

void GLFWInputHandler::glfw_window_refresh_callback(GLFWwindow* window) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_window_refresh_event();
    }
}

void GLFWInputHandler::glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    auto* handler = get_handler_from_window(window);
    if (handler) {
        handler->generate_file_drop_event(count, paths);
    }
}

void GLFWInputHandler::glfw_joystick_callback(int jid, int event) {
    // Find handlers for all windows and notify them
    for (const auto& [window, handler] : g_window_handlers) {
        if (handler) {
            handler->generate_gamepad_connection_event(jid, event == GLFW_CONNECTED);
        }
    }
}

// Event Generation Methods
void GLFWInputHandler::generate_keyboard_event(int key, int scancode, int action, int mods) {
    KeyCode key_code = event_utils::glfw_key_to_keycode(key);
    ModifierKey modifiers = create_modifier_flags(mods);

    // Update keyboard state
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        keyboard_state_[static_cast<std::size_t>(key)] = true;
    } else if (action == GLFW_RELEASE) {
        keyboard_state_[static_cast<std::size_t>(key)] = false;
    }

    // Generate appropriate event
    switch (action) {
        case GLFW_PRESS:
            publish_event<KeyPressedEvent>(key_code, static_cast<std::uint32_t>(scancode), modifiers, false);
            break;
        case GLFW_RELEASE:
            publish_event<KeyReleasedEvent>(key_code, static_cast<std::uint32_t>(scancode), modifiers);
            break;
        case GLFW_REPEAT:
            publish_event<KeyPressedEvent>(key_code, static_cast<std::uint32_t>(scancode), modifiers, true);
            break;
    }

    // Also generate generic keyboard event
    InputState state = (action == GLFW_PRESS) ? InputState::Pressed :
                      (action == GLFW_RELEASE) ? InputState::Released : InputState::Repeated;
    publish_event<KeyboardEvent>(key_code, static_cast<std::uint32_t>(scancode), state, modifiers);
}

void GLFWInputHandler::generate_text_input_event(unsigned int codepoint) {
    // Convert codepoint to UTF-8 string
    std::string text;
    if (codepoint < 0x80) {
        text = static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        text += static_cast<char>(0xC0 | (codepoint >> 6));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        text += static_cast<char>(0xE0 | (codepoint >> 12));
        text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        text += static_cast<char>(0xF0 | (codepoint >> 18));
        text += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    }

    publish_event<TextInputEvent>(text, codepoint);
}

void GLFWInputHandler::generate_mouse_button_event(int button, int action, int mods) {
    MouseButton mouse_btn = event_utils::glfw_mouse_button_to_mouse_button(button);
    ModifierKey modifiers = create_modifier_flags(mods);

    // Update mouse button state
    if (action == GLFW_PRESS) {
        mouse_state_.buttons[button] = true;
    } else if (action == GLFW_RELEASE) {
        mouse_state_.buttons[button] = false;
    }

    // Detect multi-clicks
    std::uint32_t click_count = 1;
    if (action == GLFW_PRESS) {
        click_count = detect_multi_click(mouse_btn, mouse_state_.position);
    }

    // Generate events
    InputState state = (action == GLFW_PRESS) ? InputState::Pressed : InputState::Released;

    publish_event<MouseButtonEvent>(mouse_btn, state, mouse_state_.position, modifiers);

    if (action == GLFW_PRESS) {
        publish_event<MouseButtonPressedEvent>(mouse_btn, mouse_state_.position, modifiers, click_count);
    } else if (action == GLFW_RELEASE) {
        publish_event<MouseButtonReleasedEvent>(mouse_btn, mouse_state_.position, modifiers);
    }
}

void GLFWInputHandler::generate_mouse_move_event(double xpos, double ypos) {
    glm::vec2 new_position(static_cast<float>(xpos), static_cast<float>(ypos));

    // Calculate delta
    glm::vec2 delta = new_position - mouse_state_.last_position;

    // Apply sensitivity
    delta *= mouse_sensitivity_;

    // Handle first mouse movement
    if (mouse_state_.first_mouse_move) {
        delta = glm::vec2(0.0f);
        mouse_state_.first_mouse_move = false;
    }

    // Update state
    mouse_state_.last_position = mouse_state_.position;
    mouse_state_.position = new_position;

    // Generate event
    publish_event<MouseMoveEvent>(new_position, delta);
}

void GLFWInputHandler::generate_scroll_event(double xoffset, double yoffset) {
    glm::vec2 offset(static_cast<float>(xoffset), static_cast<float>(yoffset));
    offset *= scroll_sensitivity_;

    publish_event<MouseScrollEvent>(offset, mouse_state_.position);
}

void GLFWInputHandler::generate_mouse_enter_event(bool entered) {
    publish_event<MouseEnterEvent>(entered);
}

void GLFWInputHandler::generate_window_size_event(int width, int height) {
    publish_event<WindowResizeEvent>(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
}

void GLFWInputHandler::generate_window_close_event() {
    publish_event<WindowCloseEvent>();
}

void GLFWInputHandler::generate_window_focus_event(bool focused) {
    publish_event<WindowFocusEvent>(focused);
}

void GLFWInputHandler::generate_window_iconify_event(bool iconified) {
    publish_event<WindowIconifyEvent>(iconified);
}

void GLFWInputHandler::generate_window_maximize_event(bool maximized) {
    publish_event<WindowMaximizeEvent>(maximized);
}

void GLFWInputHandler::generate_window_move_event(int xpos, int ypos) {
    publish_event<WindowMoveEvent>(xpos, ypos);
}

void GLFWInputHandler::generate_window_refresh_event() {
    publish_event<WindowRefreshEvent>();
}

void GLFWInputHandler::generate_file_drop_event(int count, const char** paths) {
    std::vector<std::string> file_paths;
    file_paths.reserve(count);

    for (int i = 0; i < count; ++i) {
        file_paths.emplace_back(paths[i]);
    }

    publish_event<FileDropEvent>(std::move(file_paths));
}

void GLFWInputHandler::generate_gamepad_connection_event(int jid, bool connected) {
    if (jid < 0 || jid >= static_cast<int>(gamepads_.size())) return;

    auto& gamepad = gamepads_[jid];

    if (connected != gamepad.connected) {
        gamepad.connected = connected;

        if (connected) {
            gamepad.name = glfwGetJoystickName(jid) ? glfwGetJoystickName(jid) : "Unknown";
        } else {
            gamepad.name.clear();
            // Reset gamepad state
            gamepad = GLFWGamepadState{};
        }

        publish_event<GamepadConnectionEvent>(static_cast<std::uint32_t>(jid), connected, gamepad.name);
    }
}

// Gamepad State Management
void GLFWInputHandler::update_gamepad_states() {
    for (int jid = 0; jid < static_cast<int>(gamepads_.size()); ++jid) {
        auto& gamepad = gamepads_[jid];

        if (!gamepad.connected || !glfwJoystickPresent(jid)) continue;

        GLFWgamepadstate state;
        if (glfwGetGamepadState(jid, &state)) {
            // Update button states
            for (int i = 0; i < 15; ++i) {
                bool current = static_cast<bool>(state.buttons[i]);
                bool previous = gamepad.buttons[i];

                gamepad.buttons[i] = current;

                // Generate button events on state change
                if (current != previous) {
                    GamepadButton button = event_utils::glfw_gamepad_button_to_gamepad_button(i);
                    InputState input_state = current ? InputState::Pressed : InputState::Released;

                    publish_event<GamepadButtonEvent>(static_cast<std::uint32_t>(jid), button, input_state);

                    if (current) {
                        publish_event<GamepadButtonPressedEvent>(static_cast<std::uint32_t>(jid), button);
                    } else {
                        publish_event<GamepadButtonReleasedEvent>(static_cast<std::uint32_t>(jid), button);
                    }
                }
            }

            // Update axis states
            for (int i = 0; i < 6; ++i) {
                float current = apply_deadzone(state.axes[i], gamepad.deadzone);
                float previous = gamepad.axes[i];
                float delta = current - previous;

                gamepad.axes[i] = current;

                // Generate axis events on significant change
                if (std::abs(delta) > 0.01f) {
                    GamepadAxis axis = event_utils::glfw_gamepad_axis_to_gamepad_axis(i);
                    publish_event<GamepadAxisEvent>(static_cast<std::uint32_t>(jid), axis, current, delta);
                }
            }

            gamepad.last_update = std::chrono::high_resolution_clock::now();
        }
    }
}

void GLFWInputHandler::check_gamepad_connections() {
    for (int jid = 0; jid < static_cast<int>(gamepads_.size()); ++jid) {
        bool present = glfwJoystickPresent(jid);
        bool is_gamepad = present && glfwJoystickIsGamepad(jid);

        if (is_gamepad != gamepads_[jid].connected) {
            generate_gamepad_connection_event(jid, is_gamepad);
        }
    }
}

void GLFWInputHandler::generate_gamepad_events() {
    // This method can be used for additional gamepad event generation
    // Currently, gamepad events are generated in update_gamepad_states()
}

// Utility Methods
ModifierKey GLFWInputHandler::create_modifier_flags(int mods) const {
    ModifierKey flags = ModifierKey::None;

    if (mods & GLFW_MOD_SHIFT) flags = flags | ModifierKey::Shift;
    if (mods & GLFW_MOD_CONTROL) flags = flags | ModifierKey::Control;
    if (mods & GLFW_MOD_ALT) flags = flags | ModifierKey::Alt;
    if (mods & GLFW_MOD_SUPER) flags = flags | ModifierKey::Super;

    return flags;
}

std::uint32_t GLFWInputHandler::detect_multi_click(MouseButton button, glm::vec2 position) {
    auto& tracker = mouse_state_.click_tracker;
    auto now = std::chrono::high_resolution_clock::now();

    bool is_multi_click = false;

    if (tracker.last_button == button) {
        auto time_diff = now - tracker.last_click_time;
        float distance = glm::length(position - tracker.last_click_position);

        if (time_diff <= GLFWMouseState::ClickTracker::double_click_time &&
            distance <= GLFWMouseState::ClickTracker::double_click_distance) {
            is_multi_click = true;
        }
    }

    if (is_multi_click) {
        ++tracker.click_count;
    } else {
        tracker.click_count = 1;
    }

    tracker.last_click_time = now;
    tracker.last_button = button;
    tracker.last_click_position = position;

    return tracker.click_count;
}

float GLFWInputHandler::apply_deadzone(float value, float deadzone) const {
    if (std::abs(value) < deadzone) {
        return 0.0f;
    }

    // Smooth deadzone application
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float abs_value = std::abs(value);
    float scaled = (abs_value - deadzone) / (1.0f - deadzone);

    return sign * std::clamp(scaled, 0.0f, 1.0f);
}

// Static Handler Management
GLFWInputHandler* GLFWInputHandler::get_handler_from_window(GLFWwindow* window) {
    auto it = g_window_handlers.find(window);
    return (it != g_window_handlers.end()) ? it->second : nullptr;
}

void GLFWInputHandler::set_handler_for_window(GLFWwindow* window, GLFWInputHandler* handler) {
    if (handler) {
        g_window_handlers[window] = handler;
    } else {
        g_window_handlers.erase(window);
    }
}

// GLFWInputSystem implementation
GLFWInputSystem::GLFWInputSystem()
    : input_handler_(event_dispatcher_)
{}

GLFWInputSystem::~GLFWInputSystem() {
    shutdown();
}

bool GLFWInputSystem::initialize(GLFWwindow* window) {
    if (!window) {
        std::cerr << "GLFWInputSystem::initialize: Invalid window" << std::endl;
        return false;
    }

    window_ = window;
    input_handler_.initialize(window);
    initialized_ = true;

    std::cout << "GLFWInputSystem initialized successfully" << std::endl;
    return true;
}

void GLFWInputSystem::shutdown() {
    if (initialized_) {
        input_handler_.shutdown();
        window_ = nullptr;
        initialized_ = false;
        std::cout << "GLFWInputSystem shut down" << std::endl;
    }
}

void GLFWInputSystem::update(float delta_time) {
    if (!initialized_) return;

    // Update frame number
    event_dispatcher_.set_frame_number(++frame_number_);

    // Update input handler
    input_handler_.update_frame();
    input_handler_.poll_events();

    // Process events through dispatcher
    event_dispatcher_.process_events();
}

} // namespace lore::input