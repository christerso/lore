#pragma once

#include <lore/input/event_system.hpp>
#include <lore/input/input_events.hpp>
#include <lore/input/input_listener_manager.hpp>
#include <lore/input/glfw_input_handler.hpp>
#include <lore/input/input_ecs.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <fstream>
#include <atomic>
#include <mutex>

namespace lore::input::debug {

// Debug output modes
enum class DebugOutputMode {
    Console,
    File,
    Both,
    None
};

// Debug level filtering
enum class DebugLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4
};

// Input event recording for replay and analysis
struct EventRecord {
    std::unique_ptr<IEvent> event;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::uint64_t frame_number;
    std::string event_source;  // "GLFW", "System", "Generated", etc.
};

// Performance metrics for input system
struct InputPerformanceMetrics {
    // Event processing metrics
    std::size_t events_processed_per_second = 0;
    float average_event_processing_time_ms = 0.0f;
    float max_event_processing_time_ms = 0.0f;
    float min_event_processing_time_ms = 0.0f;

    // Queue metrics
    std::size_t max_queue_size = 0;
    std::size_t current_queue_size = 0;
    std::size_t total_events_dropped = 0;

    // Listener metrics
    std::size_t active_listeners = 0;
    std::size_t total_listeners_created = 0;
    std::size_t listeners_auto_removed = 0;

    // Memory usage
    std::size_t estimated_memory_usage_bytes = 0;

    // Frame timing
    float input_system_frame_time_ms = 0.0f;
    float total_frame_time_ms = 0.0f;
    float input_percentage_of_frame = 0.0f;
};

// Input state snapshot for debugging
struct InputStateSnapshot {
    std::chrono::high_resolution_clock::time_point timestamp;
    std::uint64_t frame_number;

    // Keyboard state
    std::unordered_map<KeyCode, bool> keyboard_state;
    std::vector<KeyCode> keys_pressed_this_frame;
    std::vector<KeyCode> keys_released_this_frame;

    // Mouse state
    glm::vec2 mouse_position;
    glm::vec2 mouse_delta;
    std::unordered_map<MouseButton, bool> mouse_button_state;
    std::vector<MouseButton> mouse_buttons_pressed_this_frame;
    std::vector<MouseButton> mouse_buttons_released_this_frame;

    // Gamepad state
    struct GamepadSnapshot {
        bool connected;
        std::string name;
        std::unordered_map<GamepadButton, bool> button_state;
        std::unordered_map<GamepadAxis, float> axis_state;
    };
    std::array<GamepadSnapshot, 16> gamepads;

    // Focus and UI state
    lore::ecs::Entity focused_entity;
    std::vector<lore::ecs::Entity> hovered_ui_entities;
};

// Event filter for debugging specific events
class DebugEventFilter {
public:
    DebugEventFilter() = default;

    // Event type filtering
    void add_event_type(std::type_index type) { allowed_types_.insert(type); }
    void remove_event_type(std::type_index type) { allowed_types_.erase(type); }
    void clear_event_types() { allowed_types_.clear(); }
    bool is_event_type_allowed(std::type_index type) const {
        return allowed_types_.empty() || allowed_types_.count(type) > 0;
    }

    // Priority filtering
    void set_min_priority(EventPriority priority) { min_priority_ = priority; }
    EventPriority get_min_priority() const { return min_priority_; }

    // Frame range filtering
    void set_frame_range(std::uint64_t min_frame, std::uint64_t max_frame) {
        min_frame_ = min_frame;
        max_frame_ = max_frame;
    }
    bool is_frame_in_range(std::uint64_t frame) const {
        return frame >= min_frame_ && frame <= max_frame_;
    }

    // Custom filter function
    void set_custom_filter(std::function<bool(const IEvent&)> filter) {
        custom_filter_ = std::move(filter);
    }

    // Apply all filters
    bool should_include_event(const IEvent& event) const {
        if (!is_event_type_allowed(event.get_type())) return false;
        if (event.get_priority() < min_priority_) return false;
        if (!is_frame_in_range(event.get_frame_number())) return false;
        if (custom_filter_ && !custom_filter_(event)) return false;
        return true;
    }

private:
    std::unordered_set<std::type_index> allowed_types_;
    EventPriority min_priority_ = EventPriority::Lowest;
    std::uint64_t min_frame_ = 0;
    std::uint64_t max_frame_ = std::numeric_limits<std::uint64_t>::max();
    std::function<bool(const IEvent&)> custom_filter_;
};

// Main debug monitor class
class InputDebugMonitor {
public:
    explicit InputDebugMonitor(std::string name = "InputDebugMonitor");
    ~InputDebugMonitor();

    // Non-copyable, movable
    InputDebugMonitor(const InputDebugMonitor&) = delete;
    InputDebugMonitor& operator=(const InputDebugMonitor&) = delete;
    InputDebugMonitor(InputDebugMonitor&&) = default;
    InputDebugMonitor& operator=(InputDebugMonitor&&) = default;

