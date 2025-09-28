#pragma once

#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>

#include <memory>
#include <unordered_map>
#include <chrono>
#include <array>

struct GLFWwindow;

namespace lore::input {

// GLFW gamepad state tracking
struct GLFWGamepadState {
    bool connected = false;
    std::string name;
    std::array<bool, 15> buttons = {};
    std::array<bool, 15> buttons_previous = {};
    std::array<float, 6> axes = {};
    std::array<float, 6> axes_previous = {};
    float deadzone = 0.15f;
    std::chrono::high_resolution_clock::time_point last_update;
};

// GLFW mouse state for tracking
struct GLFWMouseState {
    glm::vec2 position{0.0f};
    glm::vec2 last_position{0.0f};
    bool first_mouse_move = true;
    std::array<bool, 8> buttons = {};
    std::array<bool, 8> buttons_previous = {};

    // Click tracking for multi-click detection
    struct ClickTracker {
        std::chrono::high_resolution_clock::time_point last_click_time;
        std::uint32_t click_count = 0;
        MouseButton last_button = MouseButton::Left;
        glm::vec2 last_click_position{0.0f};
        static constexpr auto double_click_time = std::chrono::milliseconds(500);
        static constexpr float double_click_distance = 5.0f;
    } click_tracker;
};

// GLFW input handler - bridges GLFW callbacks to event system
class GLFWInputHandler {
public:
    explicit GLFWInputHandler(EventDispatcher& dispatcher);
    ~GLFWInputHandler();

    // Non-copyable, movable
    GLFWInputHandler(const GLFWInputHandler&) = delete;
    GLFWInputHandler& operator=(const GLFWInputHandler&) = delete;
    GLFWInputHandler(GLFWInputHandler&&) = default;
    GLFWInputHandler& operator=(GLFWInputHandler&&) = default;

    // GLFW initialization
    void initialize(GLFWwindow* window);
    void shutdown();

    // Frame updates
    void update_frame();
    void poll_events();

    // Configuration
    void set_mouse_sensitivity(float sensitivity) noexcept { mouse_sensitivity_ = sensitivity; }
    float get_mouse_sensitivity() const noexcept { return mouse_sensitivity_; }

    void set_scroll_sensitivity(float sensitivity) noexcept { scroll_sensitivity_ = sensitivity; }
    float get_scroll_sensitivity() const noexcept { return scroll_sensitivity_; }

    void set_gamepad_deadzone(float deadzone) noexcept {
        for (auto& gamepad : gamepads_) {
            gamepad.deadzone = deadzone;
        }
    }

    // Cursor management
    void set_cursor_mode(bool visible, bool locked);
    void get_cursor_mode(bool& visible, bool& locked) const;

    // State queries
    bool is_key_pressed(KeyCode key) const;
    bool is_mouse_button_pressed(MouseButton button) const;
    bool is_gamepad_connected(std::uint32_t gamepad_id) const;
    glm::vec2 get_mouse_position() const { return mouse_state_.position; }

    // Statistics
    std::size_t get_events_generated_this_frame() const noexcept { return events_generated_this_frame_; }
    std::size_t get_total_events_generated() const noexcept { return total_events_generated_; }

private:
    EventDispatcher& event_dispatcher_;
    GLFWwindow* window_ = nullptr;

    // State tracking
    GLFWMouseState mouse_state_;
    std::array<GLFWGamepadState, 16> gamepads_;
    std::bitset<512> keyboard_state_;
    std::bitset<512> keyboard_state_previous_;

    // Configuration
    float mouse_sensitivity_ = 1.0f;
    float scroll_sensitivity_ = 1.0f;
    bool cursor_visible_ = true;
    bool cursor_locked_ = false;

    // Statistics
    std::size_t events_generated_this_frame_ = 0;
    std::size_t total_events_generated_ = 0;

