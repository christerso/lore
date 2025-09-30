#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>
#include <lore/input/input_listener_manager.hpp>
#include <lore/input/glfw_input_handler.hpp>
#include <lore/input/input_ecs.hpp>
#include <lore/input/input_debug.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <chrono>
#include <atomic>

using namespace lore::input;
using namespace lore::input::debug;
using namespace testing;

// Mock event for testing
class MockEvent : public Event<MockEvent> {
public:
    int value;
    explicit MockEvent(int v) : value(v) {}

    std::string to_string() const override {
        return "MockEvent(value=" + std::to_string(value) + ")";
    }
};

// Test fixture for event system tests
class EventSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatcher = std::make_unique<EventDispatcher>();
    }

    void TearDown() override {
        dispatcher.reset();
    }

    std::unique_ptr<EventDispatcher> dispatcher;
};

// Test fixture for input system integration tests
class InputSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: In real tests, you'd need to mock GLFW or use a headless context
        // For now, we'll test the non-GLFW components
    }

    void TearDown() override {
    }
};

// Event System Tests
TEST_F(EventSystemTest, EventDispatcherCreation) {
    EXPECT_NE(dispatcher.get(), nullptr);
    EXPECT_EQ(dispatcher->get_frame_number(), 0);
}

TEST_F(EventSystemTest, EventPublishingAndSubscription) {
    std::atomic<int> event_received{0};
    int received_value = 0;

    auto handle = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        event_received++;
        received_value = event.value;
    });

    EXPECT_TRUE(handle.is_connected());

    // Publish an event
    dispatcher->publish<MockEvent>(42);
    dispatcher->process_events();

    EXPECT_EQ(event_received.load(), 1);
    EXPECT_EQ(received_value, 42);
}

TEST_F(EventSystemTest, MultipleListeners) {
    std::atomic<int> listener1_count{0};
    std::atomic<int> listener2_count{0};

    auto handle1 = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        listener1_count++;
    });

    auto handle2 = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        listener2_count++;
    });

    dispatcher->publish<MockEvent>(1);
    dispatcher->publish<MockEvent>(2);
    dispatcher->process_events();

    EXPECT_EQ(listener1_count.load(), 2);
    EXPECT_EQ(listener2_count.load(), 2);
}

TEST_F(EventSystemTest, ListenerPriority) {
    std::vector<int> execution_order;

    auto high_priority_handle = dispatcher->subscribe<MockEvent>(
        [&](const MockEvent& event) {
            execution_order.push_back(1);
        }, EventPriority::High);

    auto low_priority_handle = dispatcher->subscribe<MockEvent>(
        [&](const MockEvent& event) {
            execution_order.push_back(2);
        }, EventPriority::Low);

    auto normal_priority_handle = dispatcher->subscribe<MockEvent>(
        [&](const MockEvent& event) {
            execution_order.push_back(3);
        }, EventPriority::Normal);

    dispatcher->publish<MockEvent>(1);
    dispatcher->process_events();

    // High priority should execute first, then normal, then low
    ASSERT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1); // High priority
    EXPECT_EQ(execution_order[1], 3); // Normal priority
    EXPECT_EQ(execution_order[2], 2); // Low priority
}

TEST_F(EventSystemTest, ListenerDisconnection) {
    std::atomic<int> event_count{0};

    auto handle = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        event_count++;
    });

    dispatcher->publish<MockEvent>(1);
    dispatcher->process_events();
    EXPECT_EQ(event_count.load(), 1);

    handle.disconnect();

    dispatcher->publish<MockEvent>(2);
    dispatcher->process_events();
    EXPECT_EQ(event_count.load(), 1); // Should not have increased
}

TEST_F(EventSystemTest, EventQueueThreadSafety) {
    const int num_threads = 4;
    const int events_per_thread = 100;
    std::atomic<int> total_events_received{0};

    auto handle = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        total_events_received++;
    });

    std::vector<std::thread> threads;

    // Start multiple threads publishing events
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < events_per_thread; ++j) {
                dispatcher->publish<MockEvent>(i * events_per_thread + j);
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Process all events
    dispatcher->process_events();

    EXPECT_EQ(total_events_received.load(), num_threads * events_per_thread);
}

TEST_F(EventSystemTest, EventQueueMaxSize) {
    dispatcher->set_max_events_per_frame(5);

    std::atomic<int> events_processed{0};
    auto handle = dispatcher->subscribe<MockEvent>([&](const MockEvent& event) {
        events_processed++;
    });

    // Publish more events than the max per frame
    for (int i = 0; i < 10; ++i) {
        dispatcher->publish<MockEvent>(i);
    }

    dispatcher->process_events();

    // Should only process max events per frame
    EXPECT_EQ(events_processed.load(), 5);
}