    // Configuration
    void set_debug_level(DebugLevel level) { debug_level_ = level; }
    DebugLevel get_debug_level() const { return debug_level_; }

    void set_output_mode(DebugOutputMode mode) { output_mode_ = mode; }
    DebugOutputMode get_output_mode() const { return output_mode_; }

    void set_log_file_path(const std::string& path);
    const std::string& get_log_file_path() const { return log_file_path_; }

    void set_max_event_records(std::size_t max) { max_event_records_ = max; }
    std::size_t get_max_event_records() const { return max_event_records_; }

    // Event recording
    void start_recording() { recording_enabled_ = true; }
    void stop_recording() { recording_enabled_ = false; }
    bool is_recording() const { return recording_enabled_; }

    void record_event(std::unique_ptr<IEvent> event, const std::string& source = "Unknown");
    void clear_event_records();

    const std::vector<EventRecord>& get_event_records() const { return event_records_; }
    std::vector<EventRecord> get_filtered_event_records(const DebugEventFilter& filter) const;

    // State snapshots
    void take_input_state_snapshot(const GLFWInputHandler& input_handler,
                                  const InputECSSystem* ecs_system = nullptr);
    const std::vector<InputStateSnapshot>& get_state_snapshots() const { return state_snapshots_; }
    void clear_state_snapshots() { state_snapshots_.clear(); }

    // Performance monitoring
    void update_performance_metrics(const InputPerformanceMetrics& metrics);
    const InputPerformanceMetrics& get_performance_metrics() const { return current_metrics_; }
    std::vector<InputPerformanceMetrics> get_performance_history() const { return metrics_history_; }

    // Logging
    void log(DebugLevel level, const std::string& message) const;
    void log_event(const IEvent& event, const std::string& context = "") const;
    void log_performance_summary() const;
    void log_listener_summary(const InputListenerManager& listener_manager) const;

    // Reporting
    void generate_debug_report(const std::string& file_path) const;
    std::string generate_event_summary() const;
    std::string generate_performance_summary() const;
    std::string generate_state_summary() const;

    // Real-time monitoring
    void enable_real_time_monitoring(bool enabled) { real_time_monitoring_ = enabled; }
    bool is_real_time_monitoring_enabled() const { return real_time_monitoring_; }

    // Event replay
    void save_event_sequence(const std::string& file_path) const;
    bool load_event_sequence(const std::string& file_path);
    void replay_events(EventDispatcher& dispatcher, float time_scale = 1.0f) const;

    // Statistics
    struct DebugStatistics {
        std::size_t total_events_recorded = 0;
        std::size_t events_by_type[32] = {}; // Assuming max 32 event types for simplicity
        std::size_t snapshots_taken = 0;
        std::size_t log_messages_written = 0;
        std::chrono::high_resolution_clock::time_point monitoring_start_time;
        std::chrono::milliseconds total_monitoring_time{0};
    };

    const DebugStatistics& get_statistics() const { return statistics_; }
    void reset_statistics();

private:
    std::string name_;
    DebugLevel debug_level_ = DebugLevel::Info;
    DebugOutputMode output_mode_ = DebugOutputMode::Console;
    std::string log_file_path_;
    mutable std::ofstream log_file_;

    // Event recording
    bool recording_enabled_ = false;
    std::size_t max_event_records_ = 10000;
    std::vector<EventRecord> event_records_;
    mutable std::mutex event_records_mutex_;

    // State snapshots
    std::vector<InputStateSnapshot> state_snapshots_;
    std::size_t max_state_snapshots_ = 1000;

    // Performance monitoring
    InputPerformanceMetrics current_metrics_;
    std::vector<InputPerformanceMetrics> metrics_history_;
    std::size_t max_metrics_history_ = 3600; // 1 hour at 60 FPS

    // Real-time monitoring
    bool real_time_monitoring_ = false;

    // Statistics
    mutable DebugStatistics statistics_;

    // Helper methods
    void write_to_output(DebugLevel level, const std::string& message) const;
    void ensure_log_file_open() const;
    std::string format_timestamp() const;
    std::string debug_level_to_string(DebugLevel level) const;
    void cleanup_old_records();
    void cleanup_old_snapshots();
    void cleanup_old_metrics();
};

// Debug console for interactive debugging
class InputDebugConsole {
public:
    InputDebugConsole();
    ~InputDebugConsole() = default;

    // Command registration
    using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;
    void register_command(const std::string& name, CommandHandler handler, const std::string& help = "");
    void unregister_command(const std::string& name);

    // Command execution
    std::string execute_command(const std::string& command_line);
    std::vector<std::string> get_available_commands() const;
    std::string get_command_help(const std::string& command) const;

