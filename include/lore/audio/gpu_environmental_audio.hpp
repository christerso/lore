#pragma once

// GPU Environmental Audio System Header
// Complete arena-based GPU memory management and comprehensive error handling for environmental acoustics

#include <lore/math/math.hpp>
#include <lore/ecs/ecs.hpp>
#include <lore/graphics/gpu_compute.hpp>

#include <vulkan/vulkan.h>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>
#include <exception>

namespace lore::audio {

// Forward declarations
class AudioSystem;
namespace graphics { class GpuComputeSystem; }

// GPU Environmental Audio Error Handling
class GpuEnvironmentalAudioException : public std::exception {
public:
    enum class ErrorType {
        MEMORY_ALLOCATION_FAILED,
        SHADER_COMPILATION_FAILED,
        PIPELINE_CREATION_FAILED,
        BUFFER_UPLOAD_FAILED,
        COMPUTE_DISPATCH_FAILED,
        ARENA_ALLOCATION_FAILED,
        GPU_TIMEOUT,
        INVALID_CONFIGURATION,
        DRIVER_ERROR
    };

    GpuEnvironmentalAudioException(ErrorType type, const std::string& message)
        : error_type_(type), message_(message) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

    ErrorType get_error_type() const noexcept {
        return error_type_;
    }

private:
    ErrorType error_type_;
    std::string message_;
};

// Arena-based GPU Memory Manager for Environmental Audio
class GpuEnvironmentalArenaManager {
public:
    struct ArenaConfiguration {
        size_t total_arena_size = 128 * 1024 * 1024; // 128MB default
        size_t convolution_arena_size = 32 * 1024 * 1024;
        size_t ray_tracing_arena_size = 48 * 1024 * 1024;
        size_t occlusion_arena_size = 24 * 1024 * 1024;
        size_t reverb_arena_size = 16 * 1024 * 1024;
        size_t output_buffer_size = 8 * 1024 * 1024;
        uint32_t alignment_requirement = 256;
        bool enable_memory_compaction = true;
        float compaction_threshold = 0.7f; // Compact when 70% fragmented
    };

    struct MemoryStats {
        size_t total_allocated;
        size_t total_used;
        size_t total_free;
        size_t largest_free_block;
        float fragmentation_ratio;
        uint32_t active_allocations;
        uint32_t failed_allocations;
        uint32_t compaction_operations;
        std::chrono::microseconds last_compaction_time;
    };

    struct ArenaAllocationInfo {
        graphics::VulkanGpuArenaManager::ArenaAllocation allocation;
        size_t size;
        uint32_t arena_id;
        std::chrono::steady_clock::time_point allocation_time;
        std::string debug_name;
        bool is_persistent; // Persistent allocations survive compaction
    };

    explicit GpuEnvironmentalArenaManager(graphics::VulkanGpuArenaManager& base_manager);
    ~GpuEnvironmentalArenaManager();

    // Arena management with comprehensive error handling
    void initialize_environmental_arenas(const ArenaConfiguration& config);
    void shutdown_environmental_arenas();

    // Smart allocation with automatic retry and compaction
    ArenaAllocationInfo allocate_environmental_buffer(
        const std::string& name,
        size_t size,
        bool is_persistent = false,
        uint32_t preferred_arena = 0
    );

    void deallocate_environmental_buffer(const ArenaAllocationInfo& allocation);

    // Memory management operations
    void compact_arenas_if_needed();
    void force_compact_all_arenas();
    MemoryStats get_memory_stats() const;

    // Error recovery
    void attempt_memory_recovery();
    bool is_low_memory_condition() const;

    // Configuration and monitoring
    void set_configuration(const ArenaConfiguration& config);
    const ArenaConfiguration& get_configuration() const;

    // Debug and profiling
    void enable_memory_debugging(bool enable = true);
    std::vector<ArenaAllocationInfo> get_active_allocations() const;
    void log_memory_usage() const;

private:
    graphics::VulkanGpuArenaManager& base_manager_;
    ArenaConfiguration config_;
    mutable std::mutex arena_mutex_;

    // Arena IDs for different subsystems
    uint32_t convolution_arena_id_;
    uint32_t ray_tracing_arena_id_;
    uint32_t occlusion_arena_id_;
    uint32_t reverb_arena_id_;
    uint32_t output_buffer_arena_id_;

    // Allocation tracking
    std::vector<ArenaAllocationInfo> active_allocations_;
    mutable std::atomic<uint32_t> allocation_counter_;
    std::atomic<bool> low_memory_condition_;

    // Statistics
    mutable MemoryStats cached_stats_;
    mutable std::chrono::steady_clock::time_point last_stats_update_;

    // Debug and monitoring
    bool memory_debugging_enabled_;
    std::function<void(const std::string&)> debug_callback_;

