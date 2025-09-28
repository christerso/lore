#include <lore/input/input_events.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <unordered_map>
#include <algorithm>

namespace lore::input::event_utils {

ModifierKey create_modifiers(bool shift, bool ctrl, bool alt, bool super) {
    ModifierKey mods = ModifierKey::None;
    if (shift) mods = mods | ModifierKey::Shift;
    if (ctrl) mods = mods | ModifierKey::Control;
    if (alt) mods = mods | ModifierKey::Alt;
    if (super) mods = mods | ModifierKey::Super;
    return mods;
}

KeyCode glfw_key_to_keycode(int glfw_key) {
    // Direct mapping for most keys
    if (glfw_key >= 32 && glfw_key <= 348) {
        return static_cast<KeyCode>(glfw_key);
    }
    return KeyCode::Unknown;
}

MouseButton glfw_mouse_button_to_mouse_button(int glfw_button) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::Left;
        case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
        case GLFW_MOUSE_BUTTON_4: return MouseButton::Button4;
        case GLFW_MOUSE_BUTTON_5: return MouseButton::Button5;
        case GLFW_MOUSE_BUTTON_6: return MouseButton::Button6;
        case GLFW_MOUSE_BUTTON_7: return MouseButton::Button7;
        case GLFW_MOUSE_BUTTON_8: return MouseButton::Button8;
        default: return MouseButton::Left;
    }
}

GamepadButton glfw_gamepad_button_to_gamepad_button(int glfw_button) {
    switch (glfw_button) {
        case GLFW_GAMEPAD_BUTTON_A: return GamepadButton::A;
        case GLFW_GAMEPAD_BUTTON_B: return GamepadButton::B;
        case GLFW_GAMEPAD_BUTTON_X: return GamepadButton::X;
        case GLFW_GAMEPAD_BUTTON_Y: return GamepadButton::Y;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return GamepadButton::LeftBumper;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return GamepadButton::RightBumper;
        case GLFW_GAMEPAD_BUTTON_BACK: return GamepadButton::Back;
        case GLFW_GAMEPAD_BUTTON_START: return GamepadButton::Start;
        case GLFW_GAMEPAD_BUTTON_GUIDE: return GamepadButton::Guide;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return GamepadButton::LeftStick;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return GamepadButton::RightStick;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return GamepadButton::DpadUp;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButton::DpadRight;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return GamepadButton::DpadDown;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return GamepadButton::DpadLeft;
        default: return GamepadButton::A;
    }
}

GamepadAxis glfw_gamepad_axis_to_gamepad_axis(int glfw_axis) {
    switch (glfw_axis) {
        case GLFW_GAMEPAD_AXIS_LEFT_X: return GamepadAxis::LeftStickX;
        case GLFW_GAMEPAD_AXIS_LEFT_Y: return GamepadAxis::LeftStickY;
        case GLFW_GAMEPAD_AXIS_RIGHT_X: return GamepadAxis::RightStickX;
        case GLFW_GAMEPAD_AXIS_RIGHT_Y: return GamepadAxis::RightStickY;
        case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER: return GamepadAxis::LeftTrigger;
        case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
        default: return GamepadAxis::LeftStickX;
    }
}

