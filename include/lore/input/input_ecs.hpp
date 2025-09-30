#pragma once

#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>
#include <lore/input/input_listener_manager.hpp>
#include <lore/input/glfw_input_handler.hpp>
#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>

#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <bitset>

namespace lore::input {

// Forward declarations
class InputECSSystem;
class EntityInputHandler;

// Input component for entities that can receive input
struct InputComponent {
    // Component state
    bool enabled = true;
    bool consume_events = false;  // If true, handled events won't propagate to other entities

    // Input action mappings for this entity
    std::unordered_map<InputAction, std::function<void(float)>> action_handlers;

    // Key mappings
    std::unordered_map<KeyCode, std::function<void(bool)>> key_handlers;  // bool = pressed/released

    // Mouse mappings
    std::unordered_map<MouseButton, std::function<void(bool, glm::vec2)>> mouse_button_handlers;
    std::function<void(glm::vec2, glm::vec2)> mouse_move_handler;  // position, delta
    std::function<void(glm::vec2, glm::vec2)> mouse_scroll_handler;  // offset, position

    // Gamepad mappings
    std::uint32_t preferred_gamepad_id = 0;
    std::unordered_map<GamepadButton, std::function<void(bool)>> gamepad_button_handlers;
    std::unordered_map<GamepadAxis, std::function<void(float, float)>> gamepad_axis_handlers;  // value, delta

    // Window event handlers
    std::function<void(std::uint32_t, std::uint32_t)> window_resize_handler;  // width, height
    std::function<void(bool)> window_focus_handler;  // focused

    // Text input handler
    std::function<void(const std::string&)> text_input_handler;

    // File drop handler
    std::function<void(const std::vector<std::string>&)> file_drop_handler;

    // Priority for input handling (higher = handled first)
    EventPriority priority = EventPriority::Normal;

    // Input filtering
    std::unordered_set<std::type_index> accepted_event_types;  // Empty = accept all
    std::function<bool(const IEvent&)> event_filter;  // Additional custom filtering

    // Statistics
    std::size_t events_handled = 0;
    std::chrono::high_resolution_clock::time_point last_input_time;
};

// Focus component - entities with this component can receive input focus
struct FocusableComponent {
    bool has_focus = false;
    bool can_receive_focus = true;
    bool steal_focus_on_click = true;

    // Focus priority (higher = receives focus first)
    int focus_priority = 0;

    // Bounding box for focus determination (in world or screen coordinates)
    glm::vec2 focus_bounds_min{0.0f};
    glm::vec2 focus_bounds_max{0.0f};
    bool use_world_coordinates = true;

    // Focus callbacks
    std::function<void()> on_focus_gained;
    std::function<void()> on_focus_lost;
};

// UI input component for UI elements
struct UIInputComponent {
    bool enabled = true;
    bool visible = true;

    // UI element bounds (screen coordinates)
    glm::vec2 position{0.0f};
    glm::vec2 size{0.0f};

    // UI event handlers
    std::function<void(glm::vec2)> on_click;
    std::function<void(glm::vec2)> on_hover_enter;
    std::function<void(glm::vec2)> on_hover_exit;
    std::function<void(glm::vec2, glm::vec2)> on_drag;
    std::function<void(glm::vec2)> on_scroll;

    // UI state
    bool is_hovered = false;
    bool is_pressed = false;
    bool is_dragging = false;

    // Click handling
    MouseButton trigger_button = MouseButton::Left;
    bool handle_double_click = false;
    std::function<void(glm::vec2)> on_double_click;

    // Keyboard input for UI (when focused)
    std::function<void(KeyCode, bool)> on_key;
    std::function<void(const std::string&)> on_text_input;
};

// Transform component for spatial input handling
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 world_matrix{1.0f};
    bool dirty = true;

    void mark_dirty() { dirty = true; }
    const glm::mat4& get_world_matrix();
    void update_world_matrix();
};

// Camera component for camera-specific input handling
struct CameraComponent {
    // Camera parameters
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    float fov = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect_ratio = 16.0f / 9.0f;

