#include <lore/input/input.hpp>
#include <lore/ecs/ecs.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lore::input {

// Input-related ECS systems

// System for processing input components on entities
class InputComponentSystem : public lore::ecs::System {
public:
    void init(lore::ecs::World& world) override {
        input_system_ = &world.get_system<InputSystem>();
    }

    void update(lore::ecs::World& world, float delta_time) override {
        (void)delta_time; // Unused parameter

        auto& input_components = world.get_component_array<InputComponent>();
        auto& action_mapper = input_system_->get_action_mapper();

        // Process all entities with input components
        for (std::size_t i = 0; i < input_components.size(); ++i) {
            // auto entity = input_components.entities()[i];  // Not currently used
            auto& input_comp = input_components.data()[i];

            if (!input_comp.process_input) continue;

            // Process each action for this entity
            for (auto action : input_comp.actions) {
                float action_value = action_mapper.get_action_value(action);
                bool triggered = action_mapper.was_action_triggered_this_frame(action);
                // bool released = action_mapper.was_action_released_this_frame(action);  // Not currently used

                // Call entity-specific action handler if available
                auto handler_it = input_comp.action_handlers.find(action);
                if (handler_it != input_comp.action_handlers.end() && handler_it->second) {
                    if (triggered || (action_value > 0.0f)) {
                        handler_it->second(action_value);
                    }
                }
            }
        }
    }

private:
    InputSystem* input_system_ = nullptr;
};

// System for mouse follower components
class MouseFollowerSystem : public lore::ecs::System {
public:
    void init(lore::ecs::World& world) override {
        input_system_ = &world.get_system<InputSystem>();
    }

    void update(lore::ecs::World& world, float /*delta_time*/) override {
        auto& mouse_followers = world.get_component_array<MouseFollowerComponent>();
        auto mouse_position = input_system_->get_state_manager().get_mouse_position();

        // Update all entities with mouse follower components
        for (std::size_t i = 0; i < mouse_followers.size(); ++i) {
            // auto entity = mouse_followers.entities()[i];  // Not currently used
            auto& follower = mouse_followers.data()[i];

            if (!follower.active) continue;

            // Calculate target position with offset
            glm::vec2 target_position = mouse_position + follower.offset;

            // Get entity's current position (assuming Transform component exists)
            // Note: This would require a Transform component to be defined
            // For now, we'll just demonstrate the concept

            // Smooth movement towards target
            if (follower.smoothing > 0.0f) {
                // Interpolate towards target position
                // float factor = std::min(1.0f, delta_time * follower.smoothing);  // Not currently used
                // current_position = lerp(current_position, target_position, factor);
            } else {
                // Instant movement
                // current_position = target_position;
            }
        }
    }

private:
    InputSystem* input_system_ = nullptr;
};

// First-person camera controller using input actions
class FirstPersonCameraController : public lore::ecs::System {
public:
    FirstPersonCameraController(float sensitivity = 1.0f, float move_speed = 5.0f)
        : mouse_sensitivity_(sensitivity), move_speed_(move_speed) {}

    void init(lore::ecs::World& world) override {
        input_system_ = &world.get_system<InputSystem>();

        // Set up action callbacks for camera control
        auto& action_mapper = input_system_->get_action_mapper();

        action_mapper.set_action_callback(InputAction::LookUp,
            [this](InputAction, float value) { pitch_input_ += value * mouse_sensitivity_; });

        action_mapper.set_action_callback(InputAction::LookDown,
            [this](InputAction, float value) { pitch_input_ -= value * mouse_sensitivity_; });

        action_mapper.set_action_callback(InputAction::LookLeft,
            [this](InputAction, float value) { yaw_input_ -= value * mouse_sensitivity_; });

        action_mapper.set_action_callback(InputAction::LookRight,
            [this](InputAction, float value) { yaw_input_ += value * mouse_sensitivity_; });

        // Set up mouse move callback for direct mouse input
        input_system_->set_mouse_move_callback([this](const MouseMoveEvent& event) {
            if (mouse_look_enabled_) {
                yaw_ += event.delta.x * mouse_sensitivity_ * 0.1f;
                pitch_ -= event.delta.y * mouse_sensitivity_ * 0.1f;
                pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
            }
        });
    }

