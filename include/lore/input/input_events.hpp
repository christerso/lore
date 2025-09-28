#pragma once

#include <lore/input/event_system.hpp>
#include <lore/math/math.hpp>

#include <string>
#include <array>
#include <bitset>

namespace lore::input {

// Input device enumeration
enum class InputDevice : std::uint8_t {
    Keyboard,
    Mouse,
    Gamepad,
    Touch,      // For future mobile support
    Unknown
};

// Key codes (aligned with GLFW for compatibility)
enum class KeyCode : std::uint16_t {
    Unknown = 0,

    // Printable keys
    Space = 32,
    Apostrophe = 39,    // '
    Comma = 44,         // ,
    Minus = 45,         // -
    Period = 46,        // .
    Slash = 47,         // /

    // Numbers
    Digit0 = 48, Digit1 = 49, Digit2 = 50, Digit3 = 51, Digit4 = 52,
    Digit5 = 53, Digit6 = 54, Digit7 = 55, Digit8 = 56, Digit9 = 57,

    // Punctuation
    Semicolon = 59,     // ;
    Equal = 61,         // =

    // Letters (uppercase values, but represents both cases)
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
    K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84,
    U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,

    // Brackets
    LeftBracket = 91,   // [
    Backslash = 92,     // \
    RightBracket = 93,  // ]
    GraveAccent = 96,   // `

    // Control keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,

    // Arrow keys
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,

    // Page navigation
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,

    // Lock keys
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    // Function keys
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    F13 = 302, F14 = 303, F15 = 304, F16 = 305, F17 = 306, F18 = 307,
    F19 = 308, F20 = 309, F21 = 310, F22 = 311, F23 = 312, F24 = 313, F25 = 314,

    // Keypad
    Kp0 = 320, Kp1 = 321, Kp2 = 322, Kp3 = 323, Kp4 = 324,
    Kp5 = 325, Kp6 = 326, Kp7 = 327, Kp8 = 328, Kp9 = 329,
    KpDecimal = 330,
    KpDivide = 331,
    KpMultiply = 332,
    KpSubtract = 333,
    KpAdd = 334,
    KpEnter = 335,
    KpEqual = 336,

    // Modifier keys
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348
};

// Mouse button enumeration
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

// Gamepad button enumeration (standardized layout)
enum class GamepadButton : std::uint8_t {
    // Face buttons (Xbox naming, PlayStation equivalents in comments)
    A = 0,              // Cross
    B = 1,              // Circle
    X = 2,              // Square
    Y = 3,              // Triangle

    // Shoulder buttons
    LeftBumper = 4,     // L1
    RightBumper = 5,    // R1

    // System buttons
    Back = 6,           // Select/Share
    Start = 7,          // Options
    Guide = 8,          // Home/PS button

    // Stick buttons
    LeftStick = 9,      // L3
    RightStick = 10,    // R3

    // D-pad
    DpadUp = 11,
    DpadRight = 12,
    DpadDown = 13,
    DpadLeft = 14,

