#include <lore/input/input_debug.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <thread>

namespace lore::input::debug {

// InputDebugMonitor implementation
InputDebugMonitor::InputDebugMonitor(std::string name)
    : name_(std::move(name))
{
    statistics_.monitoring_start_time = std::chrono::high_resolution_clock::now();
}

InputDebugMonitor::~InputDebugMonitor() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void InputDebugMonitor::set_log_file_path(const std::string& path) {
    log_file_path_ = path;
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void InputDebugMonitor::record_event(std::unique_ptr<IEvent> event, const std::string& source) {
    if (!recording_enabled_) return;

    std::lock_guard<std::mutex> lock(event_records_mutex_);

    EventRecord record;
    record.event = std::move(event);
    record.timestamp = std::chrono::high_resolution_clock::now();
    record.frame_number = record.event->get_frame_number();
    record.event_source = source;

    event_records_.emplace_back(std::move(record));

    // Cleanup old records if necessary
    if (event_records_.size() > max_event_records_) {
        event_records_.erase(event_records_.begin());
    }

    statistics_.total_events_recorded++;

    if (real_time_monitoring_) {
        log_event(*record.event, "RECORDED from " + source);
    }
}

void InputDebugMonitor::clear_event_records() {
    std::lock_guard<std::mutex> lock(event_records_mutex_);
    event_records_.clear();
}

std::vector<EventRecord> InputDebugMonitor::get_filtered_event_records(const DebugEventFilter& filter) const {
    std::lock_guard<std::mutex> lock(event_records_mutex_);

    std::vector<EventRecord> filtered;
    filtered.reserve(event_records_.size());

    for (const auto& record : event_records_) {
        if (filter.should_include_event(*record.event)) {
            // Create a copy - this is expensive but necessary for thread safety
            EventRecord copy;
            copy.timestamp = record.timestamp;
            copy.frame_number = record.frame_number;
            copy.event_source = record.event_source;
            // Note: We can't easily copy the event, so we'll store a reference
            // In a real implementation, you'd want to make events copyable or use shared_ptr
            filtered.emplace_back(std::move(copy));
        }
    }

    return filtered;
}

void InputDebugMonitor::take_input_state_snapshot(const GLFWInputHandler& input_handler,
                                                 const InputECSSystem* ecs_system) {
    InputStateSnapshot snapshot;
    snapshot.timestamp = std::chrono::high_resolution_clock::now();
    snapshot.frame_number = 0; // Would need to get this from the system

    // Capture mouse state
    snapshot.mouse_position = input_handler.get_mouse_position();

    // Capture keyboard state
    for (int i = 32; i <= 348; ++i) {
        KeyCode key = static_cast<KeyCode>(i);
        if (input_handler.is_key_pressed(key)) {
            snapshot.keyboard_state[key] = true;
        }
    }

    // Capture mouse button state
    for (int i = 0; i < 8; ++i) {
        MouseButton button = static_cast<MouseButton>(i);
        if (input_handler.is_mouse_button_pressed(button)) {
            snapshot.mouse_button_state[button] = true;
        }
    }

    // Capture gamepad state
    for (std::uint32_t i = 0; i < 16; ++i) {
        if (input_handler.is_gamepad_connected(i)) {
            snapshot.gamepads[i].connected = true;
            // Would need access to gamepad state details
        }
    }

    // Capture ECS state if available
    if (ecs_system) {
        snapshot.focused_entity = ecs_system->get_focused_entity();
    }

    state_snapshots_.emplace_back(std::move(snapshot));

    // Cleanup old snapshots
    if (state_snapshots_.size() > max_state_snapshots_) {
        state_snapshots_.erase(state_snapshots_.begin());
    }

    statistics_.snapshots_taken++;

    if (real_time_monitoring_) {
        log(DebugLevel::Debug, "Input state snapshot taken");
    }
}

void InputDebugMonitor::update_performance_metrics(const InputPerformanceMetrics& metrics) {
    current_metrics_ = metrics;
    metrics_history_.push_back(metrics);

    // Cleanup old metrics
    if (metrics_history_.size() > max_metrics_history_) {
        metrics_history_.erase(metrics_history_.begin());
    }

    if (real_time_monitoring_) {
        static int counter = 0;
        if (++counter % 60 == 0) { // Log every 60 updates
            log_performance_summary();
        }
    }
}

void InputDebugMonitor::log(DebugLevel level, const std::string& message) const {
    if (level < debug_level_) return;

    std::string formatted_message = "[" + format_timestamp() + "] " +
                                  "[" + debug_level_to_string(level) + "] " +
                                  "[" + name_ + "] " + message;

    write_to_output(level, formatted_message);
    statistics_.log_messages_written++;
}

void InputDebugMonitor::log_event(const IEvent& event, const std::string& context) const {
    if (DebugLevel::Debug < debug_level_) return;

    std::ostringstream oss;
    oss << "Event: " << event.get_name()
        << " (ID: " << event.get_id()
        << ", Frame: " << event.get_frame_number()
        << ", Priority: " << static_cast<int>(event.get_priority()) << ")";

    if (!context.empty()) {
        oss << " - " << context;
    }

    log(DebugLevel::Debug, oss.str());
}

void InputDebugMonitor::log_performance_summary() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Performance: "
        << current_metrics_.events_processed_per_second << " events/sec, "
        << current_metrics_.average_event_processing_time_ms << "ms avg, "
        << current_metrics_.current_queue_size << " queued, "
        << current_metrics_.active_listeners << " listeners, "
        << (current_metrics_.estimated_memory_usage_bytes / 1024) << " KB mem";

    log(DebugLevel::Info, oss.str());
}

void InputDebugMonitor::log_listener_summary(const InputListenerManager& listener_manager) const {
    auto stats = listener_manager.get_statistics();

    std::ostringstream oss;
    oss << "Listeners: " << stats.active_listeners << " active/"
        << stats.total_listeners << " total, "
        << stats.active_groups << " groups, "
        << stats.total_invocations << " invocations";

    log(DebugLevel::Info, oss.str());
}

void InputDebugMonitor::generate_debug_report(const std::string& file_path) const {
    std::ofstream report(file_path);
    if (!report.is_open()) {
        log(DebugLevel::Error, "Failed to create debug report file: " + file_path);
        return;
    }

    report << "=== Lore Input System Debug Report ===" << std::endl;
    report << "Generated: " << format_timestamp() << std::endl;
    report << "Monitor: " << name_ << std::endl;
    report << std::endl;

    // Statistics summary
    report << "=== Statistics ===" << std::endl;
    report << "Events recorded: " << statistics_.total_events_recorded << std::endl;
    report << "Snapshots taken: " << statistics_.snapshots_taken << std::endl;
    report << "Log messages: " << statistics_.log_messages_written << std::endl;
    report << "Monitoring time: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - statistics_.monitoring_start_time).count() << " seconds" << std::endl;
    report << std::endl;

    // Performance summary
    report << "=== Performance Summary ===" << std::endl;
    report << generate_performance_summary() << std::endl;

    // Event summary
    report << "=== Event Summary ===" << std::endl;
    report << generate_event_summary() << std::endl;

    // State summary
    report << "=== State Summary ===" << std::endl;
    report << generate_state_summary() << std::endl;

    report.close();
    log(DebugLevel::Info, "Debug report generated: " + file_path);
}

std::string InputDebugMonitor::generate_event_summary() const {
    std::lock_guard<std::mutex> lock(event_records_mutex_);

    std::ostringstream oss;
    oss << "Total events recorded: " << event_records_.size() << std::endl;

    if (!event_records_.empty()) {
        // Count events by type
        std::unordered_map<std::string, std::size_t> event_type_counts;
        for (const auto& record : event_records_) {
            event_type_counts[record.event->get_name()]++;
        }

        oss << "Events by type:" << std::endl;
        for (const auto& [type, count] : event_type_counts) {
            oss << "  " << type << ": " << count << std::endl;
        }

        // Recent events
        oss << "Most recent events:" << std::endl;
        std::size_t recent_count = std::min(event_records_.size(), std::size_t(10));
        for (std::size_t i = event_records_.size() - recent_count; i < event_records_.size(); ++i) {
            const auto& record = event_records_[i];
            oss << "  Frame " << record.frame_number << ": " << record.event->get_name()
                << " (" << record.event_source << ")" << std::endl;
        }
    }

    return oss.str();
}

std::string InputDebugMonitor::generate_performance_summary() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "Current metrics:" << std::endl;
    oss << "  Events/sec: " << current_metrics_.events_processed_per_second << std::endl;
    oss << "  Avg processing time: " << current_metrics_.average_event_processing_time_ms << " ms" << std::endl;
    oss << "  Max processing time: " << current_metrics_.max_event_processing_time_ms << " ms" << std::endl;
    oss << "  Queue size: " << current_metrics_.current_queue_size << " / " << current_metrics_.max_queue_size << " max" << std::endl;
    oss << "  Active listeners: " << current_metrics_.active_listeners << std::endl;
    oss << "  Memory usage: " << (current_metrics_.estimated_memory_usage_bytes / 1024) << " KB" << std::endl;

