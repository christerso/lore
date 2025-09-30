#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>
#include <lore/input/input_listener_manager.hpp>
#include <lore/input/glfw_input_handler.hpp>
#include <lore/input/input_ecs.hpp>
#include <lore/input/input_debug.hpp>
#include <lore/ecs/ecs.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

using namespace lore::input;
using namespace lore::input::debug;

// Example game class demonstrating complete input system usage
class InputSystemExample {
public:
    InputSystemExample() : world_(std::make_unique<lore::ecs::World>()) {}

    bool initialize() {
        std::cout << "=== Lore Input System Example ===" << std::endl;

        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window_ = glfwCreateWindow(1200, 800, "Lore Input System Example", nullptr, nullptr);
        if (!window_) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        // Initialize input system
        input_system_ = std::make_unique<GLFWInputSystem>();
        if (!input_system_->initialize(window_)) {
            std::cerr << "Failed to initialize input system" << std::endl;
            return false;
        }

        // Initialize ECS input system
        ecs_input_system_ = std::make_unique<InputECSSystem>(*input_system_);
        ecs_input_system_->init(*world_);

        // Initialize debugging
        global::initialize_input_debugging("debug_config.txt");
        global::get_debug_console().attach_input_handler(&input_system_->get_input_handler());
        global::get_debug_console().attach_ecs_system(ecs_input_system_.get());
        global::start_recording();

        // Setup example entities and input handlers
        setup_example_entities();
        setup_input_listeners();
        setup_debug_commands();

        std::cout << "Input system initialized successfully!" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move player" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  Space - Jump" << std::endl;
        std::cout << "  Escape - Quit" << std::endl;
        std::cout << "  F1 - Toggle debug mode" << std::endl;
        std::cout << "  F2 - Take debug snapshot" << std::endl;
        std::cout << "  F3 - Generate debug report" << std::endl;
        std::cout << "  ` (tilde) - Open debug console" << std::endl;

        return true;
    }

