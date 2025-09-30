#pragma once

#include <lore/core/types.hpp>
#include <lore/graphics/vulkan_context.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace lore {

/**
 * Environment rendering mode
 */
enum class EnvironmentMode : uint32_t {
    Atmospheric = 0,  // Pure procedural atmospheric scattering
    HDRI = 1,         // Pure image-based lighting from HDRI
    Hybrid = 2        // HDRI skybox + atmospheric fog overlay
};

/**
 * Quality settings for IBL pre-computation
 */
struct HDRIQualityConfig {
    uint32_t environment_resolution = 1024;   // Cubemap resolution (512, 1024, 2048)
    uint32_t irradiance_resolution = 32;      // Irradiance map resolution (16, 32, 64)
    uint32_t prefiltered_mip_levels = 5;      // Roughness mip levels (5-7)
    uint32_t brdf_lut_resolution = 512;       // BRDF lookup texture size
    uint32_t irradiance_sample_count = 1024;  // Samples per pixel for irradiance
    uint32_t prefilter_sample_count = 1024;   // Samples per pixel for pre-filter
    uint32_t brdf_sample_count = 512;         // Samples per pixel for BRDF LUT

    // Preset configurations
    static HDRIQualityConfig create_ultra();
    static HDRIQualityConfig create_high();
    static HDRIQualityConfig create_medium();
    static HDRIQualityConfig create_low();
};

/**
 * HDRI environment texture data
 */
struct HDRITextures {
    // Source equirectangular texture (4K×2K RGBA16F)
    VkImage equirect_image = VK_NULL_HANDLE;
    VkImageView equirect_view = VK_NULL_HANDLE;
    VmaAllocation equirect_allocation = VK_NULL_HANDLE;
    uint32_t equirect_width = 0;
    uint32_t equirect_height = 0;

    // Environment cubemap (512×512×6 RGBA16F)
    VkImage environment_image = VK_NULL_HANDLE;
    VkImageView environment_view = VK_NULL_HANDLE;
    VmaAllocation environment_allocation = VK_NULL_HANDLE;

    // Irradiance cubemap for diffuse IBL (32×32×6 RGBA16F)
    VkImage irradiance_image = VK_NULL_HANDLE;
    VkImageView irradiance_view = VK_NULL_HANDLE;
    VmaAllocation irradiance_allocation = VK_NULL_HANDLE;

    // Pre-filtered environment map for specular IBL (512×512×6 RGBA16F, 5 mips)
    VkImage prefiltered_image = VK_NULL_HANDLE;
    VkImageView prefiltered_view = VK_NULL_HANDLE;
    VmaAllocation prefiltered_allocation = VK_NULL_HANDLE;

    // BRDF integration lookup table (512×512 RG16F)
    VkImage brdf_lut_image = VK_NULL_HANDLE;
    VkImageView brdf_lut_view = VK_NULL_HANDLE;
    VmaAllocation brdf_lut_allocation = VK_NULL_HANDLE;

    // Samplers
    VkSampler environment_sampler = VK_NULL_HANDLE;
    VkSampler irradiance_sampler = VK_NULL_HANDLE;
    VkSampler prefiltered_sampler = VK_NULL_HANDLE;
    VkSampler brdf_lut_sampler = VK_NULL_HANDLE;
};

/**
 * HDRI environment parameters
 */
struct HDRIEnvironmentParams {
    float intensity = 1.0f;              // HDRI brightness multiplier
    float rotation_y = 0.0f;             // Y-axis rotation in radians
    float saturation = 1.0f;             // Color saturation adjustment
    float contrast = 1.0f;               // Contrast adjustment
    glm::vec3 tint = glm::vec3(1.0f);    // Color tint (1,1,1 = no tint)

    // Hybrid mode parameters
    float atmospheric_blend = 0.3f;      // 0.0-1.0, blend factor for atmospheric overlay
    bool apply_fog = true;               // Apply atmospheric fog in hybrid mode
    bool apply_aerial_perspective = true; // Apply aerial perspective in hybrid mode
};

/**
 * Environment uniform buffer data (matches shader layout)
 */
struct EnvironmentUBO {
    alignas(4) uint32_t mode;            // 0 = atmospheric, 1 = HDRI, 2 = hybrid
    alignas(4) float hdri_intensity;
    alignas(4) float hdri_rotation_y;
    alignas(4) float atmospheric_blend;
    alignas(16) glm::vec3 hdri_tint;
    alignas(4) float hdri_saturation;
    alignas(4) float hdri_contrast;
    alignas(4) uint32_t apply_fog;
    alignas(4) uint32_t apply_aerial_perspective;
    alignas(4) uint32_t padding[1];
};

/**
 * HDRI environment manager
 * Handles loading HDR images, converting to cubemaps, and generating IBL textures
 */
