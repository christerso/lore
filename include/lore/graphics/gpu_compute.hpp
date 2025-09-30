#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <array>
#include <span>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lore::graphics {

// Forward declarations
class GraphicsSystem;

// GPU Arena Buffer Management - Complete zero-allocation GPU memory system
class VulkanGpuArenaManager {
public:
    struct ArenaAllocation {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
        uint32_t arena_id;
        bool is_valid;
    };

    struct ArenaBlock {
        VkBuffer buffer;
        VmaAllocation allocation;
        VkDeviceSize size;
        std::atomic<VkDeviceSize> next_offset;
        std::atomic<uint32_t> allocation_count;
        VkBufferUsageFlags usage_flags;
        VmaMemoryUsage memory_usage;
    };

    // GPU-managed allocation metadata stored in GPU buffer
    struct GpuArenaMetadata {
        std::atomic<uint32_t> next_offset;
        uint32_t total_size;
        std::atomic<uint32_t> free_list_head;
        std::atomic<uint32_t> allocation_count;
        uint32_t arena_id;
        uint32_t padding[3]; // Ensure 32-byte alignment
    };

    struct AllocationRequest {
        uint32_t size;
        uint32_t alignment;
        uint32_t arena_id;
        uint32_t result_offset; // Written by GPU
        uint32_t success;       // Written by GPU
        uint32_t padding[3];    // Ensure 32-byte alignment
    };

    VulkanGpuArenaManager(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator);
    ~VulkanGpuArenaManager();

    // Arena management
    uint32_t create_arena(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
    void destroy_arena(uint32_t arena_id);

    // GPU autonomous allocation - runs entirely on GPU
    ArenaAllocation allocate_on_gpu(uint32_t arena_id, uint32_t size, uint32_t alignment = 256);
    void deallocate_on_gpu(const ArenaAllocation& allocation);

    // Batch allocation for efficiency
    std::vector<ArenaAllocation> allocate_batch_on_gpu(uint32_t arena_id,
                                                       const std::vector<uint32_t>& sizes,
                                                       uint32_t alignment = 256);

    // Arena compaction - GPU performs memory defragmentation
    void compact_arena_on_gpu(uint32_t arena_id);

    // Memory statistics
    struct ArenaStats {
        VkDeviceSize total_size;
        VkDeviceSize allocated_size;
        VkDeviceSize free_size;
        uint32_t allocation_count;
        float fragmentation_ratio;
    };
    ArenaStats get_arena_stats(uint32_t arena_id) const;

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;

    // GPU compute pipelines for memory management
    VkPipeline allocator_pipeline_;
    VkPipeline deallocator_pipeline_;
    VkPipeline compactor_pipeline_;
    VkPipelineLayout pipeline_layout_;
    VkDescriptorSetLayout descriptor_set_layout_;
    VkDescriptorPool descriptor_pool_;

    // Command infrastructure for GPU memory operations
    VkCommandPool compute_command_pool_;
    VkQueue compute_queue_;
    uint32_t compute_queue_family_;

    // Arena storage
    std::vector<std::unique_ptr<ArenaBlock>> arenas_;
    std::mutex arenas_mutex_;

    // GPU metadata buffers
    VkBuffer allocation_requests_buffer_;
    VmaAllocation allocation_requests_allocation_;
    VkBuffer metadata_buffer_;
    VmaAllocation metadata_allocation_;
    VkBuffer free_list_buffer_;
    VmaAllocation free_list_allocation_;

    void initialize_compute_pipelines();
    void create_gpu_memory_buffers();
    VkResult dispatch_gpu_allocation(uint32_t arena_id, const std::vector<AllocationRequest>& requests);
};

// CPU Arena Allocator for minimal CPU operations
class CpuArenaAllocator {
public:
    explicit CpuArenaAllocator(size_t size);
    ~CpuArenaAllocator();

    template<typename T>
    T* allocate(size_t count = 1) {
        size_t element_size = sizeof(T);
        size_t total_size = element_size * count;
        size_t aligned_size = (total_size + alignment_ - 1) & ~(alignment_ - 1);

        if (current_offset_ + aligned_size > size_) {
            return nullptr; // Arena exhausted
        }

        T* ptr = reinterpret_cast<T*>(static_cast<char*>(buffer_) + current_offset_);
        current_offset_ += aligned_size;
        return ptr;
    }