std::string keycode_to_string(KeyCode key) {
    static const std::unordered_map<KeyCode, std::string> key_names = {
        {KeyCode::Unknown, "Unknown"},
        {KeyCode::Space, "Space"},
        {KeyCode::Apostrophe, "Apostrophe"},
        {KeyCode::Comma, "Comma"},
        {KeyCode::Minus, "Minus"},
        {KeyCode::Period, "Period"},
        {KeyCode::Slash, "Slash"},

        {KeyCode::Digit0, "0"}, {KeyCode::Digit1, "1"}, {KeyCode::Digit2, "2"},
        {KeyCode::Digit3, "3"}, {KeyCode::Digit4, "4"}, {KeyCode::Digit5, "5"},
        {KeyCode::Digit6, "6"}, {KeyCode::Digit7, "7"}, {KeyCode::Digit8, "8"},
        {KeyCode::Digit9, "9"},

        {KeyCode::Semicolon, "Semicolon"},
        {KeyCode::Equal, "Equal"},

        {KeyCode::A, "A"}, {KeyCode::B, "B"}, {KeyCode::C, "C"}, {KeyCode::D, "D"},
        {KeyCode::E, "E"}, {KeyCode::F, "F"}, {KeyCode::G, "G"}, {KeyCode::H, "H"},
        {KeyCode::I, "I"}, {KeyCode::J, "J"}, {KeyCode::K, "K"}, {KeyCode::L, "L"},
        {KeyCode::M, "M"}, {KeyCode::N, "N"}, {KeyCode::O, "O"}, {KeyCode::P, "P"},
        {KeyCode::Q, "Q"}, {KeyCode::R, "R"}, {KeyCode::S, "S"}, {KeyCode::T, "T"},
        {KeyCode::U, "U"}, {KeyCode::V, "V"}, {KeyCode::W, "W"}, {KeyCode::X, "X"},
        {KeyCode::Y, "Y"}, {KeyCode::Z, "Z"},

        {KeyCode::LeftBracket, "LeftBracket"},
        {KeyCode::Backslash, "Backslash"},
        {KeyCode::RightBracket, "RightBracket"},
        {KeyCode::GraveAccent, "GraveAccent"},

        {KeyCode::Escape, "Escape"},
        {KeyCode::Enter, "Enter"},
        {KeyCode::Tab, "Tab"},
        {KeyCode::Backspace, "Backspace"},
        {KeyCode::Insert, "Insert"},
        {KeyCode::Delete, "Delete"},

        {KeyCode::Right, "Right"},
        {KeyCode::Left, "Left"},
        {KeyCode::Down, "Down"},
        {KeyCode::Up, "Up"},

        {KeyCode::PageUp, "PageUp"},
        {KeyCode::PageDown, "PageDown"},
        {KeyCode::Home, "Home"},
        {KeyCode::End, "End"},

        {KeyCode::CapsLock, "CapsLock"},
        {KeyCode::ScrollLock, "ScrollLock"},
        {KeyCode::NumLock, "NumLock"},
        {KeyCode::PrintScreen, "PrintScreen"},
        {KeyCode::Pause, "Pause"},

        {KeyCode::F1, "F1"}, {KeyCode::F2, "F2"}, {KeyCode::F3, "F3"}, {KeyCode::F4, "F4"},
        {KeyCode::F5, "F5"}, {KeyCode::F6, "F6"}, {KeyCode::F7, "F7"}, {KeyCode::F8, "F8"},
        {KeyCode::F9, "F9"}, {KeyCode::F10, "F10"}, {KeyCode::F11, "F11"}, {KeyCode::F12, "F12"},

        {KeyCode::LeftShift, "LeftShift"},
        {KeyCode::LeftControl, "LeftControl"},
        {KeyCode::LeftAlt, "LeftAlt"},
        {KeyCode::LeftSuper, "LeftSuper"},
        {KeyCode::RightShift, "RightShift"},
        {KeyCode::RightControl, "RightControl"},
        {KeyCode::RightAlt, "RightAlt"},
        {KeyCode::RightSuper, "RightSuper"},
        {KeyCode::Menu, "Menu"}
    };

    auto it = key_names.find(key);
    if (it != key_names.end()) {
        return it->second;
    }
    return "Unknown(" + std::to_string(static_cast<int>(key)) + ")";
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
        case GamepadButton::LeftStick: return "LeftStick";
        case GamepadButton::RightStick: return "RightStick";
        case GamepadButton::DpadUp: return "DpadUp";
        case GamepadButton::DpadRight: return "DpadRight";
        case GamepadButton::DpadDown: return "DpadDown";
        case GamepadButton::DpadLeft: return "DpadLeft";
        case GamepadButton::Paddle1: return "Paddle1";
        case GamepadButton::Paddle2: return "Paddle2";
        case GamepadButton::Paddle3: return "Paddle3";
        case GamepadButton::Paddle4: return "Paddle4";
        default: return "Unknown";
    }
}

std::string gamepad_axis_to_string(GamepadAxis axis) {
    switch (axis) {
        case GamepadAxis::LeftStickX: return "LeftStickX";
        case GamepadAxis::LeftStickY: return "LeftStickY";
        case GamepadAxis::RightStickX: return "RightStickX";
        case GamepadAxis::RightStickY: return "RightStickY";
        case GamepadAxis::LeftTrigger: return "LeftTrigger";
        case GamepadAxis::RightTrigger: return "RightTrigger";
        default: return "Unknown";
    }
}