    // Additional buttons for extended controllers
    Paddle1 = 15,
    Paddle2 = 16,
    Paddle3 = 17,
    Paddle4 = 18
};

// Gamepad axis enumeration
enum class GamepadAxis : std::uint8_t {
    LeftStickX = 0,
    LeftStickY = 1,
    RightStickX = 2,
    RightStickY = 3,
    LeftTrigger = 4,
    RightTrigger = 5
};

// Input action state
enum class InputState : std::uint8_t {
    Released = 0,
    Pressed = 1,
    Repeated = 2        // For key repeat events
};

// Modifier key flags
enum class ModifierKey : std::uint8_t {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3
};

inline ModifierKey operator|(ModifierKey a, ModifierKey b) {
    return static_cast<ModifierKey>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline ModifierKey operator&(ModifierKey a, ModifierKey b) {
    return static_cast<ModifierKey>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline bool has_modifier(ModifierKey flags, ModifierKey test) {
    return (flags & test) == test;
}

// Keyboard Events
class KeyboardEvent : public Event<KeyboardEvent> {
public:
    KeyCode key;
    std::uint32_t scancode;
    InputState state;
    ModifierKey modifiers;

    KeyboardEvent(KeyCode k, std::uint32_t sc, InputState s, ModifierKey mods)
        : key(k), scancode(sc), state(s), modifiers(mods) {}

    EventPriority get_priority() const noexcept override {
        // Function keys and escape get higher priority
        if (key >= KeyCode::F1 && key <= KeyCode::F25) return EventPriority::High;
        if (key == KeyCode::Escape) return EventPriority::High;
        return EventPriority::Normal;
    }

    std::string to_string() const override {
        return "KeyboardEvent(key=" + std::to_string(static_cast<int>(key)) +
               ", state=" + std::to_string(static_cast<int>(state)) + ")";
    }
};

// Specialized keyboard events
class KeyPressedEvent : public Event<KeyPressedEvent> {
public:
    KeyCode key;
    std::uint32_t scancode;
    ModifierKey modifiers;
    bool is_repeat;

    KeyPressedEvent(KeyCode k, std::uint32_t sc, ModifierKey mods, bool repeat = false)
        : key(k), scancode(sc), modifiers(mods), is_repeat(repeat) {}

    EventPriority get_priority() const noexcept override {
        return is_repeat ? EventPriority::Low : EventPriority::Normal;
    }

    std::string to_string() const override {
        return "KeyPressedEvent(key=" + std::to_string(static_cast<int>(key)) +
               (is_repeat ? ", repeat=true" : "") + ")";
    }
};

class KeyReleasedEvent : public Event<KeyReleasedEvent> {
public:
    KeyCode key;
    std::uint32_t scancode;
    ModifierKey modifiers;

    KeyReleasedEvent(KeyCode k, std::uint32_t sc, ModifierKey mods)
        : key(k), scancode(sc), modifiers(mods) {}

    std::string to_string() const override {
        return "KeyReleasedEvent(key=" + std::to_string(static_cast<int>(key)) + ")";
    }
};

class TextInputEvent : public Event<TextInputEvent> {
public:
    std::string text;
    std::uint32_t codepoint;

    TextInputEvent(std::string txt, std::uint32_t cp)
        : text(std::move(txt)), codepoint(cp) {}

    std::string to_string() const override {
        return "TextInputEvent(text=\"" + text + "\")";
    }
};

// Mouse Events
class MouseButtonEvent : public Event<MouseButtonEvent> {
public:
    MouseButton button;
    InputState state;
    glm::vec2 position;
    ModifierKey modifiers;

    MouseButtonEvent(MouseButton b, InputState s, glm::vec2 pos, ModifierKey mods)
        : button(b), state(s), position(pos), modifiers(mods) {}

    EventPriority get_priority() const noexcept override {
        // Right-clicks might be context menus, give them priority
        return button == MouseButton::Right ? EventPriority::High : EventPriority::Normal;
    }

    std::string to_string() const override {
        return "MouseButtonEvent(button=" + std::to_string(static_cast<int>(button)) +
               ", state=" + std::to_string(static_cast<int>(state)) +
               ", pos=(" + std::to_string(position.x) + "," + std::to_string(position.y) + "))";
    }
};

class MouseButtonPressedEvent : public Event<MouseButtonPressedEvent> {
public:
    MouseButton button;
    glm::vec2 position;
    ModifierKey modifiers;
    std::uint32_t click_count;  // For double/triple click detection

    MouseButtonPressedEvent(MouseButton b, glm::vec2 pos, ModifierKey mods, std::uint32_t clicks = 1)
        : button(b), position(pos), modifiers(mods), click_count(clicks) {}

    EventPriority get_priority() const noexcept override {
        return click_count > 1 ? EventPriority::High : EventPriority::Normal;
    }

    std::string to_string() const override {
        return "MouseButtonPressedEvent(button=" + std::to_string(static_cast<int>(button)) +
               ", clicks=" + std::to_string(click_count) + ")";
    }
};

class MouseButtonReleasedEvent : public Event<MouseButtonReleasedEvent> {
public:
    MouseButton button;
    glm::vec2 position;
    ModifierKey modifiers;

    MouseButtonReleasedEvent(MouseButton b, glm::vec2 pos, ModifierKey mods)
        : button(b), position(pos), modifiers(mods) {}

    std::string to_string() const override {
        return "MouseButtonReleasedEvent(button=" + std::to_string(static_cast<int>(button)) + ")";
    }
};

class MouseMoveEvent : public Event<MouseMoveEvent> {
public:
    glm::vec2 position;
    glm::vec2 delta;
    ModifierKey modifiers;

    MouseMoveEvent(glm::vec2 pos, glm::vec2 dlt, ModifierKey mods = ModifierKey::None)
        : position(pos), delta(dlt), modifiers(mods) {}

    EventPriority get_priority() const noexcept override {
        // Mouse moves are frequent, give them lower priority
        return EventPriority::Low;
    }

    std::string to_string() const override {
        return "MouseMoveEvent(pos=(" + std::to_string(position.x) + "," + std::to_string(position.y) +
               "), delta=(" + std::to_string(delta.x) + "," + std::to_string(delta.y) + "))";
    }
};

class MouseScrollEvent : public Event<MouseScrollEvent> {
public:
    glm::vec2 offset;
    glm::vec2 position;
    ModifierKey modifiers;

    MouseScrollEvent(glm::vec2 off, glm::vec2 pos, ModifierKey mods = ModifierKey::None)
        : offset(off), position(pos), modifiers(mods) {}

    std::string to_string() const override {
        return "MouseScrollEvent(offset=(" + std::to_string(offset.x) + "," + std::to_string(offset.y) + "))";
    }
};

class MouseEnterEvent : public Event<MouseEnterEvent> {
public:
    bool entered;   // true for enter, false for leave

    explicit MouseEnterEvent(bool enter) : entered(enter) {}

    std::string to_string() const override {
        return entered ? "MouseEnterEvent(entered)" : "MouseEnterEvent(left)";
    }
};

// Gamepad Events
class GamepadButtonEvent : public Event<GamepadButtonEvent> {
public:
    std::uint32_t gamepad_id;
    GamepadButton button;
    InputState state;

    GamepadButtonEvent(std::uint32_t id, GamepadButton b, InputState s)
        : gamepad_id(id), button(b), state(s) {}

    std::string to_string() const override {
        return "GamepadButtonEvent(gamepad=" + std::to_string(gamepad_id) +
               ", button=" + std::to_string(static_cast<int>(button)) +
               ", state=" + std::to_string(static_cast<int>(state)) + ")";
    }
};

class GamepadButtonPressedEvent : public Event<GamepadButtonPressedEvent> {
public:
    std::uint32_t gamepad_id;
    GamepadButton button;

    GamepadButtonPressedEvent(std::uint32_t id, GamepadButton b)
        : gamepad_id(id), button(b) {}

    std::string to_string() const override {
        return "GamepadButtonPressedEvent(gamepad=" + std::to_string(gamepad_id) +
               ", button=" + std::to_string(static_cast<int>(button)) + ")";
    }
};

class GamepadButtonReleasedEvent : public Event<GamepadButtonReleasedEvent> {
public:
    std::uint32_t gamepad_id;
    GamepadButton button;

    GamepadButtonReleasedEvent(std::uint32_t id, GamepadButton b)
        : gamepad_id(id), button(b) {}

    std::string to_string() const override {
        return "GamepadButtonReleasedEvent(gamepad=" + std::to_string(gamepad_id) +
               ", button=" + std::to_string(static_cast<int>(button)) + ")";
    }
};

class GamepadAxisEvent : public Event<GamepadAxisEvent> {
public:
    std::uint32_t gamepad_id;
    GamepadAxis axis;
    float value;
    float delta;

    GamepadAxisEvent(std::uint32_t id, GamepadAxis a, float val, float dlt)
        : gamepad_id(id), axis(a), value(val), delta(dlt) {}

    EventPriority get_priority() const noexcept override {
        // Trigger events might be more important than stick movements
        return (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger) ?
            EventPriority::Normal : EventPriority::Low;
    }

    std::string to_string() const override {
        return "GamepadAxisEvent(gamepad=" + std::to_string(gamepad_id) +
               ", axis=" + std::to_string(static_cast<int>(axis)) +
               ", value=" + std::to_string(value) + ")";
    }
};

class GamepadConnectionEvent : public Event<GamepadConnectionEvent> {
public:
    std::uint32_t gamepad_id;
    bool connected;
    std::string name;

    GamepadConnectionEvent(std::uint32_t id, bool conn, std::string nm = "")
        : gamepad_id(id), connected(conn), name(std::move(nm)) {}

    EventPriority get_priority() const noexcept override {
        return EventPriority::High;  // Connection events are important
    }

    std::string to_string() const override {
        return "GamepadConnectionEvent(gamepad=" + std::to_string(gamepad_id) +
               ", " + (connected ? "connected" : "disconnected") +
               (name.empty() ? "" : ", name=\"" + name + "\"") + ")";
    }
};

// Window Events
class WindowResizeEvent : public Event<WindowResizeEvent> {
public:
    std::uint32_t width;
    std::uint32_t height;

    WindowResizeEvent(std::uint32_t w, std::uint32_t h) : width(w), height(h) {}

    EventPriority get_priority() const noexcept override {
        return EventPriority::High;  // Window events are important for rendering
    }

    std::string to_string() const override {
        return "WindowResizeEvent(" + std::to_string(width) + "x" + std::to_string(height) + ")";
    }
};

class WindowCloseEvent : public Event<WindowCloseEvent> {
public:
    WindowCloseEvent() = default;

    EventPriority get_priority() const noexcept override {
        return EventPriority::Highest;  // Close events must be handled immediately
    }

    std::string to_string() const override {
        return "WindowCloseEvent()";
    }
};

class WindowFocusEvent : public Event<WindowFocusEvent> {
public:
    bool focused;

    explicit WindowFocusEvent(bool focus) : focused(focus) {}

    EventPriority get_priority() const noexcept override {
        return EventPriority::High;
    }

    std::string to_string() const override {
        return focused ? "WindowFocusEvent(focused)" : "WindowFocusEvent(unfocused)";
    }
};

class WindowIconifyEvent : public Event<WindowIconifyEvent> {
public:
    bool iconified;

    explicit WindowIconifyEvent(bool icon) : iconified(icon) {}

    std::string to_string() const override {
        return iconified ? "WindowIconifyEvent(iconified)" : "WindowIconifyEvent(restored)";
    }
};

class WindowMaximizeEvent : public Event<WindowMaximizeEvent> {
public:
    bool maximized;

    explicit WindowMaximizeEvent(bool max) : maximized(max) {}

    std::string to_string() const override {
        return maximized ? "WindowMaximizeEvent(maximized)" : "WindowMaximizeEvent(restored)";
    }
};

class WindowMoveEvent : public Event<WindowMoveEvent> {
public:
    std::int32_t x;
    std::int32_t y;

    WindowMoveEvent(std::int32_t x_pos, std::int32_t y_pos) : x(x_pos), y(y_pos) {}

    std::string to_string() const override {
        return "WindowMoveEvent(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

class WindowRefreshEvent : public Event<WindowRefreshEvent> {
public:
    WindowRefreshEvent() = default;

    EventPriority get_priority() const noexcept override {
        return EventPriority::High;  // Refresh requests need immediate attention
    }

    std::string to_string() const override {
        return "WindowRefreshEvent()";
    }
};

// File Drop Events
class FileDropEvent : public Event<FileDropEvent> {
public:
    std::vector<std::string> paths;

    explicit FileDropEvent(std::vector<std::string> file_paths)
        : paths(std::move(file_paths)) {}

    EventPriority get_priority() const noexcept override {
        return EventPriority::High;
    }

    std::string to_string() const override {
        std::string result = "FileDropEvent(" + std::to_string(paths.size()) + " files: ";
        for (const auto& path : paths) {
            result += "\"" + path + "\" ";
        }
        result += ")";
        return result;
    }
};

// Input Action Events (for higher-level game actions)
enum class InputAction : std::uint32_t {
    None = 0,

    // Movement
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Jump,
    Crouch,
    Sprint,
    Walk,

    // Camera
    LookUp,
    LookDown,
    LookLeft,
    LookRight,
    CameraZoomIn,
    CameraZoomOut,

    // Combat
    Attack,
    SecondaryAttack,
    Block,
    Parry,
    Dodge,
    Reload,
    Aim,

    // Interaction
    Interact,
    Use,
    PickUp,
    Drop,
    Examine,

    // UI
    MenuToggle,
    PauseToggle,
    Inventory,
    Map,
    Journal,
    Settings,
    Accept,
    Cancel,
    Navigate,

    // Custom actions start here
    CustomActionStart = 1000
};

class InputActionEvent : public Event<InputActionEvent> {
public:
    InputAction action;
    float value;        // 0.0 to 1.0 for analog inputs, 0.0 or 1.0 for digital
    float delta;        // Change since last frame
    InputDevice source_device;
    bool is_start;      // true for action start, false for action end

    InputActionEvent(InputAction act, float val, float dlt, InputDevice dev, bool start)
        : action(act), value(val), delta(dlt), source_device(dev), is_start(start) {}

    std::string to_string() const override {
        return "InputActionEvent(action=" + std::to_string(static_cast<std::uint32_t>(action)) +
               ", value=" + std::to_string(value) +
               ", " + (is_start ? "start" : "end") + ")";
    }
};

// Utility functions for event creation and handling
namespace event_utils {

// Create modifier flags from individual booleans
ModifierKey create_modifiers(bool shift, bool ctrl, bool alt, bool super);

// Convert GLFW key code to engine key code
KeyCode glfw_key_to_keycode(int glfw_key);

// Convert GLFW mouse button to engine mouse button
MouseButton glfw_mouse_button_to_mouse_button(int glfw_button);

// Convert GLFW gamepad button to engine gamepad button
GamepadButton glfw_gamepad_button_to_gamepad_button(int glfw_button);

// Convert GLFW gamepad axis to engine gamepad axis
GamepadAxis glfw_gamepad_axis_to_gamepad_axis(int glfw_axis);

// String conversion utilities
std::string keycode_to_string(KeyCode key);
std::string mouse_button_to_string(MouseButton button);
std::string gamepad_button_to_string(GamepadButton button);
std::string gamepad_axis_to_string(GamepadAxis axis);
std::string input_action_to_string(InputAction action);
std::string modifier_key_to_string(ModifierKey modifiers);

// Parse strings back to enums
KeyCode string_to_keycode(const std::string& str);
MouseButton string_to_mouse_button(const std::string& str);
GamepadButton string_to_gamepad_button(const std::string& str);
GamepadAxis string_to_gamepad_axis(const std::string& str);
InputAction string_to_input_action(const std::string& str);

// Event validation
bool is_valid_keycode(KeyCode key);
bool is_valid_mouse_button(MouseButton button);
bool is_valid_gamepad_button(GamepadButton button);
bool is_valid_gamepad_axis(GamepadAxis axis);

} // namespace event_utils

} // namespace lore::input