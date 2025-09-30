#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include <memory>
#include <source_location>
#include <format>

namespace lore::core {

// Log severity levels
enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

// Log categories for filtering
enum class LogCategory : uint32_t {
    General = 0,
    Graphics = 1,
    Vulkan = 2,
    Physics = 3,
    Audio = 4,
    Input = 5,
    ECS = 6,
    Assets = 7,
    Network = 8,
    Game = 9,
    Performance = 10
};

// Compile-time log level configuration
#ifdef NDEBUG
    constexpr LogLevel COMPILE_TIME_LOG_LEVEL = LogLevel::Info;
    constexpr bool LOG_TO_CONSOLE = false;
#else
    constexpr LogLevel COMPILE_TIME_LOG_LEVEL = LogLevel::Trace;
    constexpr bool LOG_TO_CONSOLE = true;
#endif

// Forward declaration
class Logger;

// RAII scoped timer for performance logging
class ScopedTimer {
public:
    ScopedTimer(std::string_view name, LogCategory category = LogCategory::Performance);
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string name_;
    LogCategory category_;
    std::chrono::high_resolution_clock::time_point start_;
};

// Thread-safe, high-performance logger with Unicode support
class Logger {
public:
    // Singleton access
    static Logger& instance();

    // Initialize logger with file path
    void initialize(const std::string& log_file_path = "lore_engine.log",
                   bool append = false,
                   LogLevel min_level = COMPILE_TIME_LOG_LEVEL);

    // Shutdown and flush all pending logs
    void shutdown();

    // Set minimum log level at runtime
    void set_min_level(LogLevel level) noexcept;
    LogLevel get_min_level() const noexcept { return min_level_.load(std::memory_order_relaxed); }

    // Enable/disable specific categories
    void enable_category(LogCategory category, bool enabled = true) noexcept;
    bool is_category_enabled(LogCategory category) const noexcept;

    // Core logging function with compile-time optimization
    template<LogLevel Level, LogCategory Category = LogCategory::General>
    void log(std::string_view message,
             const std::source_location& location = std::source_location::current()) {
        // Compile-time check - completely eliminated in release builds if below threshold
        if constexpr (Level < COMPILE_TIME_LOG_LEVEL) {
            return;
        }

        // Runtime checks only if compile-time check passes
        if (Level < min_level_.load(std::memory_order_relaxed)) {
            return;
        }

        if (!is_category_enabled(Category)) {
            return;
        }

        log_impl(Level, Category, message, location);
    }

    // Formatted logging with std::format support
    template<LogLevel Level, LogCategory Category = LogCategory::General, typename... Args>
    void log_fmt(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) {
        if constexpr (Level < COMPILE_TIME_LOG_LEVEL) {
            return;
        }

        if (Level < min_level_.load(std::memory_order_relaxed)) {
            return;
        }

        if (!is_category_enabled(Category)) {
            return;
        }

        try {
            std::string formatted = std::format(fmt, std::forward<Args>(args)...);
            log_impl(Level, Category, formatted, location);
        } catch (const std::exception& e) {
            log_impl(LogLevel::Error, LogCategory::General,
                    std::format("Log formatting error: {}", e.what()), location);
        }
    }

    // Flush pending logs to disk
    void flush();

    // Get statistics
    struct Stats {
        uint64_t total_logs = 0;
        uint64_t dropped_logs = 0;
        uint64_t file_writes = 0;
        uint64_t console_writes = 0;
    };
    Stats get_stats() const noexcept;

    // Public logging implementation for advanced use cases (e.g., ScopedTimer)
    void log_impl(LogLevel level, LogCategory category, std::string_view message,
                  const std::source_location& location);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write_to_file(std::string_view formatted_message);
    void write_to_console(LogLevel level, std::string_view formatted_message);

    std::string format_log_entry(LogLevel level, LogCategory category,
                                 std::string_view message,
                                 const std::source_location& location) const;

    static const char* level_to_string(LogLevel level) noexcept;
    static const char* category_to_string(LogCategory category) noexcept;
    static const char* level_to_color_code(LogLevel level) noexcept;

    // Configuration
    std::atomic<LogLevel> min_level_{COMPILE_TIME_LOG_LEVEL};
    std::atomic<uint32_t> enabled_categories_{0xFFFFFFFF}; // All enabled by default
    bool startup_phase_{true}; // Allow console output during startup

    // File handling
    std::ofstream log_file_;
    std::string log_file_path_;
    std::mutex file_mutex_;

    // Statistics
    mutable std::atomic<uint64_t> total_logs_{0};
    mutable std::atomic<uint64_t> dropped_logs_{0};
    mutable std::atomic<uint64_t> file_writes_{0};
    mutable std::atomic<uint64_t> console_writes_{0};

    // Initialization state
    std::atomic<bool> initialized_{false};
};

// Convenience macros with compile-time optimization
#define LOG_TRACE(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Trace, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

#define LOG_DEBUG(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Debug, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

#define LOG_INFO(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Info, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

#define LOG_WARNING(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Warning, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

#define LOG_ERROR(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Error, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

#define LOG_CRITICAL(category, ...) \
    ::lore::core::Logger::instance().log_fmt<::lore::core::LogLevel::Critical, ::lore::core::LogCategory::category>(std::source_location::current(), __VA_ARGS__)

// Scoped performance timing
#define LOG_SCOPE_TIMER(name) \
    ::lore::core::ScopedTimer LORE_UNIQUE_NAME(timer_)(name)

#define LOG_SCOPE_TIMER_CAT(name, category) \
    ::lore::core::ScopedTimer LORE_UNIQUE_NAME(timer_)(name, ::lore::core::LogCategory::category)

// Helper macro for unique variable names
#define LORE_CONCAT_IMPL(a, b) a##b
#define LORE_CONCAT(a, b) LORE_CONCAT_IMPL(a, b)
#define LORE_UNIQUE_NAME(prefix) LORE_CONCAT(prefix, __LINE__)

} // namespace lore::core