    // Camera movement settings
    float movement_speed = 5.0f;
    float rotation_speed = 1.0f;
    float zoom_speed = 2.0f;
    float mouse_sensitivity = 0.1f;

    // Camera state
    bool is_active = true;
    bool invert_y = false;

    // Camera matrices (cached)
    mutable glm::mat4 view_matrix{1.0f};
    mutable glm::mat4 projection_matrix{1.0f};
    mutable bool matrices_dirty = true;

    const glm::mat4& get_view_matrix() const;
    const glm::mat4& get_projection_matrix() const;
    void mark_matrices_dirty() const { matrices_dirty = true; }
};

// Entity input handler - manages input for a specific entity
class EntityInputHandler {
public:
    explicit EntityInputHandler(lore::ecs::Entity entity, InputListenerManager& listener_manager);
    ~EntityInputHandler();

    // Non-copyable, movable
    EntityInputHandler(const EntityInputHandler&) = delete;
    EntityInputHandler& operator=(const EntityInputHandler&) = delete;
    EntityInputHandler(EntityInputHandler&&) = default;
    EntityInputHandler& operator=(EntityInputHandler&&) = default;

    // Setup input handling
    void setup_input_handlers(lore::ecs::World& world);
    void cleanup_input_handlers();

    // Input registration
    void register_action_handler(InputAction action, std::function<void(float)> handler);
    void register_key_handler(KeyCode key, std::function<void(bool)> handler);
    void register_mouse_button_handler(MouseButton button, std::function<void(bool, glm::vec2)> handler);
    void register_mouse_move_handler(std::function<void(glm::vec2, glm::vec2)> handler);

    // Focus management
    void set_focus_enabled(bool enabled);
    bool has_focus() const;
    void request_focus();
    void release_focus();

    // Entity access
    lore::ecs::Entity get_entity() const { return entity_; }

    // State
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    lore::ecs::Entity entity_;
    InputListenerManager& listener_manager_;
    std::vector<ManagedListenerHandle> listener_handles_;
    bool enabled_ = true;
    bool has_focus_ = false;

    // Helper methods
    void update_input_component_handlers(lore::ecs::World& world);
    void create_event_listeners();
    void handle_event_for_entity(const IEvent& event, lore::ecs::World& world);
};

// Input ECS System - manages input for all entities
class InputECSSystem : public lore::ecs::System {
public:
    explicit InputECSSystem(GLFWInputSystem& input_system);
    ~InputECSSystem() override;

    // System lifecycle
    void init(lore::ecs::World& world) override;
    void update(lore::ecs::World& world, float delta_time) override;
    void shutdown(lore::ecs::World& world) override;

    // Focus management
    void set_focused_entity(lore::ecs::Entity entity);
    lore::ecs::Entity get_focused_entity() const { return focused_entity_; }
    void clear_focus();
    void update_focus_from_mouse_position(lore::ecs::World& world, glm::vec2 mouse_pos);

    // UI input handling
    void update_ui_input(lore::ecs::World& world);
    void handle_ui_click(lore::ecs::World& world, glm::vec2 position, MouseButton button);
    void handle_ui_hover(lore::ecs::World& world, glm::vec2 position);

    // Entity input registration
    void register_entity_for_input(lore::ecs::Entity entity);
    void unregister_entity_from_input(lore::ecs::Entity entity);

    // Camera input handling
    void update_camera_input(lore::ecs::World& world, float delta_time);
    void setup_camera_controls(lore::ecs::Entity camera_entity);

    // Configuration
    void set_debug_mode(bool enabled) { debug_mode_ = enabled; }
    bool is_debug_mode() const { return debug_mode_; }

    void set_ui_input_enabled(bool enabled) { ui_input_enabled_ = enabled; }
    bool is_ui_input_enabled() const { return ui_input_enabled_; }

    // Statistics
    struct Statistics {
        std::size_t entities_with_input = 0;
        std::size_t focusable_entities = 0;
        std::size_t ui_entities = 0;
        std::size_t camera_entities = 0;
        std::size_t total_input_events_handled = 0;
    };