    void update(lore::ecs::World& /*world*/, float delta_time) override {
        auto& action_mapper = input_system_->get_action_mapper();

        // Update rotation from accumulated input
        yaw_ += yaw_input_ * delta_time;
        pitch_ += pitch_input_ * delta_time;
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

        // Reset input accumulation
        yaw_input_ = 0.0f;
        pitch_input_ = 0.0f;

        // Calculate movement vector
        glm::vec3 movement(0.0f);

        if (action_mapper.is_action_active(InputAction::MoveForward)) {
            movement.z -= action_mapper.get_action_value(InputAction::MoveForward);
        }
        if (action_mapper.is_action_active(InputAction::MoveBackward)) {
            movement.z += action_mapper.get_action_value(InputAction::MoveBackward);
        }
        if (action_mapper.is_action_active(InputAction::MoveLeft)) {
            movement.x -= action_mapper.get_action_value(InputAction::MoveLeft);
        }
        if (action_mapper.is_action_active(InputAction::MoveRight)) {
            movement.x += action_mapper.get_action_value(InputAction::MoveRight);
        }
        if (action_mapper.is_action_active(InputAction::Jump)) {
            movement.y += action_mapper.get_action_value(InputAction::Jump);
        }
        if (action_mapper.is_action_active(InputAction::Crouch)) {
            movement.y -= action_mapper.get_action_value(InputAction::Crouch);
        }

        // Apply sprint modifier
        float speed_multiplier = 1.0f;
        if (action_mapper.is_action_active(InputAction::Sprint)) {
            speed_multiplier = 2.0f;
        }

        // Transform movement to world space
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(yaw_), glm::vec3(0, 1, 0));
        glm::vec4 world_movement = rotation * glm::vec4(movement, 0.0f);

        // Apply movement
        position_ += glm::vec3(world_movement) * move_speed_ * speed_multiplier * delta_time;

        // Update camera matrix
        update_camera_matrix();

        // Check for menu toggle
        if (action_mapper.was_action_triggered_this_frame(InputAction::MenuToggle)) {
            toggle_mouse_look();
        }
    }

    // Camera access methods
    const glm::mat4& get_view_matrix() const { return view_matrix_; }
    const glm::vec3& get_position() const { return position_; }
    const glm::vec3& get_forward() const { return forward_; }
    const glm::vec3& get_right() const { return right_; }
    const glm::vec3& get_up() const { return up_; }

    void set_position(const glm::vec3& position) {
        position_ = position;
        update_camera_matrix();
    }

    void set_rotation(float yaw, float pitch) {
        yaw_ = yaw;
        pitch_ = std::clamp(pitch, -89.0f, 89.0f);
        update_camera_matrix();
    }

    void set_mouse_look_enabled(bool enabled) {
        mouse_look_enabled_ = enabled;
        if (input_system_) {
            input_system_->set_cursor_mode(!enabled, enabled);
        }
    }

    void toggle_mouse_look() {
        set_mouse_look_enabled(!mouse_look_enabled_);
    }

    void set_sensitivity(float sensitivity) { mouse_sensitivity_ = sensitivity; }
    void set_move_speed(float speed) { move_speed_ = speed; }

private:
    InputSystem* input_system_ = nullptr;

    // Camera state
    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    // Camera vectors
    glm::vec3 forward_{0.0f, 0.0f, -1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    glm::mat4 view_matrix_{1.0f};

    // Input accumulation
    float yaw_input_ = 0.0f;
    float pitch_input_ = 0.0f;

    // Settings
    float mouse_sensitivity_ = 1.0f;
    float move_speed_ = 5.0f;
    bool mouse_look_enabled_ = true;

    void update_camera_matrix() {
        // Calculate direction vectors
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        direction.y = sin(glm::radians(pitch_));
        direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));

        forward_ = glm::normalize(direction);
        right_ = glm::normalize(glm::cross(forward_, glm::vec3(0.0f, 1.0f, 0.0f)));
        up_ = glm::normalize(glm::cross(right_, forward_));

        view_matrix_ = glm::lookAt(position_, position_ + forward_, up_);
    }
};

// Vehicle controller system using gamepad input
class VehicleController : public lore::ecs::System {
public:
    VehicleController(std::uint32_t gamepad_id = 0) : gamepad_id_(gamepad_id) {}

    void init(lore::ecs::World& world) override {
        input_system_ = &world.get_system<InputSystem>();
    }