std::string input_action_to_string(InputAction action) {
    switch (action) {
        case InputAction::None: return "None";

        // Movement
        case InputAction::MoveForward: return "MoveForward";
        case InputAction::MoveBackward: return "MoveBackward";
        case InputAction::MoveLeft: return "MoveLeft";
        case InputAction::MoveRight: return "MoveRight";
        case InputAction::MoveUp: return "MoveUp";
        case InputAction::MoveDown: return "MoveDown";
        case InputAction::Jump: return "Jump";
        case InputAction::Crouch: return "Crouch";
        case InputAction::Sprint: return "Sprint";
        case InputAction::Walk: return "Walk";

        // Camera
        case InputAction::LookUp: return "LookUp";
        case InputAction::LookDown: return "LookDown";
        case InputAction::LookLeft: return "LookLeft";
        case InputAction::LookRight: return "LookRight";
        case InputAction::CameraZoomIn: return "CameraZoomIn";
        case InputAction::CameraZoomOut: return "CameraZoomOut";

        // Combat
        case InputAction::Attack: return "Attack";
        case InputAction::SecondaryAttack: return "SecondaryAttack";
        case InputAction::Block: return "Block";
        case InputAction::Parry: return "Parry";
        case InputAction::Dodge: return "Dodge";
        case InputAction::Reload: return "Reload";
        case InputAction::Aim: return "Aim";

        // Interaction
        case InputAction::Interact: return "Interact";
        case InputAction::Use: return "Use";
        case InputAction::PickUp: return "PickUp";
        case InputAction::Drop: return "Drop";
        case InputAction::Examine: return "Examine";

        // UI
        case InputAction::MenuToggle: return "MenuToggle";
        case InputAction::PauseToggle: return "PauseToggle";
        case InputAction::Inventory: return "Inventory";
        case InputAction::Map: return "Map";
        case InputAction::Journal: return "Journal";
        case InputAction::Settings: return "Settings";
        case InputAction::Accept: return "Accept";
        case InputAction::Cancel: return "Cancel";
        case InputAction::Navigate: return "Navigate";

        default:
            if (static_cast<std::uint32_t>(action) >= static_cast<std::uint32_t>(InputAction::CustomActionStart)) {
                return "CustomAction" + std::to_string(static_cast<std::uint32_t>(action) -
                                                     static_cast<std::uint32_t>(InputAction::CustomActionStart));
            }
            return "Unknown";
    }
}

std::string modifier_key_to_string(ModifierKey modifiers) {
    std::string result;

    if (has_modifier(modifiers, ModifierKey::Control)) {
        if (!result.empty()) result += "+";
        result += "Ctrl";
    }
    if (has_modifier(modifiers, ModifierKey::Alt)) {
        if (!result.empty()) result += "+";
        result += "Alt";
    }
    if (has_modifier(modifiers, ModifierKey::Shift)) {
        if (!result.empty()) result += "+";
        result += "Shift";
    }
    if (has_modifier(modifiers, ModifierKey::Super)) {
        if (!result.empty()) result += "+";
        result += "Super";
    }

    return result.empty() ? "None" : result;
}