    template<typename T>
    std::span<T> allocate_array(size_t count) {
        T* ptr = allocate<T>(count);
        return ptr ? std::span<T>{ptr, count} : std::span<T>{};
    }

    void reset() { current_offset_ = 0; }
    size_t bytes_used() const { return current_offset_; }
    size_t bytes_remaining() const { return size_ - current_offset_; }

    // Stack-based scoping for automatic cleanup
    template<typename F>
    auto scope(F&& func) -> decltype(func(*this)) {
        size_t saved_offset = current_offset_;
        auto result = func(*this);
        current_offset_ = saved_offset;
        return result;
    }

private:
    void* buffer_;
    size_t size_;
    std::atomic<size_t> current_offset_;
    size_t alignment_;
};

// SPIR-V Shader Compilation and Caching System
class ShaderCompiler {
public:
    struct ShaderModule {
        VkShaderModule module;
        std::vector<uint32_t> spirv_code;
        std::string entry_point;
        VkShaderStageFlags stage;
        std::filesystem::file_time_type last_modified;
    };

    struct ComputeShaderInfo {
        std::string source_path;
        std::string entry_point = "main";
        std::vector<std::string> includes;
        std::unordered_map<std::string, std::string> definitions;
    };

    ShaderCompiler(VkDevice device);
    ~ShaderCompiler();

    // Compile and cache shaders
    std::shared_ptr<ShaderModule> compile_compute_shader(const ComputeShaderInfo& info);
    std::shared_ptr<ShaderModule> load_precompiled_shader(const std::string& spirv_path);

    // Hot-reload support
    void enable_hot_reload(bool enable = true);
    void check_for_shader_updates();
    void set_reload_callback(std::function<void(const std::string&)> callback);

    // Shader reflection
    struct ShaderReflection {
        struct DescriptorBinding {
            uint32_t set;
            uint32_t binding;
            VkDescriptorType type;
            uint32_t count;
            VkShaderStageFlags stage_flags;
        };
        std::vector<DescriptorBinding> bindings;
        glm::uvec3 local_size;
    };
    ShaderReflection reflect_compute_shader(const ShaderModule& module);

private:
    VkDevice device_;
    std::unordered_map<std::string, std::shared_ptr<ShaderModule>> shader_cache_;
    std::mutex cache_mutex_;

    // Hot-reload infrastructure
    std::atomic<bool> hot_reload_enabled_;
    std::thread reload_thread_;
    std::atomic<bool> reload_thread_running_;
    std::function<void(const std::string&)> reload_callback_;

    std::vector<uint32_t> compile_glsl_to_spirv(const std::string& source,
                                                VkShaderStageFlagBits stage,
                                                const std::string& entry_point,
                                                const std::unordered_map<std::string, std::string>& definitions);
    void hot_reload_worker();
};

// Compute Pipeline Management with optimal workgroup sizing
class ComputePipelineManager {
public:
    struct PipelineConfig {
        std::shared_ptr<ShaderCompiler::ShaderModule> compute_shader;
        std::vector<VkDescriptorSetLayout> descriptor_layouts;
        std::vector<VkPushConstantRange> push_constant_ranges;
        VkPipelineCreateFlags flags = 0;
    };

    struct DispatchInfo {
        VkPipeline pipeline;
        glm::uvec3 workgroup_count;
        glm::uvec3 local_size;
        std::vector<VkDescriptorSet> descriptor_sets;
        std::vector<uint8_t> push_constants;
    };

    ComputePipelineManager(VkDevice device, VkPhysicalDevice physical_device);
    ~ComputePipelineManager();

    VkPipeline create_pipeline(const std::string& name, const PipelineConfig& config);
    void destroy_pipeline(const std::string& name);

    // Optimal workgroup calculation based on GPU characteristics
    glm::uvec3 calculate_optimal_workgroups(glm::uvec3 total_work_items,
                                           glm::uvec3 local_size,
                                           const std::string& pipeline_name);