    void update(lore::ecs::World& /*world*/, float delta_time) override {
        auto& state_manager = input_system_->get_state_manager();

        if (!state_manager.is_gamepad_connected(gamepad_id_)) {
            return;
        }

        // Get gamepad input
        float throttle = state_manager.get_gamepad_axis(gamepad_id_, GamepadAxis::RightTrigger);
        float brake = state_manager.get_gamepad_axis(gamepad_id_, GamepadAxis::LeftTrigger);
        float steering = state_manager.get_gamepad_axis(gamepad_id_, GamepadAxis::LeftX);

        // Apply deadzone to steering
        steering = util::apply_deadzone(steering, 0.1f);

        // Update vehicle state
        current_throttle_ = throttle;
        current_brake_ = brake;
        current_steering_ = steering;

        // Handle button inputs
        if (state_manager.was_gamepad_button_pressed_this_frame(gamepad_id_, GamepadButton::A)) {
            handbrake_active_ = true;
        }
        if (state_manager.was_gamepad_button_released_this_frame(gamepad_id_, GamepadButton::A)) {
            handbrake_active_ = false;
        }

        // Gear shifting
        if (state_manager.was_gamepad_button_pressed_this_frame(gamepad_id_, GamepadButton::RightBumper)) {
            shift_up();
        }
        if (state_manager.was_gamepad_button_pressed_this_frame(gamepad_id_, GamepadButton::LeftBumper)) {
            shift_down();
        }

        // Camera controls
        glm::vec2 right_stick;
        right_stick.x = state_manager.get_gamepad_axis(gamepad_id_, GamepadAxis::RightX);
        right_stick.y = state_manager.get_gamepad_axis(gamepad_id_, GamepadAxis::RightY);

        // Apply circular deadzone to camera stick
        right_stick = util::apply_circular_deadzone(right_stick, 0.15f);

        camera_yaw_input_ = right_stick.x * camera_sensitivity_;
        camera_pitch_input_ = -right_stick.y * camera_sensitivity_;

        // Update vehicle physics (simplified)
        update_vehicle_physics(delta_time);
    }

    // Vehicle state accessors
    float get_throttle() const { return current_throttle_; }
    float get_brake() const { return current_brake_; }
    float get_steering() const { return current_steering_; }
    bool is_handbrake_active() const { return handbrake_active_; }
    int get_current_gear() const { return current_gear_; }
    float get_speed() const { return current_speed_; }

    // Camera input accessors
    float get_camera_yaw_input() const { return camera_yaw_input_; }
    float get_camera_pitch_input() const { return camera_pitch_input_; }

    void set_gamepad_id(std::uint32_t id) { gamepad_id_ = id; }
    void set_camera_sensitivity(float sensitivity) { camera_sensitivity_ = sensitivity; }

private:
    InputSystem* input_system_ = nullptr;
    std::uint32_t gamepad_id_ = 0;

    // Vehicle state
    float current_throttle_ = 0.0f;
    float current_brake_ = 0.0f;
    float current_steering_ = 0.0f;
    bool handbrake_active_ = false;
    int current_gear_ = 1;
    float current_speed_ = 0.0f;

    // Camera input
    float camera_yaw_input_ = 0.0f;
    float camera_pitch_input_ = 0.0f;
    float camera_sensitivity_ = 2.0f;

    void shift_up() {
        if (current_gear_ < 6) {
            ++current_gear_;
        }
    }

    void shift_down() {
        if (current_gear_ > 1) {
            --current_gear_;
        }
    }

    void update_vehicle_physics(float delta_time) {
        // Simplified vehicle physics
        float acceleration = current_throttle_ * 10.0f; // m/s²
        float deceleration = current_brake_ * 15.0f; // m/s²

        if (handbrake_active_) {
            deceleration = std::max(deceleration, 20.0f);
        }

        // Update speed
        if (current_throttle_ > 0.0f) {
            current_speed_ += acceleration * delta_time;
        } else {
            current_speed_ -= deceleration * delta_time;
        }

        // Apply natural friction
        current_speed_ -= current_speed_ * 0.5f * delta_time;

        // Clamp speed
        current_speed_ = std::max(0.0f, current_speed_);
        current_speed_ = std::min(current_speed_, 50.0f); // Max 50 m/s
    }
};

// UI input system for menu navigation
class UIInputSystem : public lore::ecs::System {
public:
    void init(lore::ecs::World& world) override {
        input_system_ = &world.get_system<InputSystem>();

        // Set up UI-specific action callbacks
        auto& action_mapper = input_system_->get_action_mapper();

        action_mapper.set_action_callback(InputAction::Accept,
            [this](InputAction, float) {
                if (on_accept_) on_accept_();
            });

        action_mapper.set_action_callback(InputAction::Cancel,
            [this](InputAction, float) {
                if (on_cancel_) on_cancel_();
            });

        // Set up mouse callbacks for UI interaction
        input_system_->set_mouse_button_callback([this](const MouseButtonEvent& event) {
            if (event.type == EventType::MouseButtonPressed) {
                handle_mouse_click(event.position, event.button);
            }
        });

        input_system_->set_mouse_move_callback([this](const MouseMoveEvent& event) {
            handle_mouse_move(event.position);
        });
    }

