#pragma once

#include "lore/ecs/advanced_ecs.hpp"
#include "lore/ecs/components/volumetric_fire_component.hpp"
#include "lore/graphics/gpu_compute.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace lore::ecs {

/**
 * @brief System for GPU-based volumetric fire simulation
 *
 * Implements Navier-Stokes fluid dynamics on GPU compute shaders for
 * realistic fire simulation with density fields, buoyancy, and turbulence.
 *
 * GPU Pipeline (per fire entity):
 * 1. Advect: Move velocity/density/temperature through velocity field
 * 2. InjectSource: Add fuel, heat, velocity at source point
 * 3. AddForce: Apply buoyancy, gravity, wind, vorticity confinement
 * 4. Divergence: Calculate ∇·u for pressure solve
 * 5. Jacobi: Iteratively solve for pressure (40-50 iterations)
 * 6. Project: Subtract ∇p from velocity (make divergence-free)
 *
 * Integration:
 * - ThermalSystem: Fire generates heat
 * - CombustionComponent: Provides fuel rate, temperature
 * - VolumetricSmokeSystem: Smoke generated from fire
 *
 * Performance:
 * - All simulation on GPU (zero CPU cost per entity)
 * - LOD support (reduce resolution for distant fires)
 * - Async compute (overlap with rendering)
 * - Shared compute resources (one pipeline for all fires)
 *
 * Example usage:
 * @code
 * VolumetricFireSystem fire_system;
 * fire_system.initialize(vulkan_context, {
 *     .enable_async_compute = true,
 *     .max_jacobi_iterations = 40,
 * });
 *
 * // Each frame
 * fire_system.update(world, delta_time);
 *
 * // Render fire volumes
 * fire_system.render(command_buffer, camera);
 * @endcode
 */
class VolumetricFireSystem {
public:
    /**
     * @brief Configuration for volumetric fire system
     */
    struct Config {
        // === Simulation ===
        bool enable_async_compute = true;      ///< Use async compute queue
        uint32_t max_jacobi_iterations = 40;   ///< Pressure solve iterations
        float time_step_s = 0.016f;            ///< Fixed timestep (1/60)
        bool enable_adaptive_timestep = true;  ///< Dynamic dt for stability

        // === Quality ===
        bool enable_maccormack_advection = true; ///< Higher-order advection
        float maccormack_strength = 0.8f;      ///< Correction strength (0-1)
        bool enable_vorticity_confinement = true; ///< Add turbulence
        float vorticity_strength_mult = 1.0f;  ///< Global vorticity multiplier

        // === Performance ===
        bool enable_lod = true;                ///< Distance-based LOD
        float lod_distance_full_m = 20.0f;     ///< Full res distance
        float lod_distance_half_m = 50.0f;     ///< Half res distance
        float lod_distance_quarter_m = 100.0f; ///< Quarter res distance
        uint32_t max_active_fires = 20;        ///< Maximum simultaneous fires

        // === Memory ===
        VkDeviceSize max_texture_memory_mb = 512; ///< Max VRAM for fire textures
        bool allow_texture_compression = false;   ///< R16F instead of R32F

        // === Debug ===
        bool visualize_velocity_field = false;    ///< Debug arrows
        bool visualize_pressure_field = false;    ///< Debug colors
        bool log_gpu_timings = false;             ///< Print shader times
    };

    VolumetricFireSystem();
    ~VolumetricFireSystem();

    /**
     * @brief Initialize system with Vulkan context
     *
     * @param gpu_context GPU compute context
     * @param config System configuration
     * @return true on success
     */
    bool initialize(
        graphics::GPUComputeContext& gpu_context,
        const Config& config
    );

    /**
     * @brief Shutdown and release GPU resources
     */
    void shutdown();

    /**
     * @brief Update all volumetric fires
     *
     * @param world ECS world
     * @param delta_time_s Time step (seconds)
     */
    void update(World& world, float delta_time_s);

    /**
     * @brief Render volumetric fires (raymarching)
     *
     * @param command_buffer Vulkan command buffer
     * @param view_matrix Camera view matrix
     * @param proj_matrix Camera projection matrix
     */
    void render(
        VkCommandBuffer command_buffer,
        const float* view_matrix,
        const float* proj_matrix
    );

    /**
     * @brief Get configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const Config& config) { config_ = config; }

    /**
     * @brief Get GPU statistics
     */
    struct Stats {
        uint32_t active_fires = 0;
        uint32_t total_cells_simulated = 0;
        float total_gpu_time_ms = 0.0f;
        float advection_time_ms = 0.0f;
        float pressure_solve_time_ms = 0.0f;
        float projection_time_ms = 0.0f;
        uint64_t vram_used_mb = 0;
    };

    const Stats& get_stats() const { return stats_; }

private:
    /**
     * @brief GPU resources for a single fire entity
     */
    struct FireGPUResources {
        Entity entity;
        VolumetricFireComponent* component;

        // 3D textures (double-buffered for ping-pong)
        VkImage velocity_texture[2];
        VkImageView velocity_view[2];
        VkDeviceMemory velocity_memory[2];

        VkImage density_texture[2];
        VkImageView density_view[2];
        VkDeviceMemory density_memory[2];

        VkImage temperature_texture[2];
        VkImageView temperature_view[2];
        VkDeviceMemory temperature_memory[2];

        VkImage pressure_texture[2];
        VkImageView pressure_view[2];
        VkDeviceMemory pressure_memory[2];

        VkImage divergence_texture;
        VkImageView divergence_view;
        VkDeviceMemory divergence_memory;

        // Descriptor sets for compute shaders
        VkDescriptorSet descriptor_sets[6];  // One per shader pass

        // Current read/write indices (for double buffering)
        int read_index = 0;
        int write_index = 1;

        // LOD level
        float current_lod_scale = 1.0f;
    };

    Config config_;
    Stats stats_;

    graphics::GPUComputeContext* gpu_context_ = nullptr;

    // Compute pipelines (shared across all fires)
    VkPipeline pipeline_advect_;
    VkPipeline pipeline_inject_source_;
    VkPipeline pipeline_add_force_;
    VkPipeline pipeline_divergence_;
    VkPipeline pipeline_jacobi_;
    VkPipeline pipeline_project_;

    VkPipelineLayout pipeline_layout_;
    VkDescriptorSetLayout descriptor_set_layout_;
    VkDescriptorPool descriptor_pool_;

    // Per-fire GPU resources
    std::vector<FireGPUResources> fire_resources_;

    float accumulated_time_ = 0.0f;

    /**
     * @brief Create GPU resources for fire entity
     */
    bool create_fire_resources(
        Entity entity,
        VolumetricFireComponent* component,
        FireGPUResources& resources
    );

    /**
     * @brief Destroy GPU resources
     */
    void destroy_fire_resources(FireGPUResources& resources);

    /**
     * @brief Create compute pipelines
     */
    bool create_compute_pipelines();

    /**
     * @brief Dispatch compute shaders for single fire
     */
    void simulate_fire(
        VkCommandBuffer cmd,
        FireGPUResources& resources,
        float delta_time
    );

    /**
     * @brief Calculate LOD scale based on distance
     */
    float calculate_lod_scale(float distance_m) const;

    /**
     * @brief Create 3D texture with memory
     */
    bool create_3d_texture(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImage& image,
        VkImageView& view,
        VkDeviceMemory& memory
    );
};

} // namespace lore::ecs