// Input Events Tests
TEST(InputEventsTest, KeyEventCreation) {
    KeyPressedEvent event(KeyCode::A, 30, ModifierKey::Shift, false);

    EXPECT_EQ(event.key, KeyCode::A);
    EXPECT_EQ(event.scancode, 30);
    EXPECT_TRUE(has_modifier(event.modifiers, ModifierKey::Shift));
    EXPECT_FALSE(event.is_repeat);
}

TEST(InputEventsTest, MouseEventCreation) {
    glm::vec2 position(100.0f, 200.0f);
    MouseButtonPressedEvent event(MouseButton::Left, position, ModifierKey::Control, 1);

    EXPECT_EQ(event.button, MouseButton::Left);
    EXPECT_EQ(event.position.x, 100.0f);
    EXPECT_EQ(event.position.y, 200.0f);
    EXPECT_TRUE(has_modifier(event.modifiers, ModifierKey::Control));
    EXPECT_EQ(event.click_count, 1);
}

TEST(InputEventsTest, GamepadEventCreation) {
    GamepadButtonPressedEvent event(0, GamepadButton::A);

    EXPECT_EQ(event.gamepad_id, 0);
    EXPECT_EQ(event.button, GamepadButton::A);
}

TEST(InputEventsTest, WindowEventCreation) {
    WindowResizeEvent event(1920, 1080);

    EXPECT_EQ(event.width, 1920);
    EXPECT_EQ(event.height, 1080);
    EXPECT_EQ(event.get_priority(), EventPriority::High);
}

// Event Utils Tests
TEST(EventUtilsTest, KeyCodeStringConversion) {
    EXPECT_EQ(event_utils::keycode_to_string(KeyCode::A), "A");
    EXPECT_EQ(event_utils::keycode_to_string(KeyCode::Space), "Space");
    EXPECT_EQ(event_utils::keycode_to_string(KeyCode::Escape), "Escape");

    EXPECT_EQ(event_utils::string_to_keycode("A"), KeyCode::A);
    EXPECT_EQ(event_utils::string_to_keycode("Space"), KeyCode::Space);
    EXPECT_EQ(event_utils::string_to_keycode("Escape"), KeyCode::Escape);
    EXPECT_EQ(event_utils::string_to_keycode("Invalid"), KeyCode::Unknown);
}

TEST(EventUtilsTest, MouseButtonStringConversion) {
    EXPECT_EQ(event_utils::mouse_button_to_string(MouseButton::Left), "Left");
    EXPECT_EQ(event_utils::mouse_button_to_string(MouseButton::Right), "Right");
    EXPECT_EQ(event_utils::mouse_button_to_string(MouseButton::Middle), "Middle");

    EXPECT_EQ(event_utils::string_to_mouse_button("Left"), MouseButton::Left);
    EXPECT_EQ(event_utils::string_to_mouse_button("Right"), MouseButton::Right);
    EXPECT_EQ(event_utils::string_to_mouse_button("Middle"), MouseButton::Middle);
}

TEST(EventUtilsTest, ModifierKeyOperations) {
    ModifierKey mods = ModifierKey::Shift | ModifierKey::Control;

    EXPECT_TRUE(has_modifier(mods, ModifierKey::Shift));
    EXPECT_TRUE(has_modifier(mods, ModifierKey::Control));
    EXPECT_FALSE(has_modifier(mods, ModifierKey::Alt));

    ModifierKey created = event_utils::create_modifiers(true, true, false, false);
    EXPECT_TRUE(has_modifier(created, ModifierKey::Shift));
    EXPECT_TRUE(has_modifier(created, ModifierKey::Control));
    EXPECT_FALSE(has_modifier(created, ModifierKey::Alt));
    EXPECT_FALSE(has_modifier(created, ModifierKey::Super));
}

// Listener Manager Tests
class ListenerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatcher = std::make_unique<EventDispatcher>();
        listener_manager = std::make_unique<InputListenerManager>(*dispatcher);
    }

    void TearDown() override {
        listener_manager.reset();
        dispatcher.reset();
    }

    std::unique_ptr<EventDispatcher> dispatcher;
    std::unique_ptr<InputListenerManager> listener_manager;
};

TEST_F(ListenerManagerTest, BasicListenerRegistration) {
    std::atomic<bool> handler_called{false};

    auto handle = listener_manager->subscribe<MockEvent>([&](const MockEvent& event) {
        handler_called = true;
    });

    EXPECT_TRUE(handle.is_connected());

    dispatcher->publish<MockEvent>(42);
    dispatcher->process_events();

    EXPECT_TRUE(handler_called.load());
}