    void run() {
        auto last_time = std::chrono::high_resolution_clock::now();
        std::size_t frame_count = 0;

        while (!glfwWindowShouldClose(window_) && !should_quit_) {
            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            // Update input system
            input_system_->update(delta_time);

            // Update ECS input system
            ecs_input_system_->update(*world_, delta_time);

            // Update debug monitoring
            update_debug_monitoring();

            // Process any debug console commands
            process_debug_console();

            // Render (placeholder)
            render_frame();

            ++frame_count;

            // Print status every 60 frames
            if (frame_count % 60 == 0) {
                print_status();
            }

            // Limit framerate
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }

    void shutdown() {
        std::cout << "Shutting down input system example..." << std::endl;

        // Generate final debug report
        global::generate_report("final_debug_report.txt");

        // Shutdown systems
        if (ecs_input_system_) {
            ecs_input_system_->shutdown(*world_);
        }

        input_system_.reset();

        global::shutdown_input_debugging();

        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();

        std::cout << "Shutdown complete" << std::endl;
    }

private:
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<lore::ecs::World> world_;
    std::unique_ptr<GLFWInputSystem> input_system_;
    std::unique_ptr<InputECSSystem> ecs_input_system_;

    // Example entities
    lore::ecs::Entity player_entity_ = lore::ecs::kInvalidEntity;
    lore::ecs::Entity camera_entity_ = lore::ecs::kInvalidEntity;
    lore::ecs::Entity ui_button_entity_ = lore::ecs::kInvalidEntity;

    // Example state
    bool should_quit_ = false;
    bool debug_console_open_ = false;
    glm::vec3 player_position_{0.0f, 0.0f, 0.0f};
    float player_speed_ = 5.0f;

    void setup_example_entities() {
        // Create player entity with input component
        player_entity_ = world_->create_entity();
        world_->add_component<TransformComponent>(player_entity_);
        world_->add_component<InputComponent>(player_entity_);

        auto* player_transform = world_->get_component<TransformComponent>(player_entity_);
        player_transform->position = player_position_;

        auto* player_input = world_->get_component<InputComponent>(player_entity_);
        player_input->enabled = true;
        player_input->priority = EventPriority::High;
        player_input->consume_events = true;

        // Setup player movement handlers
        player_input->action_handlers[InputAction::MoveForward] = [this](float value) {
            auto* transform = world_->get_component<TransformComponent>(player_entity_);
            transform->position.z -= value * player_speed_ * 0.016f; // Assuming ~60 FPS
            transform->mark_dirty();
            std::cout << "Player moving forward: " << value << std::endl;
        };

        player_input->action_handlers[InputAction::MoveBackward] = [this](float value) {
            auto* transform = world_->get_component<TransformComponent>(player_entity_);
            transform->position.z += value * player_speed_ * 0.016f;
            transform->mark_dirty();
            std::cout << "Player moving backward: " << value << std::endl;
        };

        player_input->action_handlers[InputAction::MoveLeft] = [this](float value) {
            auto* transform = world_->get_component<TransformComponent>(player_entity_);
            transform->position.x -= value * player_speed_ * 0.016f;
            transform->mark_dirty();
            std::cout << "Player moving left: " << value << std::endl;
        };

        player_input->action_handlers[InputAction::MoveRight] = [this](float value) {
            auto* transform = world_->get_component<TransformComponent>(player_entity_);
            transform->position.x += value * player_speed_ * 0.016f;
            transform->mark_dirty();
            std::cout << "Player moving right: " << value << std::endl;
        };

        player_input->action_handlers[InputAction::Jump] = [this](float value) {
            if (value > 0.5f) { // Only on press, not release
                auto* transform = world_->get_component<TransformComponent>(player_entity_);
                transform->position.y += 2.0f;
                transform->mark_dirty();
                std::cout << "Player jumping!" << std::endl;
            }
        };

        // Setup key handlers
        player_input->key_handlers[KeyCode::Space] = [this](bool pressed) {
            if (pressed) {
                auto* transform = world_->get_component<TransformComponent>(player_entity_);
                transform->position.y += 1.0f;
                transform->mark_dirty();
                std::cout << "Space pressed - player jump!" << std::endl;
            }
        };

        // Setup mouse handlers
        player_input->mouse_move_handler = [this](glm::vec2 position, glm::vec2 delta) {
            // Simple mouse look
            static float yaw = 0.0f;
            static float pitch = 0.0f;

            yaw += delta.x * 0.001f;
            pitch += delta.y * 0.001f;

            // Clamp pitch
            pitch = std::clamp(pitch, -1.5f, 1.5f);

            std::cout << "Mouse look - Yaw: " << yaw << ", Pitch: " << pitch << std::endl;
        };

        // Create camera entity
        camera_entity_ = world_->create_entity();
        world_->add_component<CameraComponent>(camera_entity_);
        world_->add_component<InputComponent>(camera_entity_);

        auto* camera = world_->get_component<CameraComponent>(camera_entity_);
        camera->is_active = true;
        camera->position = glm::vec3(0.0f, 5.0f, 10.0f);
        camera->target = glm::vec3(0.0f, 0.0f, 0.0f);

        // Create UI button entity
        ui_button_entity_ = world_->create_entity();
        world_->add_component<UIInputComponent>(ui_button_entity_);
        world_->add_component<FocusableComponent>(ui_button_entity_);

        auto* ui = world_->get_component<UIInputComponent>(ui_button_entity_);
        ui->enabled = true;
        ui->visible = true;
        ui->position = glm::vec2(100.0f, 100.0f);
        ui->size = glm::vec2(200.0f, 50.0f);
        ui->on_click = [this](glm::vec2 position) {
            std::cout << "UI Button clicked at: (" << position.x << ", " << position.y << ")" << std::endl;
        };

        auto* focus = world_->get_component<FocusableComponent>(ui_button_entity_);
        focus->can_receive_focus = true;
        focus->focus_bounds_min = ui->position;
        focus->focus_bounds_max = ui->position + ui->size;
        focus->focus_priority = 100;

        // Register entities with input system
        ecs_input_system_->register_entity_for_input(player_entity_);
        ecs_input_system_->register_entity_for_input(camera_entity_);

        std::cout << "Example entities created and configured" << std::endl;
    }

    void setup_input_listeners() {
        auto& listener_manager = ecs_input_system_->get_listener_manager();

        // Global escape key handler
        auto escape_handle = listener_manager.on_key_pressed(KeyCode::Escape, [this]() {
            std::cout << "Escape pressed - quitting application" << std::endl;
            should_quit_ = true;
        }, listener_configs::HIGH_PRIORITY);

        // Debug key handlers
        auto f1_handle = listener_manager.on_key_pressed(KeyCode::F1, [this]() {
            bool debug_mode = ecs_input_system_->is_debug_mode();
            ecs_input_system_->set_debug_mode(!debug_mode);
            std::cout << "Debug mode " << (!debug_mode ? "enabled" : "disabled") << std::endl;
        });

        auto f2_handle = listener_manager.on_key_pressed(KeyCode::F2, [this]() {
            global::take_snapshot(input_system_->get_input_handler(), ecs_input_system_.get());
            std::cout << "Debug snapshot taken" << std::endl;
        });

        auto f3_handle = listener_manager.on_key_pressed(KeyCode::F3, [this]() {
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            std::string filename = "debug_report_" + std::to_string(timestamp) + ".txt";
            global::generate_report(filename);
            std::cout << "Debug report generated: " << filename << std::endl;
        });

        // Console toggle
        auto console_handle = listener_manager.on_key_pressed(KeyCode::GraveAccent, [this]() {
            debug_console_open_ = !debug_console_open_;
            std::cout << "Debug console " << (debug_console_open_ ? "opened" : "closed") << std::endl;
            if (debug_console_open_) {
                std::cout << "Type 'help' for available commands" << std::endl;
            }
        });

        // Movement key mappings (using key combo for demo)
        std::vector<KeyCode> wasd_keys = {KeyCode::W, KeyCode::A, KeyCode::S, KeyCode::D};
        auto wasd_handle = listener_manager.on_key_combination(wasd_keys, [this]() {
            std::cout << "WASD combination detected!" << std::endl;
        });

        // Mouse click handler for UI interaction
        auto click_handle = listener_manager.subscribe<MouseButtonPressedEvent>([this](const MouseButtonPressedEvent& event) {
            std::cout << "Mouse click at (" << event.position.x << ", " << event.position.y << ")" << std::endl;

            // Handle UI clicks
            ecs_input_system_->handle_ui_click(*world_, event.position, event.button);

            // Update focus based on click position
            ecs_input_system_->update_focus_from_mouse_position(*world_, event.position);
        });

        // Window events
        auto resize_handle = listener_manager.subscribe<WindowResizeEvent>([this](const WindowResizeEvent& event) {
            std::cout << "Window resized to " << event.width << "x" << event.height << std::endl;
        });

        auto close_handle = listener_manager.subscribe<WindowCloseEvent>([this](const WindowCloseEvent& event) {
            std::cout << "Window close requested" << std::endl;
            should_quit_ = true;
        });

        // Gamepad events
        auto gamepad_connect_handle = listener_manager.subscribe<GamepadConnectionEvent>([this](const GamepadConnectionEvent& event) {
            std::cout << "Gamepad " << event.gamepad_id << " "
                     << (event.connected ? "connected" : "disconnected");
            if (event.connected && !event.name.empty()) {
                std::cout << " (" << event.name << ")";
            }
            std::cout << std::endl;
        });

        std::cout << "Input listeners configured" << std::endl;
    }

    void setup_debug_commands() {
        auto& console = global::get_debug_console();

        // Custom command for player teleport
        console.register_command("teleport", [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() != 3) {
                return "Usage: teleport <x> <y> <z>";
            }

            try {
                float x = std::stof(args[0]);
                float y = std::stof(args[1]);
                float z = std::stof(args[2]);

                auto* transform = world_->get_component<TransformComponent>(player_entity_);
                if (transform) {
                    transform->position = glm::vec3(x, y, z);
                    transform->mark_dirty();
                    return "Player teleported to (" + args[0] + ", " + args[1] + ", " + args[2] + ")";
                } else {
                    return "Error: Player entity has no transform component";
                }
            } catch (const std::exception& e) {
                return "Error: Invalid coordinates";
            }
        }, "Teleport player to specified coordinates");

        // Custom command for input simulation
        console.register_command("simulate", [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty()) {
                return "Usage: simulate <event_type> [args...]";
            }

            std::string event_type = args[0];
            auto& dispatcher = input_system_->get_event_dispatcher();

            if (event_type == "key_press" && args.size() >= 2) {
                KeyCode key = event_utils::string_to_keycode(args[1]);
                dispatcher.publish<KeyPressedEvent>(key, 0, ModifierKey::None, false);
                return "Simulated key press: " + args[1];
            } else if (event_type == "mouse_click" && args.size() >= 3) {
                float x = std::stof(args[1]);
                float y = std::stof(args[2]);
                dispatcher.publish<MouseButtonPressedEvent>(MouseButton::Left, glm::vec2(x, y), ModifierKey::None, 1);
                return "Simulated mouse click at (" + args[1] + ", " + args[2] + ")";
            } else {
                return "Error: Unknown event type or insufficient arguments";
            }
        }, "Simulate input events for testing");