    // Dispatch operations
    void record_dispatch(VkCommandBuffer cmd, const DispatchInfo& info);
    VkResult dispatch_compute(const DispatchInfo& info, VkFence completion_fence = VK_NULL_HANDLE);

    // Performance profiling
    struct PipelineStats {
        uint64_t total_dispatches;
        uint64_t total_workgroups;
        std::chrono::nanoseconds total_gpu_time;
        std::chrono::nanoseconds average_dispatch_time;
    };
    PipelineStats get_pipeline_stats(const std::string& name) const;

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VkQueue compute_queue_;
    uint32_t compute_queue_family_;
    VkCommandPool command_pool_;

    std::unordered_map<std::string, VkPipeline> pipelines_;
    std::unordered_map<std::string, VkPipelineLayout> pipeline_layouts_;
    std::unordered_map<std::string, PipelineConfig> pipeline_configs_;
    std::unordered_map<std::string, PipelineStats> pipeline_stats_;
    std::mutex pipelines_mutex_;

    // GPU characteristics for optimization
    VkPhysicalDeviceProperties device_properties_;
    VkPhysicalDeviceSubgroupProperties subgroup_properties_;
    uint32_t max_workgroup_size_[3];
    uint32_t max_workgroup_invocations_;
};

// GPU Physics Simulation System
class GpuPhysicsSystem {
public:
    struct alignas(16) RigidBody {
        glm::vec3 position;
        float mass;
        glm::vec3 velocity;
        float restitution;
        glm::vec3 angular_velocity;
        float friction;
        glm::quat orientation;
        glm::vec3 force_accumulator;
        float inv_mass;
        glm::vec3 torque_accumulator;
        float padding;
    };

    struct CollisionShape {
        enum Type : uint32_t { SPHERE = 0, BOX = 1, CAPSULE = 2 };
        Type type;
        glm::vec3 extents; // radius for sphere, half-extents for box, radius+height for capsule
        uint32_t material_id;
        glm::vec3 center_offset;
        uint32_t padding[3];
    };

    struct Collision {
        uint32_t body_a;
        uint32_t body_b;
        glm::vec3 contact_point;
        float penetration;
        glm::vec3 contact_normal;
        float impulse_magnitude;
    };

    GpuPhysicsSystem(VkDevice device, VkPhysicalDevice physical_device,
                     VmaAllocator allocator, VulkanGpuArenaManager& arena_manager);
    ~GpuPhysicsSystem();

    void initialize(uint32_t max_rigid_bodies = 100000);
    void shutdown();

    // Entity management
    uint32_t create_rigid_body(const RigidBody& body, const CollisionShape& shape);
    void destroy_rigid_body(uint32_t body_id);
    void update_rigid_body(uint32_t body_id, const RigidBody& body);

    // Simulation
    void simulate_step(float delta_time);
    void set_gravity(const glm::vec3& gravity);

    // GPU-optimized broad-phase collision detection
    void update_spatial_grid();
    void detect_collisions();
    void resolve_collisions();
    void integrate_physics(float delta_time);

    // Performance monitoring
    struct PhysicsStats {
        uint32_t active_bodies;
        uint32_t collision_tests;
        uint32_t collisions_detected;
        std::chrono::microseconds simulation_time;
    };
    PhysicsStats get_stats() const { return stats_; }

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;
    VulkanGpuArenaManager& arena_manager_;

    // Compute pipelines for physics
    VkPipeline integration_pipeline_;
    VkPipeline collision_detection_pipeline_;
    VkPipeline collision_resolution_pipeline_;
    VkPipeline spatial_grid_update_pipeline_;

    // GPU data structures
    VulkanGpuArenaManager::ArenaAllocation rigid_bodies_buffer_;
    VulkanGpuArenaManager::ArenaAllocation collision_shapes_buffer_;
    VulkanGpuArenaManager::ArenaAllocation collisions_buffer_;
    VulkanGpuArenaManager::ArenaAllocation spatial_grid_buffer_;

    uint32_t max_rigid_bodies_;
    std::atomic<uint32_t> active_body_count_;
    glm::vec3 gravity_;

    PhysicsStats stats_;

