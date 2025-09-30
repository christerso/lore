#include <lore/core/log.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
#endif

namespace lore::core {

// ANSI color codes for console output
namespace {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* GRAY = "\033[90m";
    constexpr const char* CYAN = "\033[96m";
    constexpr const char* GREEN = "\033[92m";
    constexpr const char* YELLOW = "\033[93m";
    constexpr const char* RED = "\033[91m";
    constexpr const char* MAGENTA = "\033[95m";
    constexpr const char* BOLD_RED = "\033[1;91m";

    // Enable ANSI colors on Windows console
    void enable_ansi_colors() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }

        // Enable UTF-8 output
        SetConsoleOutputCP(CP_UTF8);

        // Note: Do NOT use _O_U8TEXT with std::cout - it requires wide character output
        // SetConsoleOutputCP(CP_UTF8) is sufficient for UTF-8 support with narrow streams
#endif
    }

    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    std::string get_thread_id() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }
}

// ScopedTimer Implementation
ScopedTimer::ScopedTimer(std::string_view name, LogCategory category)
    : name_(name)
    , category_(category)
    , start_(std::chrono::high_resolution_clock::now()) {
}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);

    // Log the timing result directly using log_impl
    std::string message = std::format("{} took {:.3f}ms", name_, duration.count() / 1000.0);
    Logger::instance().log_impl(LogLevel::Debug, category_, message, std::source_location::current());
}

// Logger Implementation
Logger::Logger() {
    // Enable ANSI colors and UTF-8 support on Windows
    enable_ansi_colors();
}

Logger::~Logger() {
    shutdown();
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& log_file_path, bool append, LogLevel min_level) {
    if (initialized_.exchange(true, std::memory_order_acquire)) {
        return; // Already initialized
    }

    log_file_path_ = log_file_path;
    min_level_.store(min_level, std::memory_order_relaxed);

    // Create log directory if it doesn't exist
    std::filesystem::path log_path(log_file_path);
    if (log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }

    // Open log file with UTF-8 encoding
    std::ios_base::openmode mode = std::ios::out;
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }

    log_file_.open(log_file_path, mode);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path << std::endl;
        return;
    }

    // Write UTF-8 BOM for proper encoding detection
    if (!append) {
        log_file_ << "\xEF\xBB\xBF";
    }

    // Log initialization message
    log_impl(LogLevel::Info, LogCategory::General,
            std::format("Lore Engine Logger initialized - Log file: {}", log_file_path),
            std::source_location::current());

    log_impl(LogLevel::Info, LogCategory::General,
            std::format("Compile-time log level: {}", level_to_string(COMPILE_TIME_LOG_LEVEL)),
            std::source_location::current());

    // Mark end of startup phase after a brief moment
    // In practice, you'd call end_startup() explicitly after engine initialization
}

void Logger::shutdown() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }

    startup_phase_ = false;

    // Log shutdown message
    log_impl(LogLevel::Info, LogCategory::General,
            "Logger shutting down",
            std::source_location::current());

    // Print statistics
    auto stats = get_stats();
    log_impl(LogLevel::Info, LogCategory::General,
            std::format("Logger stats - Total: {} | Dropped: {} | File writes: {} | Console writes: {}",
                       stats.total_logs, stats.dropped_logs, stats.file_writes, stats.console_writes),
            std::source_location::current());

    flush();

    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
}

void Logger::set_min_level(LogLevel level) noexcept {
    min_level_.store(level, std::memory_order_relaxed);
}

void Logger::enable_category(LogCategory category, bool enabled) noexcept {
    uint32_t mask = 1u << static_cast<uint32_t>(category);
    uint32_t current = enabled_categories_.load(std::memory_order_relaxed);

    if (enabled) {
        enabled_categories_.store(current | mask, std::memory_order_relaxed);
    } else {
        enabled_categories_.store(current & ~mask, std::memory_order_relaxed);
    }
}