        std::cout << "Debug commands registered" << std::endl;
    }

    void update_debug_monitoring() {
        // Update performance metrics
        auto stats = input_system_->get_statistics();

        InputPerformanceMetrics metrics;
        metrics.events_processed_per_second = stats.events_per_second;
        metrics.average_event_processing_time_ms = stats.average_processing_time_ms;
        metrics.current_queue_size = stats.events_queued;
        metrics.active_listeners = stats.listeners_active;
        metrics.input_system_frame_time_ms = 1.0f; // Placeholder

        global::get_debug_monitor().update_performance_metrics(metrics);

        // Periodic snapshots in debug mode
        if (ecs_input_system_->is_debug_mode()) {
            static int snapshot_counter = 0;
            if (++snapshot_counter % 300 == 0) { // Every 5 seconds at 60 FPS
                global::take_snapshot(input_system_->get_input_handler(), ecs_input_system_.get());
            }
        }
    }

    void process_debug_console() {
        if (!debug_console_open_) return;

        // In a real application, you'd have a proper console UI
        // For this example, we'll just process commands from stdin occasionally
        static int console_check_counter = 0;
        if (++console_check_counter % 60 != 0) return; // Check every second

        // Simple stdin command processing (non-blocking would be better)
        // This is just for demonstration
    }

    void render_frame() {
        // Placeholder rendering
        // In a real application, this would render the scene with Vulkan

        // Just clear the screen color for now
        static int frame_counter = 0;
        if (++frame_counter % 300 == 0) { // Every 5 seconds
            std::cout << "Rendering frame " << frame_counter << " - Player at ("
                     << player_position_.x << ", " << player_position_.y << ", " << player_position_.z << ")" << std::endl;
        }
    }

    void print_status() {
        auto input_stats = input_system_->get_statistics();
        auto ecs_stats = ecs_input_system_->get_statistics(*world_);
        auto listener_stats = ecs_input_system_->get_listener_manager().get_statistics();

        std::cout << "\n=== Status Update ===" << std::endl;
        std::cout << "Input System:" << std::endl;
        std::cout << "  Events/sec: " << input_stats.events_per_second << std::endl;
        std::cout << "  Queue size: " << input_stats.events_queued << std::endl;
        std::cout << "  Listeners: " << input_stats.listeners_active << std::endl;

        std::cout << "ECS Input System:" << std::endl;
        std::cout << "  Input entities: " << ecs_stats.entities_with_input << std::endl;
        std::cout << "  UI entities: " << ecs_stats.ui_entities << std::endl;
        std::cout << "  Focused entity: " << static_cast<std::uint32_t>(ecs_input_system_->get_focused_entity()) << std::endl;

        std::cout << "Debug:" << std::endl;
        std::cout << "  Recording: " << (global::get_debug_monitor().is_recording() ? "Yes" : "No") << std::endl;
        std::cout << "  Events recorded: " << global::get_debug_monitor().get_statistics().total_events_recorded << std::endl;
        std::cout << "===================" << std::endl;
    }
};

int main() {
    InputSystemExample example;

    if (!example.initialize()) {
        std::cerr << "Failed to initialize input system example" << std::endl;
        return -1;
    }

    example.run();
    example.shutdown();

    return 0;
}