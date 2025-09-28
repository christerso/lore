#pragma once

#include <lore/ecs/ecs.hpp>

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>
#include <bitset>
#include <functional>
#include <chrono>
#include <array>
#include <memory>

struct GLFWwindow;

namespace lore::input {

// Forward declarations
class InputSystem;
class EventQueue;
class InputStateManager;
class ActionMapper;
struct InputEvent;

// Maximum supported input devices
constexpr std::size_t MAX_GAMEPADS = 16;
constexpr std::size_t MAX_KEYS = 512;
constexpr std::size_t MAX_MOUSE_BUTTONS = 8;

// Input device types
enum class InputDevice : std::uint8_t {
    Keyboard,
    Mouse,
    Gamepad
};

// Key codes (aligned with GLFW)
enum class Key : std::uint16_t {
    Unknown = 0,

    // Printable keys
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,

    // Numbers
    Key0 = 48, Key1 = 49, Key2 = 50, Key3 = 51, Key4 = 52,
    Key5 = 53, Key6 = 54, Key7 = 55, Key8 = 56, Key9 = 57,

    // Punctuation
    Semicolon = 59,
    Equal = 61,

    // Letters
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
    K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84,
    U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,

    // Brackets
    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,

    // Function keys
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259,
    Insert = 260, Delete = 261,
    Right = 262, Left = 263, Down = 264, Up = 265,
    PageUp = 266, PageDown = 267,
    Home = 268, End = 269,
    CapsLock = 280, ScrollLock = 281, NumLock = 282,
    PrintScreen = 283, Pause = 284,

    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    F13 = 302, F14 = 303, F15 = 304, F16 = 305, F17 = 306, F18 = 307,
    F19 = 308, F20 = 309, F21 = 310, F22 = 311, F23 = 312, F24 = 313, F25 = 314,

    // Keypad
    Kp0 = 320, Kp1 = 321, Kp2 = 322, Kp3 = 323, Kp4 = 324,
    Kp5 = 325, Kp6 = 326, Kp7 = 327, Kp8 = 328, Kp9 = 329,
    KpDecimal = 330, KpDivide = 331, KpMultiply = 332,
    KpSubtract = 333, KpAdd = 334, KpEnter = 335, KpEqual = 336,

    // Modifiers
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
    RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347,
    Menu = 348
};

// Mouse button codes
enum class MouseButton : std::uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7
};

// Gamepad button codes (Xbox controller layout)
enum class GamepadButton : std::uint8_t {
    A = 0,           // Cross on PlayStation
    B = 1,           // Circle on PlayStation
    X = 2,           // Square on PlayStation
    Y = 3,           // Triangle on PlayStation
    LeftBumper = 4,
    RightBumper = 5,
    Back = 6,        // Select/Share
    Start = 7,       // Options
    Guide = 8,       // Home/PS button
    LeftThumb = 9,   // Left stick press
    RightThumb = 10, // Right stick press
    DpadUp = 11,
    DpadRight = 12,
    DpadDown = 13,
    DpadLeft = 14
};

// Gamepad axis codes
enum class GamepadAxis : std::uint8_t {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5
};

// Input action for mapping
enum class InputAction : std::uint32_t {
    None = 0,

    // Movement actions
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Crouch,
    Sprint,

    // Camera actions
    LookUp,
    LookDown,
    LookLeft,
    LookRight,

    // Interaction actions
    Interact,
    Attack,
    SecondaryAttack,
    Block,
    Reload,

    // UI actions
    MenuToggle,
    Inventory,
    Map,
    Accept,
    Cancel,

    // Custom actions start here
    CustomActionStart = 1000
};

// Input state
enum class InputState : std::uint8_t {
    Released = 0,
    Pressed = 1,
    Held = 2
};

// Event types
enum class EventType : std::uint8_t {
    KeyPressed,
    KeyReleased,
    KeyRepeated,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadAxisMoved,
    GamepadConnected,
    GamepadDisconnected,
    WindowFocusChanged,
    CursorModeChanged
};

// Base input event
struct InputEvent {
    EventType type;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::uint32_t frame_number;