TEST_F(ListenerManagerTest, ConditionalListener) {
    std::atomic<int> events_received{0};

    auto handle = listener_manager->subscribe_conditional<MockEvent>(
        [&](const MockEvent& event) {
            events_received++;
        },
        [](const MockEvent& event) {
            return event.value > 50; // Only process events with value > 50
        }
    );

    dispatcher->publish<MockEvent>(25);  // Should be filtered out
    dispatcher->publish<MockEvent>(75);  // Should be processed
    dispatcher->publish<MockEvent>(100); // Should be processed
    dispatcher->process_events();

    EXPECT_EQ(events_received.load(), 2);
}

TEST_F(ListenerManagerTest, OneTimeListener) {
    std::atomic<int> events_received{0};

    auto handle = listener_manager->subscribe_once<MockEvent>([&](const MockEvent& event) {
        events_received++;
    });

    dispatcher->publish<MockEvent>(1);
    dispatcher->publish<MockEvent>(2);
    dispatcher->publish<MockEvent>(3);
    dispatcher->process_events();

    EXPECT_EQ(events_received.load(), 1); // Should only receive one event
}

TEST_F(ListenerManagerTest, ListenerGroups) {
    auto group = listener_manager->create_group("test_group");
    EXPECT_NE(group, nullptr);
    EXPECT_EQ(group->get_name(), "test_group");

    std::atomic<int> events_received{0};

    std::vector<std::function<void(const MockEvent&)>> handlers;
    for (int i = 0; i < 3; ++i) {
        handlers.push_back([&](const MockEvent& event) {
            events_received++;
        });
    }

    auto handles = listener_manager->subscribe_to_group<MockEvent>("test_group", handlers);
    EXPECT_EQ(handles.size(), 3);
    EXPECT_EQ(group->size(), 3);

    dispatcher->publish<MockEvent>(42);
    dispatcher->process_events();

    EXPECT_EQ(events_received.load(), 3);

    // Disconnect group
    listener_manager->disconnect_group("test_group");
    events_received = 0;

    dispatcher->publish<MockEvent>(43);
    dispatcher->process_events();

    EXPECT_EQ(events_received.load(), 0); // No events should be received
}

TEST_F(ListenerManagerTest, KeyConvenienceHandlers) {
    std::atomic<bool> key_pressed{false};
    std::atomic<bool> key_released{false};

    auto press_handle = listener_manager->on_key_pressed(KeyCode::A, [&]() {
        key_pressed = true;
    });

    auto release_handle = listener_manager->on_key_released(KeyCode::A, [&]() {
        key_released = true;
    });

    dispatcher->publish<KeyPressedEvent>(KeyCode::A, 30, ModifierKey::None, false);
    dispatcher->publish<KeyReleasedEvent>(KeyCode::A, 30, ModifierKey::None);
    dispatcher->process_events();

    EXPECT_TRUE(key_pressed.load());
    EXPECT_TRUE(key_released.load());
}

TEST_F(ListenerManagerTest, MouseConvenienceHandlers) {
    std::atomic<bool> clicked{false};
    glm::vec2 click_position{0.0f};

    auto handle = listener_manager->on_mouse_clicked(MouseButton::Left, [&](glm::vec2 pos) {
        clicked = true;
        click_position = pos;
    });

    glm::vec2 test_pos(100.0f, 200.0f);
    dispatcher->publish<MouseButtonPressedEvent>(MouseButton::Left, test_pos, ModifierKey::None, 1);
    dispatcher->process_events();

    EXPECT_TRUE(clicked.load());
    EXPECT_EQ(click_position.x, 100.0f);
    EXPECT_EQ(click_position.y, 200.0f);
}

// Debug System Tests
class DebugSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<InputDebugMonitor>("TestMonitor");
        monitor->set_debug_level(DebugLevel::Debug);
        monitor->set_output_mode(DebugOutputMode::None); // Don't spam console during tests
    }

    void TearDown() override {
        monitor.reset();
    }

    std::unique_ptr<InputDebugMonitor> monitor;
};

TEST_F(DebugSystemTest, EventRecording) {
    EXPECT_FALSE(monitor->is_recording());

    monitor->start_recording();
    EXPECT_TRUE(monitor->is_recording());

    auto event = std::make_unique<MockEvent>(42);
    monitor->record_event(std::move(event), "TestSource");

    const auto& records = monitor->get_event_records();
    EXPECT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].event_source, "TestSource");

    monitor->stop_recording();
    EXPECT_FALSE(monitor->is_recording());
}

TEST_F(DebugSystemTest, PerformanceMetrics) {
    InputPerformanceMetrics metrics;
    metrics.events_processed_per_second = 1000;
    metrics.average_event_processing_time_ms = 0.5f;
    metrics.active_listeners = 10;

    monitor->update_performance_metrics(metrics);

    const auto& current_metrics = monitor->get_performance_metrics();
    EXPECT_EQ(current_metrics.events_processed_per_second, 1000);
    EXPECT_FLOAT_EQ(current_metrics.average_event_processing_time_ms, 0.5f);
    EXPECT_EQ(current_metrics.active_listeners, 10);
}