    if (!metrics_history_.empty()) {
        oss << "Historical metrics:" << std::endl;

        float avg_events_per_sec = 0.0f;
        float avg_processing_time = 0.0f;
        for (const auto& metrics : metrics_history_) {
            avg_events_per_sec += metrics.events_processed_per_second;
            avg_processing_time += metrics.average_event_processing_time_ms;
        }
        avg_events_per_sec /= metrics_history_.size();
        avg_processing_time /= metrics_history_.size();

        oss << "  Avg events/sec: " << avg_events_per_sec << std::endl;
        oss << "  Avg processing time: " << avg_processing_time << " ms" << std::endl;
    }

    return oss.str();
}

std::string InputDebugMonitor::generate_state_summary() const {
    std::ostringstream oss;
    oss << "Total snapshots: " << state_snapshots_.size() << std::endl;

    if (!state_snapshots_.empty()) {
        const auto& latest = state_snapshots_.back();
        oss << "Latest snapshot (Frame " << latest.frame_number << "):" << std::endl;
        oss << "  Mouse position: (" << latest.mouse_position.x << ", " << latest.mouse_position.y << ")" << std::endl;
        oss << "  Keys pressed: " << latest.keyboard_state.size() << std::endl;
        oss << "  Mouse buttons pressed: " << latest.mouse_button_state.size() << std::endl;

        std::size_t connected_gamepads = 0;
        for (const auto& gamepad : latest.gamepads) {
            if (gamepad.connected) connected_gamepads++;
        }
        oss << "  Connected gamepads: " << connected_gamepads << std::endl;
        oss << "  Focused entity: " << static_cast<std::uint32_t>(latest.focused_entity) << std::endl;
    }

    return oss.str();
}