    InputEvent(EventType t) : type(t), timestamp(std::chrono::high_resolution_clock::now()), frame_number(0) {}
    virtual ~InputEvent() = default;
};

// Keyboard events
struct KeyEvent : public InputEvent {
    Key key;
    std::uint32_t scancode;
    bool alt, ctrl, shift, super;

    KeyEvent(EventType type, Key k, std::uint32_t sc, bool a, bool c, bool s, bool su)
        : InputEvent(type), key(k), scancode(sc), alt(a), ctrl(c), shift(s), super(su) {}
};

// Mouse button events
struct MouseButtonEvent : public InputEvent {
    MouseButton button;
    glm::vec2 position;
    bool alt, ctrl, shift, super;

    MouseButtonEvent(EventType type, MouseButton b, glm::vec2 pos, bool a, bool c, bool s, bool su)
        : InputEvent(type), button(b), position(pos), alt(a), ctrl(c), shift(s), super(su) {}
};

// Mouse movement event
struct MouseMoveEvent : public InputEvent {
    glm::vec2 position;
    glm::vec2 delta;

    MouseMoveEvent(glm::vec2 pos, glm::vec2 d) : InputEvent(EventType::MouseMoved), position(pos), delta(d) {}
};

// Mouse scroll event
struct MouseScrollEvent : public InputEvent {
    glm::vec2 offset;

    MouseScrollEvent(glm::vec2 off) : InputEvent(EventType::MouseScrolled), offset(off) {}
};

// Gamepad button events
struct GamepadButtonEvent : public InputEvent {
    std::uint32_t gamepad_id;
    GamepadButton button;

    GamepadButtonEvent(EventType type, std::uint32_t id, GamepadButton b)
        : InputEvent(type), gamepad_id(id), button(b) {}
};

// Gamepad axis event
struct GamepadAxisEvent : public InputEvent {
    std::uint32_t gamepad_id;
    GamepadAxis axis;
    float value;
    float delta;

    GamepadAxisEvent(std::uint32_t id, GamepadAxis a, float v, float d)
        : InputEvent(EventType::GamepadAxisMoved), gamepad_id(id), axis(a), value(v), delta(d) {}
};

// Gamepad connection events
struct GamepadConnectionEvent : public InputEvent {
    std::uint32_t gamepad_id;
    std::string name;

    GamepadConnectionEvent(EventType type, std::uint32_t id, const std::string& n)
        : InputEvent(type), gamepad_id(id), name(n) {}
};

// Window focus event
struct WindowFocusEvent : public InputEvent {
    bool focused;

    WindowFocusEvent(bool f) : InputEvent(EventType::WindowFocusChanged), focused(f) {}
};

// Cursor mode event
struct CursorModeEvent : public InputEvent {
    bool visible;
    bool captured;

    CursorModeEvent(bool v, bool c) : InputEvent(EventType::CursorModeChanged), visible(v), captured(c) {}
};

// Input binding for action mapping
struct InputBinding {
    InputDevice device;

    union {
        Key key;
        MouseButton mouse_button;
        struct {
            GamepadButton button;
            std::uint32_t gamepad_id;
        } gamepad_button;
        struct {
            GamepadAxis axis;
            std::uint32_t gamepad_id;
            float threshold;
            bool positive_direction;
        } gamepad_axis;
    };

    InputBinding() : device(InputDevice::Keyboard), key(Key::Unknown) {}

    static InputBinding keyboard(Key k) {
        InputBinding binding;
        binding.device = InputDevice::Keyboard;
        binding.key = k;
        return binding;
    }

    static InputBinding mouse(MouseButton mb) {
        InputBinding binding;
        binding.device = InputDevice::Mouse;
        binding.mouse_button = mb;
        return binding;
    }

    static InputBinding gamepad_btn(GamepadButton gb, std::uint32_t id = 0) {
        InputBinding binding;
        binding.device = InputDevice::Gamepad;
        binding.gamepad_button.button = gb;
        binding.gamepad_button.gamepad_id = id;
        return binding;
    }