bool Logger::is_category_enabled(LogCategory category) const noexcept {
    uint32_t mask = 1u << static_cast<uint32_t>(category);
    return (enabled_categories_.load(std::memory_order_relaxed) & mask) != 0;
}

void Logger::log_impl(LogLevel level, LogCategory category, std::string_view message,
                     const std::source_location& location) {
    total_logs_.fetch_add(1, std::memory_order_relaxed);

    // Format the log entry
    std::string formatted = format_log_entry(level, category, message, location);

    // Always write to file if initialized
    if (initialized_.load(std::memory_order_acquire)) {
        write_to_file(formatted);
    }

    // Write to console during startup phase or if enabled in debug builds
    if (startup_phase_ || (LOG_TO_CONSOLE && !startup_phase_)) {
        write_to_console(level, formatted);
    }
}

void Logger::write_to_file(std::string_view formatted_message) {
    std::lock_guard<std::mutex> lock(file_mutex_);

    if (log_file_.is_open()) {
        log_file_ << formatted_message << std::endl;
        file_writes_.fetch_add(1, std::memory_order_relaxed);
    } else {
        dropped_logs_.fetch_add(1, std::memory_order_relaxed);
    }
}

void Logger::write_to_console(LogLevel level, std::string_view formatted_message) {
    // Use color coding for console output
    const char* color = level_to_color_code(level);

    if (level >= LogLevel::Error) {
        std::cerr << color << formatted_message << RESET << std::endl;
    } else {
        std::cout << color << formatted_message << RESET << std::endl;
    }

    console_writes_.fetch_add(1, std::memory_order_relaxed);
}

std::string Logger::format_log_entry(LogLevel level, LogCategory category,
                                    std::string_view message,
                                    const std::source_location& location) const {
    // Format: [TIMESTAMP] [LEVEL] [CATEGORY] [thread:THREAD_ID] message (file:line)
    std::ostringstream oss;

    oss << "[" << get_timestamp() << "] "
        << "[" << level_to_string(level) << "] "
        << "[" << category_to_string(category) << "] "
        << "[thread:" << get_thread_id() << "] "
        << message;

    // Add source location for debug/trace levels
    if (level <= LogLevel::Debug) {
        std::filesystem::path file_path(location.file_name());
        oss << " (" << file_path.filename().string() << ":" << location.line() << ")";
    }

    return oss.str();
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

Logger::Stats Logger::get_stats() const noexcept {
    Stats stats;
    stats.total_logs = total_logs_.load(std::memory_order_relaxed);
    stats.dropped_logs = dropped_logs_.load(std::memory_order_relaxed);
    stats.file_writes = file_writes_.load(std::memory_order_relaxed);
    stats.console_writes = console_writes_.load(std::memory_order_relaxed);
    return stats;
}

const char* Logger::level_to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRIT";
        default: return "UNKNOWN";
    }
}

const char* Logger::category_to_string(LogCategory category) noexcept {
    switch (category) {
        case LogCategory::General: return "General";
        case LogCategory::Graphics: return "Graphics";
        case LogCategory::Vulkan: return "Vulkan";
        case LogCategory::Physics: return "Physics";
        case LogCategory::Audio: return "Audio";
        case LogCategory::Input: return "Input";
        case LogCategory::ECS: return "ECS";
        case LogCategory::Assets: return "Assets";
        case LogCategory::Network: return "Network";
        case LogCategory::Game: return "Game";
        case LogCategory::Performance: return "Perf";
        default: return "Unknown";
    }
}

const char* Logger::level_to_color_code(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return GRAY;
        case LogLevel::Debug: return CYAN;
        case LogLevel::Info: return GREEN;
        case LogLevel::Warning: return YELLOW;
        case LogLevel::Error: return RED;
        case LogLevel::Critical: return BOLD_RED;
        default: return RESET;
    }
}

} // namespace lore::core