void InputDebugMonitor::save_event_sequence(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(event_records_mutex_);

    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        log(DebugLevel::Error, "Failed to save event sequence: " + file_path);
        return;
    }

    // Write header
    std::uint32_t version = 1;
    std::uint32_t event_count = static_cast<std::uint32_t>(event_records_.size());
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&event_count), sizeof(event_count));

    // Write events (simplified - in practice you'd need proper serialization)
    for (const auto& record : event_records_) {
        std::uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            record.timestamp.time_since_epoch()).count();
        file.write(reinterpret_cast<const char*>(&timestamp_ns), sizeof(timestamp_ns));
        file.write(reinterpret_cast<const char*>(&record.frame_number), sizeof(record.frame_number));

        std::uint32_t source_len = static_cast<std::uint32_t>(record.event_source.length());
        file.write(reinterpret_cast<const char*>(&source_len), sizeof(source_len));
        file.write(record.event_source.c_str(), source_len);

        // Event serialization would go here
        // For now, just write the event name
        std::string event_name = record.event->get_name();
        std::uint32_t name_len = static_cast<std::uint32_t>(event_name.length());
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(event_name.c_str(), name_len);
    }

    file.close();
    log(DebugLevel::Info, "Event sequence saved: " + file_path);
}

bool InputDebugMonitor::load_event_sequence(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        log(DebugLevel::Error, "Failed to load event sequence: " + file_path);
        return false;
    }

    // Read header
    std::uint32_t version, event_count;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&event_count), sizeof(event_count));

    if (version != 1) {
        log(DebugLevel::Error, "Unsupported event sequence version: " + std::to_string(version));
        return false;
    }

    clear_event_records();

    // Read events
    for (std::uint32_t i = 0; i < event_count; ++i) {
        std::uint64_t timestamp_ns;
        std::uint64_t frame_number;
        file.read(reinterpret_cast<char*>(&timestamp_ns), sizeof(timestamp_ns));
        file.read(reinterpret_cast<char*>(&frame_number), sizeof(frame_number));

        std::uint32_t source_len;
        file.read(reinterpret_cast<char*>(&source_len), sizeof(source_len));
        std::string source(source_len, '\0');
        file.read(source.data(), source_len);

        std::uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        std::string event_name(name_len, '\0');
        file.read(event_name.data(), name_len);

        // Event deserialization would go here
        // For now, we can't recreate the events without proper serialization
        log(DebugLevel::Debug, "Loaded event: " + event_name + " from " + source);
    }

    file.close();
    log(DebugLevel::Info, "Event sequence loaded: " + file_path);
    return true;
}

