// GPU Environmental Audio Error Handling and Performance Monitoring Implementation
// Complete error recovery, performance monitoring, and arena memory management

#include <lore/audio/gpu_environmental_audio.hpp>
#include <lore/graphics/gpu_compute.hpp>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <thread>

namespace lore::audio {

// GpuEnvironmentalArenaManager Implementation
GpuEnvironmentalArenaManager::GpuEnvironmentalArenaManager(graphics::VulkanGpuArenaManager& base_manager)
    : base_manager_(base_manager)
    , allocation_counter_(0)
    , low_memory_condition_(false)
    , memory_debugging_enabled_(false)
    , convolution_arena_id_(0)
    , ray_tracing_arena_id_(0)
    , occlusion_arena_id_(0)
    , reverb_arena_id_(0)
    , output_buffer_arena_id_(0)
{
    // Initialize default configuration
    config_.total_arena_size = 128 * 1024 * 1024;
    config_.convolution_arena_size = 32 * 1024 * 1024;
    config_.ray_tracing_arena_size = 48 * 1024 * 1024;
    config_.occlusion_arena_size = 24 * 1024 * 1024;
    config_.reverb_arena_size = 16 * 1024 * 1024;
    config_.output_buffer_size = 8 * 1024 * 1024;
    config_.alignment_requirement = 256;
    config_.enable_memory_compaction = true;
    config_.compaction_threshold = 0.7f;

    memset(&cached_stats_, 0, sizeof(cached_stats_));
}

GpuEnvironmentalArenaManager::~GpuEnvironmentalArenaManager() {
    shutdown_environmental_arenas();
}

void GpuEnvironmentalArenaManager::initialize_environmental_arenas(const ArenaConfiguration& config) {
    std::lock_guard<std::mutex> lock(arena_mutex_);
    config_ = config;

    try {
        // Create specialized arenas for different environmental audio subsystems
        convolution_arena_id_ = base_manager_.create_arena(
            config_.convolution_arena_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        ray_tracing_arena_id_ = base_manager_.create_arena(
            config_.ray_tracing_arena_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        occlusion_arena_id_ = base_manager_.create_arena(
            config_.occlusion_arena_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        reverb_arena_id_ = base_manager_.create_arena(
            config_.reverb_arena_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        output_buffer_arena_id_ = base_manager_.create_arena(
            config_.output_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_GPU_TO_CPU
        );

        // Initialize tracking structures
        active_allocations_.clear();
        active_allocations_.reserve(1024); // Reserve space for many allocations

        low_memory_condition_.store(false);
        allocation_counter_.store(0);

        if (memory_debugging_enabled_) {
            std::cout << "GPU Environmental Arena Manager: Initialized "
                      << (config_.total_arena_size / (1024 * 1024)) << " MB of arena memory" << std::endl;
            log_memory_usage();
        }

    } catch (const std::exception& e) {
        throw GpuEnvironmentalAudioException(
            GpuEnvironmentalAudioException::ErrorType::ARENA_ALLOCATION_FAILED,
            "Failed to initialize environmental arenas: " + std::string(e.what())
        );
    }
}

void GpuEnvironmentalArenaManager::shutdown_environmental_arenas() {
    std::lock_guard<std::mutex> lock(arena_mutex_);

    try {
        // Deallocate all active allocations
        for (const auto& allocation : active_allocations_) {
            if (allocation.allocation.is_valid) {
                base_manager_.deallocate_on_gpu(allocation.allocation);
            }
        }
        active_allocations_.clear();

        // Destroy arenas
        if (convolution_arena_id_ != 0) {
            base_manager_.destroy_arena(convolution_arena_id_);
            convolution_arena_id_ = 0;
        }
        if (ray_tracing_arena_id_ != 0) {
            base_manager_.destroy_arena(ray_tracing_arena_id_);
            ray_tracing_arena_id_ = 0;
        }
        if (occlusion_arena_id_ != 0) {
            base_manager_.destroy_arena(occlusion_arena_id_);
            occlusion_arena_id_ = 0;
        }
        if (reverb_arena_id_ != 0) {
            base_manager_.destroy_arena(reverb_arena_id_);
            reverb_arena_id_ = 0;
        }
        if (output_buffer_arena_id_ != 0) {
            base_manager_.destroy_arena(output_buffer_arena_id_);
            output_buffer_arena_id_ = 0;
        }

        if (memory_debugging_enabled_) {
            std::cout << "GPU Environmental Arena Manager: Shutdown complete" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error during arena shutdown: " << e.what() << std::endl;
    }
}

GpuEnvironmentalArenaManager::ArenaAllocationInfo GpuEnvironmentalArenaManager::allocate_environmental_buffer(
    const std::string& name,
    size_t size,
    bool is_persistent,
    uint32_t preferred_arena
) {
    std::lock_guard<std::mutex> lock(arena_mutex_);

    uint32_t arena_id = preferred_arena;
    if (arena_id == 0) {
        arena_id = select_best_arena_for_allocation(size, is_persistent);
    }

    // Attempt allocation with retry logic
    constexpr int max_retry_attempts = 3;
    for (int attempt = 0; attempt < max_retry_attempts; ++attempt) {
        try {
            auto allocation = base_manager_.allocate_on_gpu(arena_id, static_cast<uint32_t>(size), config_.alignment_requirement);

            if (allocation.is_valid) {
                ArenaAllocationInfo info;
                info.allocation = allocation;
                info.size = size;
                info.arena_id = arena_id;
                info.allocation_time = std::chrono::steady_clock::now();
                info.debug_name = name;
                info.is_persistent = is_persistent;

                active_allocations_.push_back(info);
                allocation_counter_.fetch_add(1);

                if (memory_debugging_enabled_) {
                    std::cout << "Allocated " << (size / 1024) << " KB for '" << name
                              << "' in arena " << arena_id << std::endl;
                }

                check_low_memory_condition();
                return info;
            }
        } catch (const std::exception& e) {
            if (memory_debugging_enabled_) {
                std::cout << "Allocation attempt " << (attempt + 1) << " failed for '" << name
                          << "': " << e.what() << std::endl;
            }
        }

        // Try memory recovery strategies
        if (attempt < max_retry_attempts - 1) {
            if (config_.enable_memory_compaction) {
                compact_arenas_if_needed();
            }

            // If still failing, try a different arena
            if (attempt == 1) {
                arena_id = select_best_arena_for_allocation(size, is_persistent);
            }
        }
    }

    // All retry attempts failed
    throw GpuEnvironmentalAudioException(
        GpuEnvironmentalAudioException::ErrorType::ARENA_ALLOCATION_FAILED,
        "Failed to allocate " + std::to_string(size) + " bytes for '" + name + "' after " +
        std::to_string(max_retry_attempts) + " attempts"
    );
}

void GpuEnvironmentalArenaManager::deallocate_environmental_buffer(const ArenaAllocationInfo& allocation) {
    std::lock_guard<std::mutex> lock(arena_mutex_);

    try {
        // Find and remove from active allocations
        auto it = std::find_if(active_allocations_.begin(), active_allocations_.end(),
            [&](const ArenaAllocationInfo& info) {
                return info.allocation.offset == allocation.allocation.offset &&
                       info.allocation.arena_id == allocation.allocation.arena_id;
            });

        if (it != active_allocations_.end()) {
            base_manager_.deallocate_on_gpu(it->allocation);

            if (memory_debugging_enabled_) {
                std::cout << "Deallocated " << (it->size / 1024) << " KB for '" << it->debug_name
                          << "' from arena " << it->arena_id << std::endl;
            }

            active_allocations_.erase(it);
            check_low_memory_condition();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error deallocating buffer '" << allocation.debug_name << "': " << e.what() << std::endl;
    }
}

void GpuEnvironmentalArenaManager::compact_arenas_if_needed() {
    update_memory_stats();

    if (cached_stats_.fragmentation_ratio > config_.compaction_threshold) {
        auto compaction_start = std::chrono::steady_clock::now();

        if (memory_debugging_enabled_) {
            std::cout << "Starting arena compaction (fragmentation: "
                      << (cached_stats_.fragmentation_ratio * 100.0f) << "%)" << std::endl;
        }

        // Compact each arena
        base_manager_.compact_arena_on_gpu(convolution_arena_id_);
        base_manager_.compact_arena_on_gpu(ray_tracing_arena_id_);
        base_manager_.compact_arena_on_gpu(occlusion_arena_id_);
        base_manager_.compact_arena_on_gpu(reverb_arena_id_);
        base_manager_.compact_arena_on_gpu(output_buffer_arena_id_);

        auto compaction_end = std::chrono::steady_clock::now();
        auto compaction_time = std::chrono::duration_cast<std::chrono::microseconds>(compaction_end - compaction_start);

        cached_stats_.compaction_operations++;
        cached_stats_.last_compaction_time = compaction_time;

        if (memory_debugging_enabled_) {
            std::cout << "Arena compaction completed in " << compaction_time.count() << " microseconds" << std::endl;
        }
    }
}

GpuEnvironmentalArenaManager::MemoryStats GpuEnvironmentalArenaManager::get_memory_stats() const {
    update_memory_stats();
    return cached_stats_;
}

void GpuEnvironmentalArenaManager::update_memory_stats() const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_update_);

    if (time_since_update.count() < 100) { // Update at most every 100ms
        return;
    }

    last_stats_update_ = now;

    cached_stats_.total_allocated = config_.total_arena_size;
    cached_stats_.total_used = 0;
    cached_stats_.active_allocations = static_cast<uint32_t>(active_allocations_.size());

    // Calculate used memory from active allocations
    for (const auto& allocation : active_allocations_) {
        cached_stats_.total_used += allocation.size;
    }

    cached_stats_.total_free = cached_stats_.total_allocated - cached_stats_.total_used;

    // Simplified fragmentation calculation
    if (cached_stats_.total_allocated > 0) {
        cached_stats_.fragmentation_ratio = 1.0f - (static_cast<float>(cached_stats_.total_free) /
                                                   static_cast<float>(cached_stats_.total_allocated));
    }

    // Estimate largest free block (simplified)
    cached_stats_.largest_free_block = cached_stats_.total_free;
}

uint32_t GpuEnvironmentalArenaManager::select_best_arena_for_allocation(size_t size, bool is_persistent) const {
    // Select arena based on allocation type and current usage
    if (size <= 1024 * 1024) { // Small allocations
        return convolution_arena_id_;
    } else if (size <= 4 * 1024 * 1024) { // Medium allocations
        return reverb_arena_id_;
    } else if (size <= 16 * 1024 * 1024) { // Large allocations
        return occlusion_arena_id_;
    } else { // Very large allocations
        return ray_tracing_arena_id_;
    }
}

void GpuEnvironmentalArenaManager::check_low_memory_condition() {
    update_memory_stats();

    bool is_low_memory = (cached_stats_.total_free < cached_stats_.total_allocated * 0.2f) || // Less than 20% free
                        (cached_stats_.fragmentation_ratio > 0.8f); // Highly fragmented

    bool was_low_memory = low_memory_condition_.exchange(is_low_memory);

    if (is_low_memory && !was_low_memory && memory_debugging_enabled_) {
        std::cout << "WARNING: Low memory condition detected (Free: "
                  << (cached_stats_.total_free / (1024 * 1024)) << " MB, Fragmentation: "
                  << (cached_stats_.fragmentation_ratio * 100.0f) << "%)" << std::endl;
    }
}

void GpuEnvironmentalArenaManager::log_memory_usage() const {
    update_memory_stats();

    std::cout << "=== GPU Environmental Arena Memory Usage ===" << std::endl;
    std::cout << "Total Allocated: " << (cached_stats_.total_allocated / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Total Used:      " << (cached_stats_.total_used / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Total Free:      " << (cached_stats_.total_free / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Active Allocations: " << cached_stats_.active_allocations << std::endl;
    std::cout << "Fragmentation:   " << (cached_stats_.fragmentation_ratio * 100.0f) << "%" << std::endl;
    std::cout << "Failed Allocations: " << cached_stats_.failed_allocations << std::endl;
    std::cout << "Compaction Operations: " << cached_stats_.compaction_operations << std::endl;
    std::cout << "=============================================" << std::endl;
}

// GpuEnvironmentalPerformanceMonitor Implementation
GpuEnvironmentalPerformanceMonitor::GpuEnvironmentalPerformanceMonitor()
    : monitoring_active_(false)
    , adaptive_tuning_enabled_(false)
    , consecutive_errors_(0)
{
    memset(&current_metrics_, 0, sizeof(current_metrics_));

    // Set default alert configuration
    alert_config_.max_gpu_utilization = 95.0f;
    alert_config_.max_frame_time = std::chrono::microseconds(20000);
    alert_config_.max_consecutive_errors = 5;
    alert_config_.min_acoustic_quality = 0.8f;
    alert_config_.max_memory_usage = 128 * 1024 * 1024;
}

GpuEnvironmentalPerformanceMonitor::~GpuEnvironmentalPerformanceMonitor() {
    stop_monitoring();
}

void GpuEnvironmentalPerformanceMonitor::start_monitoring(const AlertConfiguration& config) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    alert_config_ = config;
    monitoring_active_ = true;
    consecutive_errors_.store(0);

    // Initialize historical data
    historical_data_.samples.clear();
    historical_data_.samples.reserve(historical_data_.max_samples);
    historical_data_.oldest_sample_time = std::chrono::steady_clock::now();

    std::cout << "GPU Environmental Performance Monitor: Started monitoring" << std::endl;
}

void GpuEnvironmentalPerformanceMonitor::stop_monitoring() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    monitoring_active_ = false;
    std::cout << "GPU Environmental Performance Monitor: Stopped monitoring" << std::endl;
}

void GpuEnvironmentalPerformanceMonitor::begin_frame_timing() {
    if (!monitoring_active_) return;
    frame_start_time_ = std::chrono::high_resolution_clock::now();
}

void GpuEnvironmentalPerformanceMonitor::end_frame_timing() {
    if (!monitoring_active_) return;

    auto frame_end_time = std::chrono::high_resolution_clock::now();
    current_metrics_.frame_time = std::chrono::duration_cast<std::chrono::microseconds>(
        frame_end_time - frame_start_time_);

    update_current_metrics();
    check_alert_conditions();

    if (adaptive_tuning_enabled_) {
        perform_adaptive_tuning();
    }
}

void GpuEnvironmentalPerformanceMonitor::record_subsystem_timing(const std::string& subsystem, std::chrono::microseconds time) {
    if (!monitoring_active_) return;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    subsystem_timings_[subsystem] = time;

    // Update specific subsystem metrics
    if (subsystem == "convolution") {
        current_metrics_.convolution_time = time;
    } else if (subsystem == "ray_tracing") {
        current_metrics_.ray_tracing_time = time;
    } else if (subsystem == "occlusion") {
        current_metrics_.occlusion_time = time;
    } else if (subsystem == "reverb") {
        current_metrics_.reverb_time = time;
    }
}

void GpuEnvironmentalPerformanceMonitor::record_error(const std::string& error_type) {
    if (!monitoring_active_) return;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    error_counts_[error_type]++;

    // Update specific error metrics
    if (error_type == "compute_shader") {
        current_metrics_.compute_shader_errors++;
    } else if (error_type == "memory_allocation") {
        current_metrics_.memory_allocation_failures++;
    } else if (error_type == "pipeline_stall") {
        current_metrics_.pipeline_stalls++;
    } else if (error_type == "timeout") {
        current_metrics_.timeout_events++;
    }

    consecutive_errors_.fetch_add(1);
}

void GpuEnvironmentalPerformanceMonitor::update_current_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Calculate GPU utilization based on subsystem timings
    auto total_subsystem_time = current_metrics_.convolution_time +
                               current_metrics_.ray_tracing_time +
                               current_metrics_.occlusion_time +
                               current_metrics_.reverb_time;

    if (current_metrics_.frame_time.count() > 0) {
        current_metrics_.gpu_utilization_percentage =
            std::min(100.0f, static_cast<float>(total_subsystem_time.count()) /
                           static_cast<float>(current_metrics_.frame_time.count()) * 100.0f);
    }

    // Calculate throughput metrics
    if (current_metrics_.frame_time.count() > 0) {
        float frame_rate = 1000000.0f / static_cast<float>(current_metrics_.frame_time.count());
        current_metrics_.audio_sources_processed_per_second = static_cast<uint32_t>(
            throughput_metrics_["audio_sources"] * frame_rate);
        current_metrics_.rays_traced_per_second = static_cast<uint32_t>(
            throughput_metrics_["rays_traced"] * frame_rate);
        current_metrics_.occlusion_tests_per_second = static_cast<uint32_t>(
            throughput_metrics_["occlusion_tests"] * frame_rate);
        current_metrics_.convolution_operations_per_second = static_cast<uint32_t>(
            throughput_metrics_["convolution_ops"] * frame_rate);
    }

    // Calculate quality metrics (simplified)
    current_metrics_.acoustic_accuracy_score = std::max(0.0f,
        1.0f - static_cast<float>(current_metrics_.compute_shader_errors) * 0.1f);
    current_metrics_.impulse_response_quality = std::max(0.0f,
        1.0f - static_cast<float>(current_metrics_.memory_allocation_failures) * 0.05f);

    // Store historical sample
    historical_data_.samples.push_back(current_metrics_);
    if (historical_data_.samples.size() > historical_data_.max_samples) {
        historical_data_.samples.erase(historical_data_.samples.begin());
    }
}

void GpuEnvironmentalPerformanceMonitor::check_alert_conditions() {
    if (!alert_callback_) return;

    std::string alert_type;
    std::string message;
    bool should_alert = false;

    if (current_metrics_.gpu_utilization_percentage > alert_config_.max_gpu_utilization) {
        alert_type = "HIGH_GPU_UTILIZATION";
        message = "GPU utilization exceeded " + std::to_string(alert_config_.max_gpu_utilization) +
                 "% (current: " + std::to_string(current_metrics_.gpu_utilization_percentage) + "%)";
        should_alert = true;
    }

    if (current_metrics_.frame_time > alert_config_.max_frame_time) {
        alert_type = "HIGH_FRAME_TIME";
        message = "Frame time exceeded limit (current: " +
                 std::to_string(current_metrics_.frame_time.count()) + " microseconds)";
        should_alert = true;
    }

    if (consecutive_errors_.load() > alert_config_.max_consecutive_errors) {
        alert_type = "CONSECUTIVE_ERRORS";
        message = "Too many consecutive errors: " + std::to_string(consecutive_errors_.load());
        should_alert = true;
    }

    if (current_metrics_.acoustic_accuracy_score < alert_config_.min_acoustic_quality) {
        alert_type = "LOW_ACOUSTIC_QUALITY";
        message = "Acoustic quality below threshold (current: " +
                 std::to_string(current_metrics_.acoustic_accuracy_score) + ")";
        should_alert = true;
    }

    if (current_metrics_.gpu_memory_used_bytes > alert_config_.max_memory_usage) {
        alert_type = "HIGH_MEMORY_USAGE";
        message = "Memory usage exceeded limit (current: " +
                 std::to_string(current_metrics_.gpu_memory_used_bytes / (1024 * 1024)) + " MB)";
        should_alert = true;
    }

    if (should_alert) {
        alert_callback_(alert_type, message, current_metrics_);
    }
}

float GpuEnvironmentalPerformanceMonitor::calculate_performance_score() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Weighted performance score calculation
    float utilization_score = std::min(1.0f, (100.0f - current_metrics_.gpu_utilization_percentage) / 100.0f);
    float timing_score = std::min(1.0f, static_cast<float>(alert_config_.max_frame_time.count()) /
                                       static_cast<float>(current_metrics_.frame_time.count()));
    float quality_score = current_metrics_.acoustic_accuracy_score;
    float error_score = std::max(0.0f, 1.0f - static_cast<float>(consecutive_errors_.load()) * 0.1f);

    return (utilization_score * 0.3f + timing_score * 0.3f + quality_score * 0.2f + error_score * 0.2f);
}

std::string GpuEnvironmentalPerformanceMonitor::generate_performance_report() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::ostringstream report;
    report << std::fixed << std::setprecision(2);

    report << "=== GPU Environmental Audio Performance Report ===" << std::endl;
    report << "GPU Utilization: " << current_metrics_.gpu_utilization_percentage << "%" << std::endl;
    report << "Frame Time: " << current_metrics_.frame_time.count() << " μs" << std::endl;
    report << "Performance Score: " << (calculate_performance_score() * 100.0f) << "%" << std::endl;
    report << std::endl;

    report << "Subsystem Timing:" << std::endl;
    report << "  Convolution: " << current_metrics_.convolution_time.count() << " μs" << std::endl;
    report << "  Ray Tracing: " << current_metrics_.ray_tracing_time.count() << " μs" << std::endl;
    report << "  Occlusion: " << current_metrics_.occlusion_time.count() << " μs" << std::endl;
    report << "  Reverb: " << current_metrics_.reverb_time.count() << " μs" << std::endl;
    report << std::endl;

    report << "Throughput Metrics:" << std::endl;
    report << "  Audio Sources/sec: " << current_metrics_.audio_sources_processed_per_second << std::endl;
    report << "  Rays Traced/sec: " << current_metrics_.rays_traced_per_second << std::endl;
    report << "  Occlusion Tests/sec: " << current_metrics_.occlusion_tests_per_second << std::endl;
    report << "  Convolution Ops/sec: " << current_metrics_.convolution_operations_per_second << std::endl;
    report << std::endl;

    report << "Quality Metrics:" << std::endl;
    report << "  Acoustic Accuracy: " << (current_metrics_.acoustic_accuracy_score * 100.0f) << "%" << std::endl;
    report << "  Impulse Response Quality: " << (current_metrics_.impulse_response_quality * 100.0f) << "%" << std::endl;
    report << std::endl;

    report << "Error Tracking:" << std::endl;
    report << "  Compute Shader Errors: " << current_metrics_.compute_shader_errors << std::endl;
    report << "  Memory Allocation Failures: " << current_metrics_.memory_allocation_failures << std::endl;
    report << "  Pipeline Stalls: " << current_metrics_.pipeline_stalls << std::endl;
    report << "  Timeout Events: " << current_metrics_.timeout_events << std::endl;
    report << "  Consecutive Errors: " << consecutive_errors_.load() << std::endl;
    report << std::endl;

    report << "Resource Utilization:" << std::endl;
    report << "  GPU Memory Used: " << (current_metrics_.gpu_memory_used_bytes / (1024 * 1024)) << " MB" << std::endl;
    report << "  Active Compute Dispatches: " << current_metrics_.active_compute_dispatches << std::endl;
    report << "  Buffer Uploads/Frame: " << current_metrics_.buffer_uploads_per_frame << std::endl;
    report << "=================================================" << std::endl;

    return report.str();
}

void GpuEnvironmentalPerformanceMonitor::export_metrics_to_file(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw GpuEnvironmentalAudioException(
            GpuEnvironmentalAudioException::ErrorType::INVALID_CONFIGURATION,
            "Failed to open file for metrics export: " + filename
        );
    }

    file << generate_performance_report();

    // Export historical data in CSV format
    file << std::endl << "Historical Data (CSV):" << std::endl;
    file << "Timestamp,GPU_Utilization,Frame_Time_us,Acoustic_Accuracy,Memory_Used_MB" << std::endl;

    auto base_time = historical_data_.oldest_sample_time;
    for (size_t i = 0; i < historical_data_.samples.size(); ++i) {
        const auto& sample = historical_data_.samples[i];
        auto timestamp = base_time + std::chrono::seconds(i);
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();

        file << timestamp_ms << ","
             << sample.gpu_utilization_percentage << ","
             << sample.frame_time.count() << ","
             << sample.acoustic_accuracy_score << ","
             << (sample.gpu_memory_used_bytes / (1024 * 1024)) << std::endl;
    }

    file.close();
    std::cout << "Performance metrics exported to: " << filename << std::endl;
}

} // namespace lore::audio