    static InputBinding gamepad_stick(GamepadAxis ga, float threshold = 0.5f, bool positive = true, std::uint32_t id = 0) {
        InputBinding binding;
        binding.device = InputDevice::Gamepad;
        binding.gamepad_axis.axis = ga;
        binding.gamepad_axis.gamepad_id = id;
        binding.gamepad_axis.threshold = threshold;
        binding.gamepad_axis.positive_direction = positive;
        return binding;
    }
};

// Action configuration
struct ActionConfig {
    InputAction action;
    std::vector<InputBinding> bindings;
    float deadzone = 0.0f;
    float sensitivity = 1.0f;
    bool invert = false;
    std::string description;
};

// Input context for grouping actions
struct InputContext {
    std::string name;
    std::vector<ActionConfig> actions;
    std::uint32_t priority = 0;
    bool active = true;
};

// Gamepad state
struct GamepadState {
    bool connected = false;
    std::string name;
    std::array<bool, 15> buttons = {};
    std::array<bool, 15> buttons_previous = {};
    std::array<float, 6> axes = {};
    std::array<float, 6> axes_previous = {};
    float deadzone = 0.15f;
    std::chrono::high_resolution_clock::time_point last_update;
};

// Event callback function types
using EventCallback = std::function<void(const InputEvent&)>;
using KeyCallback = std::function<void(const KeyEvent&)>;
using MouseButtonCallback = std::function<void(const MouseButtonEvent&)>;
using MouseMoveCallback = std::function<void(const MouseMoveEvent&)>;
using MouseScrollCallback = std::function<void(const MouseScrollEvent&)>;
using GamepadCallback = std::function<void(const GamepadButtonEvent&)>;
using ActionCallback = std::function<void(InputAction, float)>;

// Thread-safe event queue
class EventQueue {
public:
    EventQueue();
    ~EventQueue();

    // Event submission
    void push_event(std::unique_ptr<InputEvent> event);

    // Event consumption
    std::vector<std::unique_ptr<InputEvent>> poll_events();
    void clear();

    // Statistics
    std::size_t size() const;
    std::size_t capacity() const;
    void reserve(std::size_t count);

private:
    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<InputEvent>> events_;
    std::size_t max_events_ = 1000;
};

// Input state management
class InputStateManager {
public:
    InputStateManager();
    ~InputStateManager() = default;

    // Frame management
    void begin_frame();
    void end_frame();
    std::uint32_t get_frame_number() const noexcept { return frame_number_; }

    // Keyboard state
    bool is_key_pressed(Key key) const;
    bool is_key_held(Key key) const;
    bool is_key_released(Key key) const;
    bool was_key_pressed_this_frame(Key key) const;
    bool was_key_released_this_frame(Key key) const;

    // Mouse state
    bool is_mouse_button_pressed(MouseButton button) const;
    bool is_mouse_button_held(MouseButton button) const;
    bool is_mouse_button_released(MouseButton button) const;
    bool was_mouse_button_pressed_this_frame(MouseButton button) const;
    bool was_mouse_button_released_this_frame(MouseButton button) const;

    glm::vec2 get_mouse_position() const noexcept { return mouse_position_; }
    glm::vec2 get_mouse_delta() const noexcept { return mouse_delta_; }
    glm::vec2 get_scroll_delta() const noexcept { return scroll_delta_; }

    // Gamepad state
    bool is_gamepad_connected(std::uint32_t gamepad_id) const;
    bool is_gamepad_button_pressed(std::uint32_t gamepad_id, GamepadButton button) const;
    bool is_gamepad_button_held(std::uint32_t gamepad_id, GamepadButton button) const;
    bool is_gamepad_button_released(std::uint32_t gamepad_id, GamepadButton button) const;
    bool was_gamepad_button_pressed_this_frame(std::uint32_t gamepad_id, GamepadButton button) const;
    bool was_gamepad_button_released_this_frame(std::uint32_t gamepad_id, GamepadButton button) const;

    float get_gamepad_axis(std::uint32_t gamepad_id, GamepadAxis axis) const;
    float get_gamepad_axis_delta(std::uint32_t gamepad_id, GamepadAxis axis) const;
    void set_gamepad_deadzone(std::uint32_t gamepad_id, float deadzone);