void InputDebugMonitor::replay_events([[maybe_unused]] EventDispatcher& dispatcher, [[maybe_unused]] float time_scale) const {
    std::lock_guard<std::mutex> lock(event_records_mutex_);

    if (event_records_.empty()) {
        log(DebugLevel::Warning, "No events to replay");
        return;
    }

    log(DebugLevel::Info, "Starting event replay with " + std::to_string(event_records_.size()) + " events");

    auto start_time = std::chrono::high_resolution_clock::now();
    auto first_event_time = event_records_[0].timestamp;

    for (const auto& record : event_records_) {
        // Calculate when this event should be replayed
        auto event_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.timestamp - first_event_time);
        auto scaled_delay = std::chrono::milliseconds(
            static_cast<long long>(event_delay.count() / time_scale));

        // Wait until it's time to replay this event
        auto target_time = start_time + scaled_delay;
        std::this_thread::sleep_until(target_time);

        // Replay the event (would need to recreate the event properly)
        log(DebugLevel::Debug, std::string("Replaying event: ") + record.event->get_name());
    }

    log(DebugLevel::Info, "Event replay completed");
}

void InputDebugMonitor::reset_statistics() {
    statistics_ = DebugStatistics{};
    statistics_.monitoring_start_time = std::chrono::high_resolution_clock::now();
}

void InputDebugMonitor::write_to_output([[maybe_unused]] DebugLevel level, const std::string& message) const {
    switch (output_mode_) {
        case DebugOutputMode::Console:
            std::cout << message << std::endl;
            break;
        case DebugOutputMode::File:
            ensure_log_file_open();
            if (log_file_.is_open()) {
                log_file_ << message << std::endl;
                log_file_.flush();
            }
            break;
        case DebugOutputMode::Both:
            std::cout << message << std::endl;
            ensure_log_file_open();
            if (log_file_.is_open()) {
                log_file_ << message << std::endl;
                log_file_.flush();
            }
            break;
        case DebugOutputMode::None:
            break;
    }
}

void InputDebugMonitor::ensure_log_file_open() const {
    if (!log_file_.is_open() && !log_file_path_.empty()) {
        log_file_.open(log_file_path_, std::ios::app);
    }
}

std::string InputDebugMonitor::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;

#ifdef _WIN32
    // Use thread-safe localtime_s on Windows
    std::tm tm_buf;
    if (localtime_s(&tm_buf, &time_t) == 0) {
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    } else {
        oss << "INVALID_TIME";
    }
#else
    // Use thread-safe localtime_r on POSIX systems
    std::tm tm_buf;
    if (localtime_r(&time_t, &tm_buf) != nullptr) {
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    } else {
        oss << "INVALID_TIME";
    }
