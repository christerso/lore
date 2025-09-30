#pragma once

#include "lore/ecs/components/atmospheric_component.hpp"
#include "lore/graphics/gpu_compute.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>

namespace lore::ecs {

/**
 * @brief Atmospheric scattering and lighting system
 *
 * Manages atmospheric effects including:
 * - Real-time sun/moon color calculation based on atmospheric scattering
 * - Precomputed scattering LUTs (transmittance, single scattering)
 * - Volumetric god rays
 * - Distance fog and aerial perspective
 * - Weather effects (clouds, rain, snow)
 * - Integration with lighting and rendering systems
 *
 * Physics:
 * - Rayleigh scattering (wavelength-dependent, creates blue sky)
 * - Mie scattering (aerosols, pollution, creates haze)
 * - Ozone absorption
 * - Realistic sun position calculations
 *
 * Performance:
 * - LUT generation: ~10ms (only when atmosphere changes)
 * - Per-frame updates: ~0.5ms (sun color calculation)
 * - Atmospheric apply: ~1-2ms (integrated with post-processing)
 *
 * Usage:
 * @code
 * // Create atmosphere entity
 * auto atmosphere_entity = world.create_entity();
 * auto& atmos = world.add_component<AtmosphericComponent>(
 *     atmosphere_entity,
 *     AtmosphericComponent::create_earth_clear_day()
 * );
 *
 * // System updates automatically
 * atmospheric_system.update(world, delta_time);
 *
 * // Query current sun color
 * math::Vec3 sun_color = atmos.current_sun_color_rgb;
 * @endcode
 */
class AtmosphericSystem {
public:
    /**
     * @brief Configuration for atmospheric system
     */
    struct Config {
        // LUT resolutions
        uint32_t transmittance_lut_width = 256;
        uint32_t transmittance_lut_height = 64;
        uint32_t scattering_lut_width = 200;
        uint32_t scattering_lut_height = 128;
        uint32_t scattering_lut_depth = 32;

        // Integration quality
        uint32_t transmittance_samples = 40;
        uint32_t scattering_samples = 40;

        // Update frequencies
        float sun_color_update_rate_hz = 30.0f;  // Update sun color 30 times/sec
        float lut_regeneration_delay_s = 0.5f;   // Wait 0.5s after parameter change before regenerating LUTs

        // Performance
        bool enable_multi_scattering = false;  // 20% slower, more accurate
        bool enable_lut_caching = true;        // Cache LUTs across frames
    };

    AtmosphericSystem() = default;
    ~AtmosphericSystem() = default;

    /**
     * @brief Initialize atmospheric system with GPU resources
     * @param gpu_context GPU compute context
     * @param config System configuration
     * @return true on success
     */
    bool initialize(graphics::GPUComputeContext& gpu_context, const Config& config = Config{});

    /**
     * @brief Shutdown and release GPU resources
     */
    void shutdown();

    /**
     * @brief Update atmospheric state
     * Calculates current sun/moon colors, updates LUTs if needed
     * @param world ECS world
     * @param delta_time_s Time since last update (seconds)
     */
    void update(World& world, float delta_time_s);

    /**
     * @brief Apply atmospheric effects to rendered scene
     * Call after scene rendering, before tone mapping
     * @param cmd Vulkan command buffer
     * @param scene_color_image HDR scene color
     * @param depth_image Depth buffer
     * @param output_image Output image
     * @param world ECS world
     * @param camera_position Camera world position
     * @param view_proj_matrix Camera view-projection matrix
     */
    void apply_atmospheric_effects(
        VkCommandBuffer cmd,
        VkImage scene_color_image,
        VkImage depth_image,
        VkImage output_image,
        const World& world,
        const math::Vec3& camera_position,
        const float* view_proj_matrix  // 16 floats
    );

    /**
     * @brief Force regeneration of scattering LUTs
     * Useful when atmosphere parameters change
     * @param world ECS world
     */
    void regenerate_luts(World& world);

    /**
     * @brief Get current atmospheric component (primary atmosphere in world)
     * @param world ECS world
     * @return Pointer to atmospheric component, or nullptr if none
     */
    static const AtmosphericComponent* get_atmosphere(const World& world);

    /**
     * @brief Get mutable atmospheric component
     * @param world ECS world
     * @return Pointer to atmospheric component, or nullptr if none
     */
    static AtmosphericComponent* get_atmosphere_mut(World& world);

    /**
     * @brief Calculate sun color for given atmospheric conditions
     * Static utility function, doesn't require system initialization
     * @param atmos Atmospheric component
     * @param view_direction Direction from observer to sun
     * @return Sun color (RGB, linear)
     */
    static math::Vec3 calculate_sun_color(
        const AtmosphericComponent& atmos,
        const math::Vec3& view_direction
    );

    /**
     * @brief Calculate sky color for given direction
     * @param atmos Atmospheric component
     * @param view_direction Sky view direction
     * @return Sky color (RGB, linear)
     */
    static math::Vec3 calculate_sky_color(
        const AtmosphericComponent& atmos,
        const math::Vec3& view_direction
    );

private:
    /**
     * @brief GPU resources for atmospheric rendering
     */
    struct AtmosphericGPUResources {
        // Precomputed LUTs
        VkImage transmittance_lut = VK_NULL_HANDLE;
        VkImageView transmittance_lut_view = VK_NULL_HANDLE;
        VkDeviceMemory transmittance_lut_memory = VK_NULL_HANDLE;

        VkImage scattering_lut = VK_NULL_HANDLE;
        VkImageView scattering_lut_view = VK_NULL_HANDLE;
        VkDeviceMemory scattering_lut_memory = VK_NULL_HANDLE;

        // Compute pipelines
        VkPipeline transmittance_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout transmittance_pipeline_layout = VK_NULL_HANDLE;

        VkPipeline scattering_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout scattering_pipeline_layout = VK_NULL_HANDLE;

        VkPipeline apply_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout apply_pipeline_layout = VK_NULL_HANDLE;

        // Descriptor sets
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout transmittance_desc_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout scattering_desc_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout apply_desc_layout = VK_NULL_HANDLE;

        VkDescriptorSet transmittance_desc_set = VK_NULL_HANDLE;
        VkDescriptorSet scattering_desc_set = VK_NULL_HANDLE;
        VkDescriptorSet apply_desc_set = VK_NULL_HANDLE;

        // Sampler
        VkSampler lut_sampler = VK_NULL_HANDLE;
    };

    /**
     * @brief Update sun/moon colors based on current atmospheric state
     */
    void update_celestial_colors(AtmosphericComponent& atmos) const;

    /**
     * @brief Generate transmittance LUT
     */
    void generate_transmittance_lut(
        VkCommandBuffer cmd,
        const AtmosphericComponent& atmos
    );

    /**
     * @brief Generate scattering LUT
     */
    void generate_scattering_lut(
        VkCommandBuffer cmd,
        const AtmosphericComponent& atmos
    );

    /**
     * @brief Create GPU resources (images, pipelines, etc.)
     */
    bool create_gpu_resources();

    /**
     * @brief Destroy GPU resources
     */
    void destroy_gpu_resources();

    Config config_;
    graphics::GPUComputeContext* gpu_context_ = nullptr;
    AtmosphericGPUResources gpu_resources_;

    // Timing
    float sun_color_update_accumulator_ = 0.0f;
    float lut_regeneration_timer_ = 0.0f;

    // State
    bool initialized_ = false;
    bool luts_need_update_ = true;
};

} // namespace lore::ecs