    void create_physics_pipelines();
    void dispatch_physics_compute(VkPipeline pipeline, uint32_t workgroup_count);
};

// GPU Particle System with 1M+ particle support
class GpuParticleSystem {
public:
    struct alignas(16) Particle {
        glm::vec3 position;
        float life;
        glm::vec3 velocity;
        float max_life;
        glm::vec3 acceleration;
        float size;
        glm::vec4 color;
        uint32_t emitter_id;
        uint32_t padding[3];
    };

    struct ParticleEmitter {
        glm::vec3 position;
        float emission_rate;
        glm::vec3 velocity_base;
        float velocity_variation;
        glm::vec3 acceleration;
        float life_time;
        glm::vec4 color_start;
        glm::vec4 color_end;
        float size_start;
        float size_end;
        uint32_t max_particles;
        uint32_t active_particles;
        uint32_t padding[2];
    };

    GpuParticleSystem(VkDevice device, VkPhysicalDevice physical_device,
                      VmaAllocator allocator, VulkanGpuArenaManager& arena_manager);
    ~GpuParticleSystem();

    void initialize(uint32_t max_particles = 1000000);
    void shutdown();

    // Emitter management
    uint32_t create_emitter(const ParticleEmitter& emitter);
    void destroy_emitter(uint32_t emitter_id);
    void update_emitter(uint32_t emitter_id, const ParticleEmitter& emitter);

    // Simulation
    void update_particles(float delta_time);
    void emit_particles();

    // GPU-optimized sorting for alpha blending
    void sort_particles_by_depth(const glm::vec3& camera_position);

    // Rendering data access
    const VulkanGpuArenaManager::ArenaAllocation& get_particle_buffer() const { return particles_buffer_; }
    uint32_t get_active_particle_count() const { return active_particle_count_.load(); }

    struct ParticleStats {
        uint32_t total_particles;
        uint32_t active_particles;
        uint32_t particles_born;
        uint32_t particles_died;
        std::chrono::microseconds update_time;
    };
    ParticleStats get_stats() const { return stats_; }

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;
    VulkanGpuArenaManager& arena_manager_;

    // Compute pipelines for particles
    VkPipeline emission_pipeline_;
    VkPipeline update_pipeline_;
    VkPipeline sorting_pipeline_;
    VkPipeline compaction_pipeline_;

    // GPU data structures
    VulkanGpuArenaManager::ArenaAllocation particles_buffer_;
    VulkanGpuArenaManager::ArenaAllocation emitters_buffer_;
    VulkanGpuArenaManager::ArenaAllocation alive_list_buffer_;
    VulkanGpuArenaManager::ArenaAllocation dead_list_buffer_;

    uint32_t max_particles_;
    std::atomic<uint32_t> active_particle_count_;
    std::vector<ParticleEmitter> emitters_;
    std::mutex emitters_mutex_;

    ParticleStats stats_;

    void create_particle_pipelines();
    void dispatch_particle_compute(VkPipeline pipeline, uint32_t workgroup_count);
};

// ECS Compute Integration - GPU-driven component processing
class EcsComputeIntegration {
public:
    struct TransformComponent {
        glm::mat4 model_matrix;
        glm::vec3 position;
        float scale;
        glm::quat rotation;
        uint32_t dirty_flag;
        uint32_t padding[3];
    };

    struct VelocityComponent {
        glm::vec3 linear;
        float angular_speed;
        glm::vec3 angular_axis;
        uint32_t padding;
    };

    EcsComputeIntegration(VkDevice device, VkPhysicalDevice physical_device,
                          VmaAllocator allocator, VulkanGpuArenaManager& arena_manager);
    ~EcsComputeIntegration();

    void initialize(uint32_t max_entities = 1000000);
    void shutdown();

    // Component management
    template<typename T>
    void register_component_type(const std::string& name);

    template<typename T>
    void add_component_to_entity(uint32_t entity_id, const T& component);

    template<typename T>
    void update_component(uint32_t entity_id, const T& component);

    // System execution - all runs on GPU
    void execute_transform_system(float delta_time);
    void execute_culling_system(const glm::mat4& view_projection);
    void execute_custom_system(const std::string& system_name, float delta_time);

    // Batch operations for efficiency
    void batch_update_transforms(const std::vector<std::pair<uint32_t, TransformComponent>>& updates);