    void update(lore::ecs::World& world, float delta_time) override {
        (void)world;
        (void)delta_time;

        auto& action_mapper = input_system_->get_action_mapper();

        // Handle navigation input
        if (action_mapper.was_action_triggered_this_frame(InputAction::MoveForward)) {
            navigate_up();
        }
        if (action_mapper.was_action_triggered_this_frame(InputAction::MoveBackward)) {
            navigate_down();
        }
        if (action_mapper.was_action_triggered_this_frame(InputAction::MoveLeft)) {
            navigate_left();
        }
        if (action_mapper.was_action_triggered_this_frame(InputAction::MoveRight)) {
            navigate_right();
        }
    }

    // UI event callbacks
    void set_accept_callback(std::function<void()> callback) { on_accept_ = std::move(callback); }
    void set_cancel_callback(std::function<void()> callback) { on_cancel_ = std::move(callback); }
    void set_navigate_callback(std::function<void(int, int)> callback) { on_navigate_ = std::move(callback); }
    void set_mouse_click_callback(std::function<void(glm::vec2, MouseButton)> callback) { on_mouse_click_ = std::move(callback); }
    void set_mouse_move_callback(std::function<void(glm::vec2)> callback) { on_mouse_move_ = std::move(callback); }

private:
    InputSystem* input_system_ = nullptr;

    // UI callbacks
    std::function<void()> on_accept_;
    std::function<void()> on_cancel_;
    std::function<void(int, int)> on_navigate_; // direction x, y (-1, 0, 1)
    std::function<void(glm::vec2, MouseButton)> on_mouse_click_;
    std::function<void(glm::vec2)> on_mouse_move_;

    void navigate_up() { if (on_navigate_) on_navigate_(0, -1); }
    void navigate_down() { if (on_navigate_) on_navigate_(0, 1); }
    void navigate_left() { if (on_navigate_) on_navigate_(-1, 0); }
    void navigate_right() { if (on_navigate_) on_navigate_(1, 0); }

    void handle_mouse_click(glm::vec2 position, MouseButton button) {
        if (on_mouse_click_) {
            on_mouse_click_(position, button);
        }
    }

    void handle_mouse_move(glm::vec2 position) {
        if (on_mouse_move_) {
            on_mouse_move_(position);
        }
    }
};

// Helper functions for registering input systems
namespace ecs_integration {

void register_input_systems(lore::ecs::World& world) {
    // Register core input system first
    world.add_system<InputSystem>();

    // Register input-related ECS systems
    world.add_system<InputComponentSystem>();
    world.add_system<MouseFollowerSystem>();
    world.add_system<UIInputSystem>();
}

void register_camera_controller(lore::ecs::World& world, float sensitivity = 1.0f, float move_speed = 5.0f) {
    world.add_system<FirstPersonCameraController>(sensitivity, move_speed);
}

void register_vehicle_controller(lore::ecs::World& world, std::uint32_t gamepad_id = 0) {
    world.add_system<VehicleController>(gamepad_id);
}

// Helper function to create an entity with input component
lore::ecs::EntityHandle create_input_entity(lore::ecs::World& world,
                                           const std::vector<InputAction>& actions) {
    auto entity = world.create_entity();

    InputComponent input_comp;
    input_comp.actions = actions;
    input_comp.process_input = true;

    world.add_component(entity, input_comp);

    return entity;
}

// Helper function to add action handler to an entity
void add_action_handler(lore::ecs::World& world,
                       lore::ecs::EntityHandle entity,
                       InputAction action,
                       std::function<void(float)> handler) {
    if (!world.has_component<InputComponent>(entity)) {
        return;
    }

    auto& input_comp = world.get_component<InputComponent>(entity);

    // Add action to list if not already there
    auto it = std::find(input_comp.actions.begin(), input_comp.actions.end(), action);
    if (it == input_comp.actions.end()) {
        input_comp.actions.push_back(action);
    }

    // Set handler
    input_comp.action_handlers[action] = std::move(handler);
}

// Helper function to create mouse follower entity
lore::ecs::EntityHandle create_mouse_follower(lore::ecs::World& world,
                                             glm::vec2 offset = glm::vec2(0.0f),
                                             float smoothing = 1.0f) {
    auto entity = world.create_entity();

    MouseFollowerComponent follower_comp;
    follower_comp.offset = offset;
    follower_comp.smoothing = smoothing;
    follower_comp.active = true;

    world.add_component(entity, follower_comp);

    return entity;
}

} // namespace ecs_integration

} // namespace lore::input