    Statistics get_statistics(lore::ecs::World& world) const;

    // Access to underlying systems
    GLFWInputSystem& get_input_system() { return input_system_; }
    InputListenerManager& get_listener_manager() { return listener_manager_; }

private:
    GLFWInputSystem& input_system_;
    InputListenerManager listener_manager_;

    // Focus management
    lore::ecs::Entity focused_entity_ = lore::ecs::INVALID_ENTITY;
    std::vector<lore::ecs::Entity> focusable_entities_;

    // Entity handlers
    std::unordered_map<lore::ecs::Entity, std::unique_ptr<EntityInputHandler>> entity_handlers_;

    // Mouse state for UI
    glm::vec2 last_mouse_position_{0.0f};
    std::unordered_set<lore::ecs::Entity> hovered_ui_entities_;

    // Configuration
    bool debug_mode_ = false;
    bool ui_input_enabled_ = true;
    bool camera_input_enabled_ = true;

    // Event handlers
    ManagedListenerHandle mouse_move_handle_;
    ManagedListenerHandle mouse_click_handle_;
    ManagedListenerHandle key_press_handle_;
    ManagedListenerHandle window_resize_handle_;

    // Helper methods
    void setup_global_input_handlers();
    void update_focusable_entities(lore::ecs::World& world);
    bool is_point_in_ui_element(glm::vec2 point, const UIInputComponent& ui_component) const;
    bool is_point_in_focus_bounds(glm::vec2 point, const FocusableComponent& focus_component) const;
    std::vector<lore::ecs::Entity> get_entities_at_position(lore::ecs::World& world, glm::vec2 position) const;
    void handle_global_input_event(const IEvent& event, lore::ecs::World& world);
    void propagate_event_to_entities(const IEvent& event, lore::ecs::World& world);

    // Camera helpers
    void handle_camera_movement(lore::ecs::Entity camera_entity, lore::ecs::World& world, float delta_time);
    void handle_camera_mouse_look(lore::ecs::Entity camera_entity, lore::ecs::World& world, glm::vec2 mouse_delta);
    void handle_camera_zoom(lore::ecs::Entity camera_entity, lore::ecs::World& world, float zoom_delta);

    // Debug
    void debug_print_input_state(lore::ecs::World& world) const;
};

// Utility functions for common input setups
namespace input_utils {

// Setup basic WASD movement for an entity
void setup_wasd_movement(lore::ecs::Entity entity, lore::ecs::World& world, float movement_speed = 5.0f);

// Setup camera controls for first-person camera
void setup_first_person_camera(lore::ecs::Entity camera_entity, lore::ecs::World& world);

// Setup camera controls for orbit camera
void setup_orbit_camera(lore::ecs::Entity camera_entity, lore::ecs::World& world, glm::vec3 target = {0.0f, 0.0f, 0.0f});

// Setup UI button
void setup_ui_button(lore::ecs::Entity button_entity, lore::ecs::World& world,
                    glm::vec2 position, glm::vec2 size, std::function<void()> on_click);

// Setup text input field
void setup_text_input_field(lore::ecs::Entity field_entity, lore::ecs::World& world,
                           glm::vec2 position, glm::vec2 size, std::function<void(const std::string&)> on_text_change);

// Setup draggable UI element
void setup_draggable_ui(lore::ecs::Entity ui_entity, lore::ecs::World& world,
                       std::function<void(glm::vec2)> on_drag_update = nullptr);

// Create input action mappings
InputComponent create_player_input_component();
InputComponent create_camera_input_component();
InputComponent create_ui_input_component();

// Focus management utilities
void make_entity_focusable(lore::ecs::Entity entity, lore::ecs::World& world,
                          glm::vec2 bounds_min, glm::vec2 bounds_max, int priority = 0);

void make_entity_ui_interactive(lore::ecs::Entity entity, lore::ecs::World& world,
                               glm::vec2 position, glm::vec2 size);

} // namespace input_utils

} // namespace lore::input