    // Integration with debug monitor
    void attach_monitor(InputDebugMonitor* monitor) { monitor_ = monitor; }
    void attach_listener_manager(InputListenerManager* manager) { listener_manager_ = manager; }
    void attach_input_handler(GLFWInputHandler* handler) { input_handler_ = handler; }
    void attach_ecs_system(InputECSSystem* ecs_system) { ecs_system_ = ecs_system; }

    // Command history
    const std::vector<std::string>& get_command_history() const { return command_history_; }
    void clear_command_history() { command_history_.clear(); }

private:
    struct CommandInfo {
        CommandHandler handler;
        std::string help;
    };

    std::unordered_map<std::string, CommandInfo> commands_;
    std::vector<std::string> command_history_;

    // System attachments
    InputDebugMonitor* monitor_ = nullptr;
    InputListenerManager* listener_manager_ = nullptr;
    GLFWInputHandler* input_handler_ = nullptr;
    InputECSSystem* ecs_system_ = nullptr;

    // Built-in commands
    void register_builtin_commands();
    std::string cmd_help(const std::vector<std::string>& args);
    std::string cmd_status(const std::vector<std::string>& args);
    std::string cmd_events(const std::vector<std::string>& args);
    std::string cmd_listeners(const std::vector<std::string>& args);
    std::string cmd_performance(const std::vector<std::string>& args);
    std::string cmd_snapshot(const std::vector<std::string>& args);
    std::string cmd_record(const std::vector<std::string>& args);
    std::string cmd_log_level(const std::vector<std::string>& args);
    std::string cmd_clear(const std::vector<std::string>& args);

    // Utility methods
    std::vector<std::string> split_command_line(const std::string& command_line) const;
    std::string join_args(const std::vector<std::string>& args, std::size_t start_index = 0) const;
};

// Configuration system for input debugging
class InputDebugConfig {
public:
    InputDebugConfig() = default;
    ~InputDebugConfig() = default;

    // Configuration loading/saving
    bool load_from_file(const std::string& file_path);
    bool save_to_file(const std::string& file_path) const;
    void load_from_string(const std::string& config_data);
    std::string save_to_string() const;

    // Debug settings
    DebugLevel debug_level = DebugLevel::Info;
    DebugOutputMode output_mode = DebugOutputMode::Console;
    std::string log_file_path = "input_debug.log";
    bool recording_enabled = false;
    std::size_t max_event_records = 10000;
    std::size_t max_state_snapshots = 1000;
    bool real_time_monitoring = false;

    // Event filtering
    DebugEventFilter event_filter;

    // Performance monitoring
    bool performance_monitoring_enabled = true;
    std::size_t performance_metrics_history_size = 3600;

    // Console settings
    bool console_enabled = true;
    std::string console_prompt = "> ";

    // Hot-reload settings
    bool auto_reload_config = false;
    std::string config_file_path;

    // Validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;

private:
    // Helper methods for serialization
    void parse_debug_level(const std::string& value);
    void parse_output_mode(const std::string& value);
    std::string debug_level_to_string() const;
    std::string output_mode_to_string() const;
};

// Global debug instance management
namespace global {
    extern std::unique_ptr<InputDebugMonitor> g_debug_monitor;
    extern std::unique_ptr<InputDebugConsole> g_debug_console;
    extern InputDebugConfig g_debug_config;

    // Initialization
    void initialize_input_debugging(const std::string& config_file = "");
    void shutdown_input_debugging();

    // Convenience functions
    InputDebugMonitor& get_debug_monitor();
    InputDebugConsole& get_debug_console();
    InputDebugConfig& get_debug_config();

    // Quick logging
    void debug_log(DebugLevel level, const std::string& message);
    void debug_log_event(const IEvent& event, const std::string& context = "");

    // Quick commands
    void start_recording();
    void stop_recording();
    void take_snapshot(const GLFWInputHandler& input_handler, const InputECSSystem* ecs_system = nullptr);
    void generate_report(const std::string& file_path);
}

// Macro helpers for conditional debugging
#ifdef LORE_DEBUG_INPUT
    #define LORE_INPUT_DEBUG_LOG(level, message) \
        lore::input::debug::global::debug_log(level, message)

    #define LORE_INPUT_DEBUG_EVENT(event, context) \
        lore::input::debug::global::debug_log_event(event, context)

    #define LORE_INPUT_DEBUG_SNAPSHOT(handler, ecs) \
        lore::input::debug::global::take_snapshot(handler, ecs)
#else
    #define LORE_INPUT_DEBUG_LOG(level, message) ((void)0)
    #define LORE_INPUT_DEBUG_EVENT(event, context) ((void)0)
    #define LORE_INPUT_DEBUG_SNAPSHOT(handler, ecs) ((void)0)
#endif

} // namespace lore::input::debug