#endif

    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string InputDebugMonitor::debug_level_to_string(DebugLevel level) const {
    switch (level) {
        case DebugLevel::Trace: return "TRACE";
        case DebugLevel::Debug: return "DEBUG";
        case DebugLevel::Info: return "INFO";
        case DebugLevel::Warning: return "WARN";
        case DebugLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

void InputDebugMonitor::cleanup_old_records() {
    // This is called automatically in record_event
}

void InputDebugMonitor::cleanup_old_snapshots() {
    // This is called automatically in take_input_state_snapshot
}

void InputDebugMonitor::cleanup_old_metrics() {
    // This is called automatically in update_performance_metrics
}

// InputDebugConsole implementation
InputDebugConsole::InputDebugConsole() {
    register_builtin_commands();
}

void InputDebugConsole::register_command(const std::string& name, CommandHandler handler, const std::string& help) {
    commands_[name] = {std::move(handler), help};
}

void InputDebugConsole::unregister_command(const std::string& name) {
    commands_.erase(name);
}

std::string InputDebugConsole::execute_command(const std::string& command_line) {
    if (command_line.empty()) {
        return "";
    }

    command_history_.push_back(command_line);

    auto args = split_command_line(command_line);
    if (args.empty()) {
        return "Error: Empty command";
    }

    std::string command = args[0];
    args.erase(args.begin());

    auto it = commands_.find(command);
    if (it == commands_.end()) {
        return "Error: Unknown command '" + command + "'. Type 'help' for available commands.";
    }

    try {
        return it->second.handler(args);
    } catch (const std::exception& e) {
        return "Error executing command: " + std::string(e.what());
    }
}

std::vector<std::string> InputDebugConsole::get_available_commands() const {
    std::vector<std::string> commands;
    commands.reserve(commands_.size());

    for (const auto& [name, info] : commands_) {
        commands.push_back(name);
    }

    std::sort(commands.begin(), commands.end());
    return commands;
}

std::string InputDebugConsole::get_command_help(const std::string& command) const {
    auto it = commands_.find(command);
    if (it != commands_.end()) {
        return it->second.help;
    }
    return "Unknown command";
}

void InputDebugConsole::register_builtin_commands() {
    register_command("help", [this](const auto& args) { return cmd_help(args); },
                    "Show available commands or help for a specific command");
    register_command("status", [this](const auto& args) { return cmd_status(args); },
                    "Show input system status");
    register_command("events", [this](const auto& args) { return cmd_events(args); },
                    "Show recent events or event statistics");
    register_command("listeners", [this](const auto& args) { return cmd_listeners(args); },
                    "Show active listeners");
    register_command("performance", [this](const auto& args) { return cmd_performance(args); },
                    "Show performance metrics");
    register_command("snapshot", [this](const auto& args) { return cmd_snapshot(args); },
                    "Take or show input state snapshots");
    register_command("record", [this](const auto& args) { return cmd_record(args); },
                    "Control event recording");
    register_command("log_level", [this](const auto& args) { return cmd_log_level(args); },
                    "Get or set debug log level");
    register_command("clear", [this](const auto& args) { return cmd_clear(args); },
                    "Clear various data (events, snapshots, history)");
}

std::string InputDebugConsole::cmd_help(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::ostringstream oss;
        oss << "Available commands:" << std::endl;
        for (const auto& [name, info] : commands_) {
            oss << "  " << name << " - " << info.help << std::endl;
        }
        return oss.str();
    } else {
        return get_command_help(args[0]);
    }
}

std::string InputDebugConsole::cmd_status([[maybe_unused]] const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << "=== Input System Status ===" << std::endl;

    if (monitor_) {
        oss << "Debug Monitor: " << (monitor_ ? "Active" : "Inactive") << std::endl;
        oss << "Recording: " << (monitor_->is_recording() ? "Enabled" : "Disabled") << std::endl;
        oss << "Real-time monitoring: " << (monitor_->is_real_time_monitoring_enabled() ? "Enabled" : "Disabled") << std::endl;

        auto stats = monitor_->get_statistics();
        oss << "Events recorded: " << stats.total_events_recorded << std::endl;
        oss << "Snapshots taken: " << stats.snapshots_taken << std::endl;
    }

    if (listener_manager_) {
        auto stats = listener_manager_->get_statistics();
        oss << "Active listeners: " << stats.active_listeners << std::endl;
        oss << "Active groups: " << stats.active_groups << std::endl;
        oss << "Total invocations: " << stats.total_invocations << std::endl;
    }

    if (input_handler_) {
        oss << "Events generated this frame: " << input_handler_->get_events_generated_this_frame() << std::endl;
        oss << "Total events generated: " << input_handler_->get_total_events_generated() << std::endl;
    }

    return oss.str();
}

std::string InputDebugConsole::cmd_events([[maybe_unused]] const std::vector<std::string>& args) {
    if (!monitor_) {
        return "Error: No debug monitor attached";
    }

    return monitor_->generate_event_summary();
}

std::string InputDebugConsole::cmd_listeners([[maybe_unused]] const std::vector<std::string>& args) {
    if (!listener_manager_) {
        return "Error: No listener manager attached";
    }

    std::ostringstream oss;
    auto stats = listener_manager_->get_statistics();
    oss << "Listener Statistics:" << std::endl;
    oss << "  Active: " << stats.active_listeners << std::endl;
    oss << "  Total: " << stats.total_listeners << std::endl;
    oss << "  Groups: " << stats.active_groups << std::endl;
    oss << "  Invocations: " << stats.total_invocations << std::endl;

    auto names = listener_manager_->get_listener_names();
    if (!names.empty()) {
        oss << "Active listeners:" << std::endl;
        for (const auto& name : names) {
            oss << "  " << name << std::endl;
        }
    }

    return oss.str();
}

std::string InputDebugConsole::cmd_performance([[maybe_unused]] const std::vector<std::string>& args) {
    if (!monitor_) {
        return "Error: No debug monitor attached";
    }

    return monitor_->generate_performance_summary();
}

std::string InputDebugConsole::cmd_snapshot(const std::vector<std::string>& args) {
    if (!monitor_) {
        return "Error: No debug monitor attached";
    }

    if (args.empty() || args[0] == "take") {
        if (input_handler_) {
            monitor_->take_input_state_snapshot(*input_handler_, ecs_system_);
            return "Input state snapshot taken";
        } else {
            return "Error: No input handler attached";
        }
    } else if (args[0] == "show") {
        return monitor_->generate_state_summary();
    } else if (args[0] == "clear") {
        monitor_->clear_state_snapshots();
        return "State snapshots cleared";
    } else {
        return "Usage: snapshot [take|show|clear]";
    }
}

std::string InputDebugConsole::cmd_record(const std::vector<std::string>& args) {
    if (!monitor_) {
        return "Error: No debug monitor attached";
    }

    if (args.empty()) {
        return "Recording status: " + std::string(monitor_->is_recording() ? "Enabled" : "Disabled");
    } else if (args[0] == "start") {
        monitor_->start_recording();
        return "Event recording started";
    } else if (args[0] == "stop") {
        monitor_->stop_recording();
        return "Event recording stopped";
    } else if (args[0] == "clear") {
        monitor_->clear_event_records();
        return "Event records cleared";
    } else {
        return "Usage: record [start|stop|clear]";
    }
}

std::string InputDebugConsole::cmd_log_level(const std::vector<std::string>& args) {
    if (!monitor_) {
        return "Error: No debug monitor attached";
    }

    if (args.empty()) {
        DebugLevel level = monitor_->get_debug_level();
        std::string level_str;
        switch (level) {
            case DebugLevel::Trace: level_str = "Trace"; break;
            case DebugLevel::Debug: level_str = "Debug"; break;
            case DebugLevel::Info: level_str = "Info"; break;
            case DebugLevel::Warning: level_str = "Warning"; break;
            case DebugLevel::Error: level_str = "Error"; break;
        }
        return "Current log level: " + level_str;
    } else {
        std::string level_str = args[0];
        std::transform(level_str.begin(), level_str.end(), level_str.begin(),
                      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        DebugLevel level;
        if (level_str == "trace") level = DebugLevel::Trace;
        else if (level_str == "debug") level = DebugLevel::Debug;
        else if (level_str == "info") level = DebugLevel::Info;
        else if (level_str == "warning" || level_str == "warn") level = DebugLevel::Warning;
        else if (level_str == "error") level = DebugLevel::Error;
        else return "Error: Invalid log level. Use: trace, debug, info, warning, error";

        monitor_->set_debug_level(level);
        return "Log level set to: " + args[0];
    }
}

std::string InputDebugConsole::cmd_clear(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "Usage: clear [events|snapshots|history|all]";
    }

    std::string what = args[0];
    if (what == "events" && monitor_) {
        monitor_->clear_event_records();
        return "Event records cleared";
    } else if (what == "snapshots" && monitor_) {
        monitor_->clear_state_snapshots();
        return "State snapshots cleared";
    } else if (what == "history") {
        command_history_.clear();
        return "Command history cleared";
    } else if (what == "all") {
        if (monitor_) {
            monitor_->clear_event_records();
            monitor_->clear_state_snapshots();
        }
        command_history_.clear();
        return "All data cleared";
    } else {
        return "Error: Unknown clear target or no monitor attached";
    }
}

std::vector<std::string> InputDebugConsole::split_command_line(const std::string& command_line) const {
    std::vector<std::string> args;
    std::istringstream iss(command_line);
    std::string arg;

    while (iss >> arg) {
        args.push_back(arg);
    }

    return args;
}

std::string InputDebugConsole::join_args(const std::vector<std::string>& args, std::size_t start_index) const {
    if (start_index >= args.size()) {
        return "";
    }

    std::ostringstream oss;
    for (std::size_t i = start_index; i < args.size(); ++i) {
        if (i > start_index) oss << " ";
        oss << args[i];
    }
    return oss.str();
}

// InputDebugConfig implementation
bool InputDebugConfig::load_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    load_from_string(content);
    config_file_path = file_path;
    return true;
}