    // GLFW callback handlers
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_char_callback(GLFWwindow* window, unsigned int codepoint);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void glfw_cursor_enter_callback(GLFWwindow* window, int entered);
    static void glfw_window_size_callback(GLFWwindow* window, int width, int height);
    static void glfw_window_close_callback(GLFWwindow* window);
    static void glfw_window_focus_callback(GLFWwindow* window, int focused);
    static void glfw_window_iconify_callback(GLFWwindow* window, int iconified);
    static void glfw_window_maximize_callback(GLFWwindow* window, int maximized);
    static void glfw_window_pos_callback(GLFWwindow* window, int xpos, int ypos);
    static void glfw_window_refresh_callback(GLFWwindow* window);
    static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths);
    static void glfw_joystick_callback(int jid, int event);

    // Internal event generation
    void generate_keyboard_event(int key, int scancode, int action, int mods);
    void generate_text_input_event(unsigned int codepoint);
    void generate_mouse_button_event(int button, int action, int mods);
    void generate_mouse_move_event(double xpos, double ypos);
    void generate_scroll_event(double xoffset, double yoffset);
    void generate_mouse_enter_event(bool entered);
    void generate_window_size_event(int width, int height);
    void generate_window_close_event();
    void generate_window_focus_event(bool focused);
    void generate_window_iconify_event(bool iconified);
    void generate_window_maximize_event(bool maximized);
    void generate_window_move_event(int xpos, int ypos);
    void generate_window_refresh_event();
    void generate_file_drop_event(int count, const char** paths);
    void generate_gamepad_connection_event(int jid, bool connected);

    // Gamepad polling
    void update_gamepad_states();
    void check_gamepad_connections();
    void generate_gamepad_events();

    // Utility functions
    ModifierKey create_modifier_flags(int mods) const;
    std::uint32_t detect_multi_click(MouseButton button, glm::vec2 position);
    float apply_deadzone(float value, float deadzone) const;

    // Static handler instance tracking
    static GLFWInputHandler* get_handler_from_window(GLFWwindow* window);
    static void set_handler_for_window(GLFWwindow* window, GLFWInputHandler* handler);

    // Helper for generating events
    template<typename EventType, typename... Args>
    void publish_event(Args&&... args);
};

// GLFW Input System - high-level management class
class GLFWInputSystem {
public:
    GLFWInputSystem();
    ~GLFWInputSystem();

    // Non-copyable, movable
    GLFWInputSystem(const GLFWInputSystem&) = delete;
    GLFWInputSystem& operator=(const GLFWInputSystem&) = delete;
    GLFWInputSystem(GLFWInputSystem&&) = default;
    GLFWInputSystem& operator=(GLFWInputSystem&&) = default;

    // System lifecycle
    bool initialize(GLFWwindow* window);
    void shutdown();
    void update(float delta_time);

    // Event system access
    EventDispatcher& get_event_dispatcher() { return event_dispatcher_; }
    const EventDispatcher& get_event_dispatcher() const { return event_dispatcher_; }

    // Input handler access
    GLFWInputHandler& get_input_handler() { return input_handler_; }
    const GLFWInputHandler& get_input_handler() const { return input_handler_; }

    // Configuration
    void set_max_events_per_frame(std::size_t max) { event_dispatcher_.set_max_events_per_frame(max); }
    void set_debug_logging(bool enabled) { event_dispatcher_.set_debug_logging(enabled); }

    // Statistics
    EventDispatcher::Statistics get_statistics() const { return event_dispatcher_.get_statistics(); }

    // Event subscription helpers
    template<typename EventType>
    ListenerHandle subscribe(std::function<void(const EventType&)> handler,
                           EventPriority priority = EventPriority::Normal) {
        return event_dispatcher_.subscribe<EventType>(std::move(handler), priority);
    }

    // Event publishing helpers
    template<typename EventType, typename... Args>
    void publish(Args&&... args) {
        event_dispatcher_.publish<EventType>(std::forward<Args>(args)...);
    }

private:
    EventDispatcher event_dispatcher_;
    GLFWInputHandler input_handler_;
    GLFWwindow* window_ = nullptr;
    std::uint64_t frame_number_ = 0;
    bool initialized_ = false;
};

// Template implementations

template<typename EventType, typename... Args>
void GLFWInputHandler::publish_event(Args&&... args) {
    event_dispatcher_.publish<EventType>(std::forward<Args>(args)...);
    ++events_generated_this_frame_;
    ++total_events_generated_;
}

} // namespace lore::input