    void update_memory_stats() const;
    uint32_t select_best_arena_for_allocation(size_t size, bool is_persistent) const;
    void perform_arena_compaction(uint32_t arena_id);
    void check_low_memory_condition();
};

// Performance monitoring and error recovery system
class GpuEnvironmentalPerformanceMonitor {
public:
    struct PerformanceMetrics {
        // GPU utilization metrics
        float gpu_utilization_percentage;
        float memory_bandwidth_utilization;
        std::chrono::microseconds frame_time;

        // Subsystem timing
        std::chrono::microseconds convolution_time;
        std::chrono::microseconds ray_tracing_time;
        std::chrono::microseconds occlusion_time;
        std::chrono::microseconds reverb_time;
        std::chrono::microseconds memory_management_time;

        // Throughput metrics
        uint32_t audio_sources_processed_per_second;
        uint32_t rays_traced_per_second;
        uint32_t occlusion_tests_per_second;
        uint32_t convolution_operations_per_second;

        // Quality metrics
        float acoustic_accuracy_score;
        float impulse_response_quality;
        float spatial_resolution;

        // Error tracking
        uint32_t compute_shader_errors;
        uint32_t memory_allocation_failures;
        uint32_t pipeline_stalls;
        uint32_t timeout_events;

        // Resource utilization
        uint64_t gpu_memory_used_bytes;
        uint32_t active_compute_dispatches;
        uint32_t descriptor_set_updates;
        uint32_t buffer_uploads_per_frame;
    };

    struct AlertConfiguration {
        float max_gpu_utilization = 95.0f;
        std::chrono::microseconds max_frame_time{20000}; // 20ms
        uint32_t max_consecutive_errors = 5;
        float min_acoustic_quality = 0.8f;
        uint64_t max_memory_usage = 128 * 1024 * 1024; // 128MB
    };

    using AlertCallback = std::function<void(const std::string& alert_type, const std::string& message, PerformanceMetrics metrics)>;

    explicit GpuEnvironmentalPerformanceMonitor();
    ~GpuEnvironmentalPerformanceMonitor();

    // Monitoring control
    void start_monitoring(const AlertConfiguration& config = {});
    void stop_monitoring();
    void reset_metrics();

    // Metric collection
    void begin_frame_timing();
    void end_frame_timing();
    void record_subsystem_timing(const std::string& subsystem, std::chrono::microseconds time);
    void record_throughput_metric(const std::string& metric_name, uint32_t value);
    void record_error(const std::string& error_type);
    void record_memory_usage(uint64_t bytes_used);

    // Performance analysis
    PerformanceMetrics get_current_metrics() const;
    PerformanceMetrics get_average_metrics(std::chrono::seconds duration) const;
    float calculate_performance_score() const;

    // Alerting system
    void set_alert_callback(AlertCallback callback);
    void set_alert_configuration(const AlertConfiguration& config);

    // Adaptive performance tuning
    void enable_adaptive_tuning(bool enable = true);
    void suggest_performance_optimizations() const;

    // Debug and reporting
    std::string generate_performance_report() const;
    void export_metrics_to_file(const std::string& filename) const;

private:
    mutable std::mutex metrics_mutex_;
    bool monitoring_active_;
    bool adaptive_tuning_enabled_;

    // Current metrics
    PerformanceMetrics current_metrics_;
    std::chrono::steady_clock::time_point frame_start_time_;

    // Historical data for averaging
    struct HistoricalMetrics {
        std::vector<PerformanceMetrics> samples;
        std::chrono::steady_clock::time_point oldest_sample_time;
        size_t max_samples = 300; // 5 minutes at 60 FPS
    };
    HistoricalMetrics historical_data_;

    // Alert system
    AlertConfiguration alert_config_;
    AlertCallback alert_callback_;
    std::atomic<uint32_t> consecutive_errors_;

    // Performance tracking
    std::unordered_map<std::string, std::chrono::microseconds> subsystem_timings_;
    std::unordered_map<std::string, uint32_t> throughput_metrics_;
    std::unordered_map<std::string, uint32_t> error_counts_;

    void update_current_metrics();
    void check_alert_conditions();
    void perform_adaptive_tuning();
    void cleanup_old_samples();
};

// Main GPU Environmental Audio System with complete error handling
class GpuEnvironmentalAudioSystem {
public:
    struct SystemConfiguration {
        // Processing configuration
        uint32_t max_audio_sources = 1024;
        uint32_t max_reverb_zones = 256;
        uint32_t max_rays_per_source = 16;
        uint32_t max_ray_bounces = 8;
        uint32_t sample_rate = 44100;
        uint32_t buffer_size = 512;

        // Quality settings
        float acoustic_quality_factor = 1.0f;
        uint32_t impulse_response_length = 2048;
        uint32_t fft_size = 1024;
        bool enable_frequency_dependent_processing = true;

        // Performance settings
        bool enable_autonomous_processing = true;
        bool enable_adaptive_quality = true;
        float target_gpu_utilization = 85.0f;
        std::chrono::microseconds max_processing_time{15000}; // 15ms

        // Memory configuration
        GpuEnvironmentalArenaManager::ArenaConfiguration arena_config;