bool InputDebugConfig::save_to_file(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    file << save_to_string();
    return true;
}

void InputDebugConfig::load_from_string(const std::string& config_data) {
    // Simple key=value parser
    std::istringstream iss(config_data);
    std::string line;

    while (std::getline(iss, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Parse values
        if (key == "debug_level") {
            parse_debug_level(value);
        } else if (key == "output_mode") {
            parse_output_mode(value);
        } else if (key == "log_file_path") {
            log_file_path = value;
        } else if (key == "recording_enabled") {
            recording_enabled = (value == "true" || value == "1");
        } else if (key == "max_event_records") {
            max_event_records = std::stoull(value);
        } else if (key == "max_state_snapshots") {
            max_state_snapshots = std::stoull(value);
        } else if (key == "real_time_monitoring") {
            real_time_monitoring = (value == "true" || value == "1");
        }
    }
}

std::string InputDebugConfig::save_to_string() const {
    std::ostringstream oss;
    oss << "# Lore Input Debug Configuration" << std::endl;
    oss << "debug_level=" << debug_level_to_string() << std::endl;
    oss << "output_mode=" << output_mode_to_string() << std::endl;
    oss << "log_file_path=" << log_file_path << std::endl;
    oss << "recording_enabled=" << (recording_enabled ? "true" : "false") << std::endl;
    oss << "max_event_records=" << max_event_records << std::endl;
    oss << "max_state_snapshots=" << max_state_snapshots << std::endl;
    oss << "real_time_monitoring=" << (real_time_monitoring ? "true" : "false") << std::endl;
    return oss.str();
}

bool InputDebugConfig::validate() const {
    return max_event_records > 0 && max_state_snapshots > 0;
}

std::vector<std::string> InputDebugConfig::get_validation_errors() const {
    std::vector<std::string> errors;
    if (max_event_records == 0) {
        errors.push_back("max_event_records must be greater than 0");
    }
    if (max_state_snapshots == 0) {
        errors.push_back("max_state_snapshots must be greater than 0");
    }
    return errors;
}

void InputDebugConfig::parse_debug_level(const std::string& value) {
    if (value == "trace") debug_level = DebugLevel::Trace;
    else if (value == "debug") debug_level = DebugLevel::Debug;
    else if (value == "info") debug_level = DebugLevel::Info;
    else if (value == "warning" || value == "warn") debug_level = DebugLevel::Warning;
    else if (value == "error") debug_level = DebugLevel::Error;
}

void InputDebugConfig::parse_output_mode(const std::string& value) {
    if (value == "console") output_mode = DebugOutputMode::Console;
    else if (value == "file") output_mode = DebugOutputMode::File;
    else if (value == "both") output_mode = DebugOutputMode::Both;
    else if (value == "none") output_mode = DebugOutputMode::None;
}

std::string InputDebugConfig::debug_level_to_string() const {
    switch (debug_level) {
        case DebugLevel::Trace: return "trace";
        case DebugLevel::Debug: return "debug";
        case DebugLevel::Info: return "info";
        case DebugLevel::Warning: return "warning";
        case DebugLevel::Error: return "error";
        default: return "info";
    }
}

std::string InputDebugConfig::output_mode_to_string() const {
    switch (output_mode) {
        case DebugOutputMode::Console: return "console";
        case DebugOutputMode::File: return "file";
        case DebugOutputMode::Both: return "both";
        case DebugOutputMode::None: return "none";
        default: return "console";
    }
}

// Global debug instance management
namespace global {
    std::unique_ptr<InputDebugMonitor> g_debug_monitor;
    std::unique_ptr<InputDebugConsole> g_debug_console;
    InputDebugConfig g_debug_config;

    void initialize_input_debugging(const std::string& config_file) {
        g_debug_monitor = std::make_unique<InputDebugMonitor>("GlobalInputDebug");
        g_debug_console = std::make_unique<InputDebugConsole>();

        if (!config_file.empty()) {
            g_debug_config.load_from_file(config_file);
        }

        // Apply configuration
        g_debug_monitor->set_debug_level(g_debug_config.debug_level);
        g_debug_monitor->set_output_mode(g_debug_config.output_mode);
        g_debug_monitor->set_log_file_path(g_debug_config.log_file_path);
        g_debug_monitor->set_max_event_records(g_debug_config.max_event_records);
        g_debug_monitor->enable_real_time_monitoring(g_debug_config.real_time_monitoring);

        if (g_debug_config.recording_enabled) {
            g_debug_monitor->start_recording();
        }

        g_debug_console->attach_monitor(g_debug_monitor.get());

        g_debug_monitor->log(DebugLevel::Info, "Input debugging system initialized");
    }

    void shutdown_input_debugging() {
        if (g_debug_monitor) {
            g_debug_monitor->log(DebugLevel::Info, "Input debugging system shutting down");
        }

        g_debug_console.reset();
        g_debug_monitor.reset();
    }

    InputDebugMonitor& get_debug_monitor() {
        if (!g_debug_monitor) {
            initialize_input_debugging();
        }
        return *g_debug_monitor;
    }

    InputDebugConsole& get_debug_console() {
        if (!g_debug_console) {
            initialize_input_debugging();
        }
        return *g_debug_console;
    }

    InputDebugConfig& get_debug_config() {
        return g_debug_config;
    }

    void debug_log(DebugLevel level, const std::string& message) {
        get_debug_monitor().log(level, message);
    }

    void debug_log_event(const IEvent& event, const std::string& context) {
        get_debug_monitor().log_event(event, context);
    }

    void start_recording() {
        get_debug_monitor().start_recording();
    }

    void stop_recording() {
        get_debug_monitor().stop_recording();
    }

    void take_snapshot(const GLFWInputHandler& input_handler, const InputECSSystem* ecs_system) {
        get_debug_monitor().take_input_state_snapshot(input_handler, ecs_system);
    }

    void generate_report(const std::string& file_path) {
        get_debug_monitor().generate_debug_report(file_path);
    }
}

} // namespace lore::input::debug