// String to enum conversions
KeyCode string_to_keycode(const std::string& str) {
    static std::unordered_map<std::string, KeyCode> string_to_key;

    // Initialize on first use
    if (string_to_key.empty()) {
        string_to_key["Unknown"] = KeyCode::Unknown;
        string_to_key["Space"] = KeyCode::Space;
        string_to_key["Apostrophe"] = KeyCode::Apostrophe;
        string_to_key["Comma"] = KeyCode::Comma;
        string_to_key["Minus"] = KeyCode::Minus;
        string_to_key["Period"] = KeyCode::Period;
        string_to_key["Slash"] = KeyCode::Slash;

        // Numbers
        for (int i = 0; i <= 9; ++i) {
            string_to_key[std::to_string(i)] = static_cast<KeyCode>(static_cast<int>(KeyCode::Digit0) + i);
        }

        string_to_key["Semicolon"] = KeyCode::Semicolon;
        string_to_key["Equal"] = KeyCode::Equal;

        // Letters
        for (char c = 'A'; c <= 'Z'; ++c) {
            string_to_key[std::string(1, c)] = static_cast<KeyCode>(c);
        }

        string_to_key["LeftBracket"] = KeyCode::LeftBracket;
        string_to_key["Backslash"] = KeyCode::Backslash;
        string_to_key["RightBracket"] = KeyCode::RightBracket;
        string_to_key["GraveAccent"] = KeyCode::GraveAccent;

        string_to_key["Escape"] = KeyCode::Escape;
        string_to_key["Enter"] = KeyCode::Enter;
        string_to_key["Tab"] = KeyCode::Tab;
        string_to_key["Backspace"] = KeyCode::Backspace;
        string_to_key["Insert"] = KeyCode::Insert;
        string_to_key["Delete"] = KeyCode::Delete;

        string_to_key["Right"] = KeyCode::Right;
        string_to_key["Left"] = KeyCode::Left;
        string_to_key["Down"] = KeyCode::Down;
        string_to_key["Up"] = KeyCode::Up;

        string_to_key["PageUp"] = KeyCode::PageUp;
        string_to_key["PageDown"] = KeyCode::PageDown;
        string_to_key["Home"] = KeyCode::Home;
        string_to_key["End"] = KeyCode::End;

        // Function keys
        for (int i = 1; i <= 12; ++i) {
            string_to_key["F" + std::to_string(i)] = static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + i - 1);
        }

        string_to_key["LeftShift"] = KeyCode::LeftShift;
        string_to_key["LeftControl"] = KeyCode::LeftControl;
        string_to_key["LeftAlt"] = KeyCode::LeftAlt;
        string_to_key["LeftSuper"] = KeyCode::LeftSuper;
        string_to_key["RightShift"] = KeyCode::RightShift;
        string_to_key["RightControl"] = KeyCode::RightControl;
        string_to_key["RightAlt"] = KeyCode::RightAlt;
        string_to_key["RightSuper"] = KeyCode::RightSuper;
        string_to_key["Menu"] = KeyCode::Menu;
    }

    auto it = string_to_key.find(str);
    return it != string_to_key.end() ? it->second : KeyCode::Unknown;
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
    return MouseButton::Left;
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
    if (str == "LeftStick") return GamepadButton::LeftStick;
    if (str == "RightStick") return GamepadButton::RightStick;
    if (str == "DpadUp") return GamepadButton::DpadUp;
    if (str == "DpadRight") return GamepadButton::DpadRight;
    if (str == "DpadDown") return GamepadButton::DpadDown;
    if (str == "DpadLeft") return GamepadButton::DpadLeft;
    return GamepadButton::A;
}

GamepadAxis string_to_gamepad_axis(const std::string& str) {
    if (str == "LeftStickX") return GamepadAxis::LeftStickX;
    if (str == "LeftStickY") return GamepadAxis::LeftStickY;
    if (str == "RightStickX") return GamepadAxis::RightStickX;
    if (str == "RightStickY") return GamepadAxis::RightStickY;
    if (str == "LeftTrigger") return GamepadAxis::LeftTrigger;
    if (str == "RightTrigger") return GamepadAxis::RightTrigger;
    return GamepadAxis::LeftStickX;
}

InputAction string_to_input_action(const std::string& str) {
    // This is a large mapping - in production you might want to use a static map
    if (str == "None") return InputAction::None;

    // Movement
    if (str == "MoveForward") return InputAction::MoveForward;
    if (str == "MoveBackward") return InputAction::MoveBackward;
    if (str == "MoveLeft") return InputAction::MoveLeft;
    if (str == "MoveRight") return InputAction::MoveRight;
    if (str == "Jump") return InputAction::Jump;
    if (str == "Crouch") return InputAction::Crouch;
    if (str == "Sprint") return InputAction::Sprint;

    // Camera
    if (str == "LookUp") return InputAction::LookUp;
    if (str == "LookDown") return InputAction::LookDown;
    if (str == "LookLeft") return InputAction::LookLeft;
    if (str == "LookRight") return InputAction::LookRight;

    // UI
    if (str == "MenuToggle") return InputAction::MenuToggle;
    if (str == "Accept") return InputAction::Accept;
    if (str == "Cancel") return InputAction::Cancel;

    // Check for custom actions
    if (str.substr(0, 12) == "CustomAction") {
        try {
            int custom_id = std::stoi(str.substr(12));
            return static_cast<InputAction>(static_cast<std::uint32_t>(InputAction::CustomActionStart) + custom_id);
        } catch (...) {
            return InputAction::None;
        }
    }

    return InputAction::None;
}

// Validation functions
bool is_valid_keycode(KeyCode key) {
    int key_val = static_cast<int>(key);
    return (key_val >= 32 && key_val <= 348) || key == KeyCode::Unknown;
}

bool is_valid_mouse_button(MouseButton button) {
    int button_val = static_cast<int>(button);
    return button_val >= 0 && button_val <= 7;
}

bool is_valid_gamepad_button(GamepadButton button) {
    int button_val = static_cast<int>(button);
    return button_val >= 0 && button_val <= 18;
}

bool is_valid_gamepad_axis(GamepadAxis axis) {
    int axis_val = static_cast<int>(axis);
    return axis_val >= 0 && axis_val <= 5;
}

} // namespace lore::input::event_utils