class HDRIEnvironment {
public:
    HDRIEnvironment() = default;
    ~HDRIEnvironment();

    HDRIEnvironment(const HDRIEnvironment&) = delete;
    HDRIEnvironment& operator=(const HDRIEnvironment&) = delete;
    HDRIEnvironment(HDRIEnvironment&& other) noexcept;
    HDRIEnvironment& operator=(HDRIEnvironment&& other) noexcept;

    /**
     * Load HDRI from file (.exr, .hdr supported)
     * Allocates equirectangular texture on GPU
     */
    static HDRIEnvironment load_from_file(
        VulkanContext& context,
        const std::string& file_path,
        const HDRIQualityConfig& quality = HDRIQualityConfig::create_high()
    );

    /**
     * Generate all IBL textures (one-time pre-computation)
     *
     * Pipeline:
     * 1. Convert equirectangular → environment cubemap
     * 2. Convolve environment → irradiance cubemap (diffuse IBL)
     * 3. Pre-filter environment → roughness mip chain (specular IBL)
     * 4. Integrate BRDF → lookup table (Fresnel × Geometry term)
     *
     * @param context Vulkan context
     * @param command_buffer Command buffer for compute dispatch (must be in recording state)
     *
     * Performance: ~262ms for high quality, ~150ms for medium quality
     */
    void generate_ibl_maps(VulkanContext& context, VkCommandBuffer command_buffer);

    /**
     * Check if IBL maps have been generated
     */
    bool has_ibl_maps() const { return m_ibl_generated; }

    /**
     * Get environment parameters (modifiable)
     */
    HDRIEnvironmentParams& params() { return m_params; }
    const HDRIEnvironmentParams& params() const { return m_params; }

    /**
     * Get quality configuration
     */
    const HDRIQualityConfig& quality() const { return m_quality; }

    /**
     * Get IBL textures for descriptor set binding
     */
    const HDRITextures& textures() const { return m_textures; }

    /**
     * Get environment UBO data for shader upload
     */
    EnvironmentUBO get_environment_ubo(EnvironmentMode mode) const;

    /**
     * Calculate average luminance of HDRI (for auto-exposure)
     */
    float calculate_average_luminance() const;

    /**
     * Get source file path
     */
    const std::string& file_path() const { return m_file_path; }

    /**
     * Destroy all Vulkan resources
     */
    void destroy(VulkanContext& context);

private:
    /**
     * Load HDR image from file (EXR or Radiance HDR format)
     * Returns pixel data in RGBA32F format
     */
    static std::vector<float> load_hdr_image(
        const std::string& file_path,
        uint32_t& width,
        uint32_t& height
    );

    /**
     * Create equirectangular texture on GPU
     */
    void create_equirectangular_texture(
        VulkanContext& context,
        const std::vector<float>& pixel_data,
        uint32_t width,
        uint32_t height
    );

    /**
     * Create output textures (environment, irradiance, prefiltered, BRDF LUT)
     */
    void create_output_textures(VulkanContext& context);

    /**
     * Convert equirectangular map to cubemap using compute shader
     */
    void convert_to_cubemap(VulkanContext& context, VkCommandBuffer cmd);

    /**
     * Generate irradiance map (diffuse IBL) using compute shader
     */
    void generate_irradiance_map(VulkanContext& context, VkCommandBuffer cmd);

    /**
     * Generate pre-filtered environment map (specular IBL) using compute shader
     */
    void generate_prefiltered_map(VulkanContext& context, VkCommandBuffer cmd);

    /**
     * Generate BRDF integration LUT using compute shader
     */
    void generate_brdf_lut(VulkanContext& context, VkCommandBuffer cmd);

    /**
     * Create samplers for IBL textures
     */
    void create_samplers(VulkanContext& context);

    /**
     * Create compute pipelines for IBL generation
     */
    void create_compute_pipelines(VulkanContext& context);

    /**
     * Destroy compute pipelines
     */
    void destroy_compute_pipelines(VulkanContext& context);

private:
    std::string m_file_path;
    HDRIQualityConfig m_quality;
    HDRIEnvironmentParams m_params;
    HDRITextures m_textures;
    bool m_ibl_generated = false;

    // Compute pipeline resources for IBL generation
    VkPipelineLayout m_equirect_to_cubemap_layout = VK_NULL_HANDLE;
    VkPipeline m_equirect_to_cubemap_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_irradiance_layout = VK_NULL_HANDLE;
    VkPipeline m_irradiance_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_prefilter_layout = VK_NULL_HANDLE;
    VkPipeline m_prefilter_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_brdf_lut_layout = VK_NULL_HANDLE;
    VkPipeline m_brdf_lut_pipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
};

} // namespace lore