    const GamepadState& get_gamepad_state(std::uint32_t gamepad_id) const;
    std::vector<std::uint32_t> get_connected_gamepads() const;

    // State updates from events
    void update_key_state(Key key, InputState state);
    void update_mouse_button_state(MouseButton button, InputState state);
    void update_mouse_position(glm::vec2 position, glm::vec2 delta);
    void update_scroll(glm::vec2 delta);
    void update_gamepad_button_state(std::uint32_t gamepad_id, GamepadButton button, InputState state);
    void update_gamepad_axis_state(std::uint32_t gamepad_id, GamepadAxis axis, float value);
    void update_gamepad_connection(std::uint32_t gamepad_id, bool connected, const std::string& name = "");

private:
    std::uint32_t frame_number_ = 0;

    // Keyboard state
    std::bitset<MAX_KEYS> keys_current_;
    std::bitset<MAX_KEYS> keys_previous_;
    std::bitset<MAX_KEYS> keys_pressed_this_frame_;
    std::bitset<MAX_KEYS> keys_released_this_frame_;

    // Mouse state
    std::bitset<MAX_MOUSE_BUTTONS> mouse_buttons_current_;
    std::bitset<MAX_MOUSE_BUTTONS> mouse_buttons_previous_;
    std::bitset<MAX_MOUSE_BUTTONS> mouse_buttons_pressed_this_frame_;
    std::bitset<MAX_MOUSE_BUTTONS> mouse_buttons_released_this_frame_;
    glm::vec2 mouse_position_{0.0f};
    glm::vec2 mouse_delta_{0.0f};
    glm::vec2 scroll_delta_{0.0f};

    // Gamepad state
    std::array<GamepadState, MAX_GAMEPADS> gamepads_;
    std::bitset<MAX_GAMEPADS> gamepads_pressed_this_frame_[15];  // 15 buttons max
    std::bitset<MAX_GAMEPADS> gamepads_released_this_frame_[15];
};

// Action mapping system
class ActionMapper {
public:
    ActionMapper();
    ~ActionMapper() = default;

    // Context management
    void add_context(const InputContext& context);
    void remove_context(const std::string& name);
    void set_context_active(const std::string& name, bool active);
    void set_context_priority(const std::string& name, std::uint32_t priority);
    bool is_context_active(const std::string& name) const;

    // Action binding
    void bind_action(const std::string& context_name, InputAction action, const InputBinding& binding);
    void unbind_action(const std::string& context_name, InputAction action, const InputBinding& binding);
    void clear_action_bindings(const std::string& context_name, InputAction action);

    // Action queries
    bool is_action_active(InputAction action) const;
    float get_action_value(InputAction action) const;
    bool was_action_triggered_this_frame(InputAction action) const;
    bool was_action_released_this_frame(InputAction action) const;

    // Process input state and generate action values
    void update(const InputStateManager& state_manager);

    // Configuration
    void load_config(const std::string& config_data);
    std::string save_config() const;
    void reset_to_defaults();

    // Callbacks
    void set_action_callback(InputAction action, ActionCallback callback);
    void remove_action_callback(InputAction action);

private:
    struct ActionState {
        float value = 0.0f;
        float previous_value = 0.0f;
        bool triggered_this_frame = false;
        bool released_this_frame = false;
    };

    std::vector<InputContext> contexts_;
    std::unordered_map<InputAction, ActionState> action_states_;
    std::unordered_map<InputAction, ActionCallback> action_callbacks_;

    float evaluate_binding(const InputBinding& binding, const InputStateManager& state_manager) const;
    void update_action_state(InputAction action, float value);
};

// ECS components for input
struct InputComponent {
    bool process_input = true;
    std::vector<InputAction> actions;
    std::unordered_map<InputAction, std::function<void(float)>> action_handlers;
};

struct MouseFollowerComponent {
    glm::vec2 offset{0.0f};
    float smoothing = 1.0f;
    bool active = true;
};