    struct EcsStats {
        uint32_t active_entities;
        uint32_t transform_updates;
        uint32_t culled_entities;
        std::chrono::microseconds total_system_time;
    };
    EcsStats get_stats() const { return stats_; }

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;
    VulkanGpuArenaManager& arena_manager_;

    // ECS compute pipelines
    VkPipeline transform_update_pipeline_;
    VkPipeline frustum_culling_pipeline_;
    std::unordered_map<std::string, VkPipeline> custom_system_pipelines_;

    // Component storage
    VulkanGpuArenaManager::ArenaAllocation entity_indices_buffer_;
    std::unordered_map<std::string, VulkanGpuArenaManager::ArenaAllocation> component_buffers_;

    uint32_t max_entities_;
    std::atomic<uint32_t> active_entity_count_;

    EcsStats stats_;

    void create_ecs_pipelines();
    void dispatch_system_compute(VkPipeline pipeline, uint32_t entity_count);
};

// Main GPU Compute System - Complete 100% GPU execution framework
class GpuComputeSystem {
public:
    GpuComputeSystem(GraphicsSystem& graphics_system);
    ~GpuComputeSystem();

    void initialize();
    void shutdown();

    // Subsystem access
    VulkanGpuArenaManager& get_arena_manager() { return *arena_manager_; }
    CpuArenaAllocator& get_cpu_arena() { return *cpu_arena_; }
    ShaderCompiler& get_shader_compiler() { return *shader_compiler_; }
    ComputePipelineManager& get_pipeline_manager() { return *pipeline_manager_; }
    GpuPhysicsSystem& get_physics_system() { return *physics_system_; }
    GpuParticleSystem& get_particle_system() { return *particle_system_; }
    EcsComputeIntegration& get_ecs_integration() { return *ecs_integration_; }

    // Frame execution - all GPU autonomous
    void begin_frame();
    void execute_compute_frame(float delta_time);
    void end_frame();

    // Synchronization with graphics pipeline
    void wait_for_compute_completion();
    VkSemaphore get_compute_completion_semaphore() const { return compute_completion_semaphore_; }

    // Performance monitoring
    struct ComputeSystemStats {
        std::chrono::microseconds total_frame_time;
        std::chrono::microseconds physics_time;
        std::chrono::microseconds particles_time;
        std::chrono::microseconds ecs_time;
        uint64_t total_dispatches;
        float gpu_utilization;
    };
    ComputeSystemStats get_stats() const { return stats_; }

    // GPU autonomous execution - zero CPU involvement after setup
    void enable_autonomous_execution(bool enable = true);
    bool is_autonomous_execution_enabled() const { return autonomous_execution_enabled_; }

private:
    GraphicsSystem& graphics_system_;

    // Vulkan compute context
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;
    VkQueue compute_queue_;
    uint32_t compute_queue_family_;

    // Subsystems - all GPU-accelerated
    std::unique_ptr<VulkanGpuArenaManager> arena_manager_;
    std::unique_ptr<CpuArenaAllocator> cpu_arena_;
    std::unique_ptr<ShaderCompiler> shader_compiler_;
    std::unique_ptr<ComputePipelineManager> pipeline_manager_;
    std::unique_ptr<GpuPhysicsSystem> physics_system_;
    std::unique_ptr<GpuParticleSystem> particle_system_;
    std::unique_ptr<EcsComputeIntegration> ecs_integration_;

    // Multi-frame compute synchronization
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> compute_command_buffers_;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> compute_fences_;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> compute_semaphores_;
    VkSemaphore compute_completion_semaphore_;
    VkCommandPool compute_command_pool_;
    uint32_t current_frame_;

    // Autonomous execution
    std::atomic<bool> autonomous_execution_enabled_;
    std::thread autonomous_thread_;
    std::atomic<bool> autonomous_thread_running_;
    std::condition_variable autonomous_cv_;
    std::mutex autonomous_mutex_;

    ComputeSystemStats stats_;

    void initialize_vulkan_compute();
    void create_compute_synchronization();
    void autonomous_execution_loop();
    void record_compute_commands(VkCommandBuffer cmd, float delta_time);
    void update_performance_stats();
};

} // namespace lore::graphics