TEST_F(DebugSystemTest, DebugConfiguration) {
    InputDebugConfig config;
    config.debug_level = DebugLevel::Warning;
    config.output_mode = DebugOutputMode::File;
    config.recording_enabled = true;
    config.max_event_records = 5000;

    std::string config_str = config.save_to_string();
    EXPECT_FALSE(config_str.empty());

    InputDebugConfig loaded_config;
    loaded_config.load_from_string(config_str);

    EXPECT_EQ(loaded_config.debug_level, DebugLevel::Warning);
    EXPECT_EQ(loaded_config.output_mode, DebugOutputMode::File);
    EXPECT_TRUE(loaded_config.recording_enabled);
    EXPECT_EQ(loaded_config.max_event_records, 5000);
}

TEST_F(DebugSystemTest, DebugConsole) {
    InputDebugConsole console;
    console.attach_monitor(monitor.get());

    std::string result = console.execute_command("help");
    EXPECT_FALSE(result.empty());
    EXPECT_THAT(result, HasSubstr("Available commands"));

    result = console.execute_command("status");
    EXPECT_FALSE(result.empty());

    result = console.execute_command("record start");
    EXPECT_TRUE(monitor->is_recording());

    result = console.execute_command("record stop");
    EXPECT_FALSE(monitor->is_recording());
}

// Integration Tests
class InputSystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: These tests would require a mock GLFW context in a real scenario
        // For now, we test the components that don't require GLFW
    }

    void TearDown() override {
    }
};

TEST_F(InputSystemIntegrationTest, EndToEndEventFlow) {
    // Create a complete input pipeline
    EventDispatcher dispatcher;
    InputListenerManager listener_manager(dispatcher);

    std::atomic<bool> keyboard_event_received{false};
    std::atomic<bool> mouse_event_received{false};

    // Setup listeners
    auto keyboard_handle = listener_manager.on_key_pressed(KeyCode::Space, [&]() {
        keyboard_event_received = true;
    });

    auto mouse_handle = listener_manager.on_mouse_clicked(MouseButton::Left, [&](glm::vec2 pos) {
        mouse_event_received = true;
    });

    // Simulate events
    dispatcher.publish<KeyPressedEvent>(KeyCode::Space, 57, ModifierKey::None, false);
    dispatcher.publish<MouseButtonPressedEvent>(MouseButton::Left, glm::vec2(100, 100), ModifierKey::None, 1);

    // Process events
    dispatcher.process_events();

    EXPECT_TRUE(keyboard_event_received.load());
    EXPECT_TRUE(mouse_event_received.load());
}

TEST_F(InputSystemIntegrationTest, ECSInputIntegration) {
    // Test ECS integration without requiring GLFW
    // This would be more comprehensive with a mock input handler
    auto world = std::make_unique<lore::ecs::World>();

    // Create entity with input component
    auto entity = world->create_entity();
    world->add_component<InputComponent>(entity);

    auto* input = world->get_component<InputComponent>(entity);
    EXPECT_NE(input, nullptr);
    EXPECT_TRUE(input->enabled);
}

// Performance Tests
TEST(PerformanceTest, HighVolumeEventProcessing) {
    EventDispatcher dispatcher;
    dispatcher.set_max_events_per_frame(10000);

    std::atomic<int> events_processed{0};
    auto handle = dispatcher.subscribe<MockEvent>([&](const MockEvent& event) {
        events_processed++;
    });

    const int num_events = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();

    // Publish many events
    for (int i = 0; i < num_events; ++i) {
        dispatcher.publish<MockEvent>(i);
    }

    // Process all events
    dispatcher.process_events();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    EXPECT_EQ(events_processed.load(), num_events);

    // Performance expectation: should process 10k events in less than 10ms
    EXPECT_LT(duration.count(), 10000); // microseconds

    std::cout << "Processed " << num_events << " events in " << duration.count() << " microseconds" << std::endl;
}

TEST(PerformanceTest, ListenerRegistrationPerformance) {
    EventDispatcher dispatcher;
    InputListenerManager listener_manager(dispatcher);

    const int num_listeners = 1000;
    std::vector<ManagedListenerHandle> handles;
    handles.reserve(num_listeners);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Register many listeners
    for (int i = 0; i < num_listeners; ++i) {
        handles.emplace_back(listener_manager.subscribe<MockEvent>([](const MockEvent& event) {
            // Do nothing
        }));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    EXPECT_EQ(handles.size(), num_listeners);

    // Performance expectation: should register 1k listeners in less than 1ms
    EXPECT_LT(duration.count(), 1000); // microseconds

    std::cout << "Registered " << num_listeners << " listeners in " << duration.count() << " microseconds" << std::endl;
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}