// Input configuration asset
struct InputConfigAsset {
    std::vector<InputContext> contexts;
    std::unordered_map<std::string, float> settings;
    std::uint32_t version = 1;
};

// Main input system
class InputSystem : public lore::ecs::System {
public:
    InputSystem();
    ~InputSystem();

    // System lifecycle
    void init(lore::ecs::World& world) override;
    void update(lore::ecs::World& world, float delta_time) override;
    void shutdown(lore::ecs::World& world) override;

    // GLFW integration
    void initialize_glfw(GLFWwindow* window);
    void poll_glfw_events();

    // Event system
    EventQueue& get_event_queue() { return event_queue_; }
    const EventQueue& get_event_queue() const { return event_queue_; }

    // State management
    InputStateManager& get_state_manager() { return state_manager_; }
    const InputStateManager& get_state_manager() const { return state_manager_; }

    // Action mapping
    ActionMapper& get_action_mapper() { return action_mapper_; }
    const ActionMapper& get_action_mapper() const { return action_mapper_; }

    // Event callbacks
    void set_event_callback(EventCallback callback) { event_callback_ = std::move(callback); }
    void set_key_callback(KeyCallback callback) { key_callback_ = std::move(callback); }
    void set_mouse_button_callback(MouseButtonCallback callback) { mouse_button_callback_ = std::move(callback); }
    void set_mouse_move_callback(MouseMoveCallback callback) { mouse_move_callback_ = std::move(callback); }
    void set_mouse_scroll_callback(MouseScrollCallback callback) { mouse_scroll_callback_ = std::move(callback); }
    void set_gamepad_callback(GamepadCallback callback) { gamepad_callback_ = std::move(callback); }

    // Cursor management
    void set_cursor_mode(bool visible, bool captured);
    void get_cursor_mode(bool& visible, bool& captured) const;

    // Configuration
    void load_input_config(const std::string& config_path);
    void save_input_config(const std::string& config_path) const;

    // Statistics
    std::size_t get_events_processed_this_frame() const { return events_processed_this_frame_; }
    std::size_t get_total_events_processed() const { return total_events_processed_; }

private:
    GLFWwindow* window_ = nullptr;
    EventQueue event_queue_;
    InputStateManager state_manager_;
    ActionMapper action_mapper_;

    // Callbacks
    EventCallback event_callback_;
    KeyCallback key_callback_;
    MouseButtonCallback mouse_button_callback_;
    MouseMoveCallback mouse_move_callback_;
    MouseScrollCallback mouse_scroll_callback_;
    GamepadCallback gamepad_callback_;

    // Frame statistics
    std::size_t events_processed_this_frame_ = 0;
    std::size_t total_events_processed_ = 0;

    // Cursor state
    bool cursor_visible_ = true;
    bool cursor_captured_ = false;

    // GLFW callbacks
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void glfw_joystick_callback(int jid, int event);
    static void glfw_window_focus_callback(GLFWwindow* window, int focused);

    // Internal event processing
    void process_events();
    void update_gamepad_states();
    void invoke_callbacks(const InputEvent& event);

    // Gamepad support
    void initialize_gamepad_database();
    bool is_gamepad_supported(int jid) const;
    std::string get_gamepad_name(int jid) const;
};

// Input utility functions
namespace util {
    std::string key_to_string(Key key);
    Key string_to_key(const std::string& str);
    std::string mouse_button_to_string(MouseButton button);
    MouseButton string_to_mouse_button(const std::string& str);
    std::string gamepad_button_to_string(GamepadButton button);
    GamepadButton string_to_gamepad_button(const std::string& str);
    std::string gamepad_axis_to_string(GamepadAxis axis);
    GamepadAxis string_to_gamepad_axis(const std::string& str);
    std::string action_to_string(InputAction action);
    InputAction string_to_action(const std::string& str);

    bool is_modifier_key(Key key);
    bool is_function_key(Key key);
    bool is_arrow_key(Key key);
    bool is_number_key(Key key);
    bool is_letter_key(Key key);

    float apply_deadzone(float value, float deadzone);
    glm::vec2 apply_circular_deadzone(glm::vec2 input, float deadzone);
}

} // namespace lore::input