        // Error handling
        bool enable_error_recovery = true;
        uint32_t max_retry_attempts = 3;
        std::chrono::milliseconds timeout_duration{100};
    };

    struct SystemStatus {
        bool is_initialized;
        bool is_processing_enabled;
        bool is_autonomous_mode;
        bool has_recent_errors;
        GpuEnvironmentalPerformanceMonitor::PerformanceMetrics performance;
        GpuEnvironmentalArenaManager::MemoryStats memory_stats;
        std::string last_error_message;
        std::chrono::steady_clock::time_point last_update_time;
    };

    // Error recovery strategies
    enum class RecoveryStrategy {
        RETRY_OPERATION,
        REDUCE_QUALITY,
        FALLBACK_TO_CPU,
        RESTART_SUBSYSTEM,
        DISABLE_FEATURE
    };

    using ErrorRecoveryCallback = std::function<void(GpuEnvironmentalAudioException::ErrorType error_type,
                                                   RecoveryStrategy strategy,
                                                   const std::string& details)>;

    explicit GpuEnvironmentalAudioSystem(graphics::GpuComputeSystem& gpu_system);
    ~GpuEnvironmentalAudioSystem();

    // System lifecycle with error handling
    void initialize_system(const SystemConfiguration& config);
    void shutdown_system();
    void restart_system();

    // Main processing with comprehensive error handling
    void update_environmental_acoustics(ecs::World& world, float delta_time);
    void force_synchronize_gpu_processing();

    // Configuration and control
    void set_configuration(const SystemConfiguration& config);
    const SystemConfiguration& get_configuration() const;
    void enable_processing(bool enable = true);
    void enable_autonomous_mode(bool enable = true);

    // Error handling and recovery
    void set_error_recovery_callback(ErrorRecoveryCallback callback);
    void enable_error_recovery(bool enable = true);
    void clear_error_state();
    bool has_critical_errors() const;

    // Status and monitoring
    SystemStatus get_system_status() const;
    GpuEnvironmentalPerformanceMonitor::PerformanceMetrics get_performance_metrics() const;
    GpuEnvironmentalArenaManager::MemoryStats get_memory_stats() const;

    // Debug and diagnostics
    void enable_debug_mode(bool enable = true);
    std::string generate_diagnostic_report() const;
    void export_performance_data(const std::string& filename) const;

    // Integration with AudioSystem
    void integrate_with_audio_system(AudioSystem& audio_system);

private:
    graphics::GpuComputeSystem& gpu_system_;
    SystemConfiguration config_;
    mutable std::mutex system_mutex_;

    // Core subsystems
    std::unique_ptr<GpuEnvironmentalArenaManager> arena_manager_;
    std::unique_ptr<GpuEnvironmentalPerformanceMonitor> performance_monitor_;

    // System state
    std::atomic<bool> is_initialized_;
    std::atomic<bool> processing_enabled_;
    std::atomic<bool> autonomous_mode_enabled_;
    std::atomic<bool> has_critical_errors_;

    // Error handling
    ErrorRecoveryCallback error_recovery_callback_;
    std::atomic<bool> error_recovery_enabled_;
    std::atomic<uint32_t> consecutive_errors_;
    std::string last_error_message_;

    // Debug and monitoring
    bool debug_mode_enabled_;
    std::atomic<uint32_t> update_counter_;

    // Internal processing methods
    void perform_safe_gpu_update(ecs::World& world, float delta_time);
    void handle_processing_error(const GpuEnvironmentalAudioException& e);
    void attempt_error_recovery(GpuEnvironmentalAudioException::ErrorType error_type);
    void validate_system_state() const;

    // Subsystem management
    void initialize_arena_manager();
    void initialize_performance_monitoring();
    void cleanup_subsystems();

    // Error recovery implementations
    void retry_failed_operation();
    void reduce_processing_quality();
    void fallback_to_cpu_processing();
    void restart_gpu_subsystem();
    void disable_failed_feature();
};

// Integration helper for existing AudioSystem
namespace integration {

// Enhanced AudioSystem methods for GPU environmental processing
void enable_gpu_environmental_processing(AudioSystem& audio_system, graphics::GpuComputeSystem* gpu_system);
void update_gpu_reverb_zones(AudioSystem& audio_system, ecs::World& world, float delta_time);
void compute_environmental_impulse_responses(AudioSystem& audio_system, ecs::World& world);
void process_gpu_occlusion(AudioSystem& audio_system, ecs::World& world, float delta_time);
GpuEnvironmentalPerformanceMonitor::PerformanceMetrics get_gpu_environmental_stats(const AudioSystem& audio_system);

// Error handling integration
void setup_audio_system_error_handling(AudioSystem& audio_system,
                                      GpuEnvironmentalAudioSystem::ErrorRecoveryCallback callback);

// Performance optimization integration
void optimize_audio_system_for_gpu(AudioSystem& audio_system,
                                  const GpuEnvironmentalAudioSystem::SystemConfiguration& target_config);

} // namespace integration

} // namespace lore::audio