#pragma once

#include <lore/math/math.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>

namespace lore::graphics {

// Forward declarations
class GraphicsSystem;

/**
 * @brief G-Buffer configuration for deferred rendering
 *
 * Layout:
 * - Attachment 0: Albedo (RGB) + Metallic (A) - RGBA8
 * - Attachment 1: Normal (RGB) + Roughness (A) - RGBA16F
 * - Attachment 2: Position (RGB) + AO (A) - RGBA16F
 * - Attachment 3: Emissive (RGB) - RGBA16F
 * - Attachment 4: Depth/Stencil - D32_SFLOAT_S8_UINT
 */
struct GBufferAttachment {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
};

struct GBuffer {
    // Color attachments
    GBufferAttachment albedo_metallic;      // RGBA8 - RGB: albedo, A: metallic
    GBufferAttachment normal_roughness;     // RGBA16F - RGB: world-space normal, A: roughness
    GBufferAttachment position_ao;          // RGBA16F - RGB: world position, A: ambient occlusion
    GBufferAttachment emissive;             // RGBA16F - RGB: emissive color

    // Depth attachment
    GBufferAttachment depth;                // D32_SFLOAT_S8_UINT

    // Framebuffer and render pass
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;

    // Descriptor set for sampling G-Buffer in lighting pass
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkExtent2D extent{};

    bool is_valid() const {
        return albedo_metallic.image != VK_NULL_HANDLE &&
               normal_roughness.image != VK_NULL_HANDLE &&
               position_ao.image != VK_NULL_HANDLE &&
               emissive.image != VK_NULL_HANDLE &&
               depth.image != VK_NULL_HANDLE;
    }
};

/**
 * @brief Light types supported by the deferred renderer
 */
enum class LightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

/**
 * @brief Shadow quality settings
 */
enum class ShadowQuality : uint32_t {
    Off = 0,        // No shadows
    Hard = 1,       // Hard shadows (1 sample)
    PCF_3x3 = 2,    // 9 samples (3×3 PCF kernel)
    PCF_5x5 = 3,    // 25 samples (5×5 PCF kernel) - default
    PCF_7x7 = 4     // 49 samples (7×7 PCF kernel) - expensive
};

/**
 * @brief Light data structure for GPU
 */
struct alignas(16) Light {
    math::Vec3 position;          // World position for point/spot lights
    float range;                  // Attenuation range

    math::Vec3 direction;         // Direction for directional/spot lights
    float intensity;              // Light intensity

    math::Vec3 color;             // RGB color
    float inner_cone_angle;       // Inner cone angle for spot lights (radians)

    float outer_cone_angle;       // Outer cone angle for spot lights (radians)
    uint32_t type;                // LightType
    uint32_t casts_shadows;       // 0 or 1
    uint32_t padding;             // Alignment padding
};

static_assert(sizeof(Light) == 80, "Light struct must be 80 bytes for GPU alignment");

/**
 * @brief PBR material properties
 */
struct alignas(16) PBRMaterial {
    math::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};

    math::Vec3 emissive{0.0f, 0.0f, 0.0f};
    float roughness{0.5f};

    float ao{1.0f};                // Ambient occlusion
    float alpha{1.0f};             // Transparency
    uint32_t albedo_texture{0};    // Texture index (0 = none)
    uint32_t normal_texture{0};    // Normal map texture index

    uint32_t metallic_roughness_texture{0};  // Combined metallic/roughness texture
    uint32_t emissive_texture{0};            // Emissive texture
    uint32_t ao_texture{0};                  // AO texture
    uint32_t padding;
};

static_assert(sizeof(PBRMaterial) == 80, "PBRMaterial struct must be 80 bytes for GPU alignment");

/**
 * @brief Shadow configuration
 *
 * INI Configuration: data/config/shadows.ini
 * Example:
 * ```ini
 * [Cascades]
 * count = 4                      # Number of cascades (1-4)
 * resolution = 2048              # Resolution per cascade (512-4096)
 * split_lambda = 0.5             # Cascade split lambda (0=uniform, 1=logarithmic)
 *
 * [Quality]
 * quality = pcf_5x5              # off, hard, pcf_3x3, pcf_5x5, pcf_7x7
 * pcf_radius = 1.5               # PCF sampling radius in texels (0.5-5.0)
 * soft_shadow_scale = 1.0        # Softness multiplier (0.5-2.0)
 *
 * [Bias]
 * depth_bias = 0.0005            # Depth bias to prevent shadow acne
 * normal_bias = 0.001            # Normal-based bias
 * slope_bias = 0.0001            # Slope-scaled depth bias
 *
 * [Fade]
 * fade_start = 0.8               # Start fading shadows (0-1, fraction of max distance)
 * fade_end = 1.0                 # Fully fade shadows (0-1)
 * max_distance = 100.0           # Maximum shadow distance (meters)
 *
 * [Performance]
 * enable_early_exit = true       # Skip fully lit/shadowed pixels
 * use_poisson_disk = true        # Use Poisson disk for better PCF distribution
 * ```
 */
struct ShadowConfig {
    // Cascaded shadow maps for directional lights
    uint32_t cascade_count = 4;           // Number of cascades (1-4)
    uint32_t cascade_resolution = 2048;   // Resolution per cascade
    float cascade_split_lambda = 0.5f;    // 0=uniform, 1=logarithmic

    // Shadow quality
    ShadowQuality quality = ShadowQuality::PCF_5x5;

    // Shadow parameters
    float shadow_bias = 0.0005f;          // Depth bias to prevent shadow acne
    float shadow_normal_bias = 0.001f;    // Normal-based bias
    float shadow_slope_bias = 0.0001f;    // Slope-scaled bias
    float shadow_fade_start = 0.8f;       // Start fading shadows (fraction of max distance)
    float shadow_fade_end = 1.0f;         // Fully fade shadows
    float max_shadow_distance = 100.0f;   // Maximum shadow distance (meters)

    // Cascade split distances (auto-calculated if all 0)
    float cascade_splits[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // PCF parameters
    float pcf_radius = 1.5f;              // PCF sampling radius (texels)
    float soft_shadow_scale = 1.0f;       // Softness multiplier (0.5-2.0)
    bool use_poisson_disk = true;         // Better PCF distribution

    // Performance
    bool enable_early_exit = true;        // Skip fully lit/shadowed pixels
    bool enable_backface_culling = true;  // Cull backfaces in shadow pass

    /**
     * @brief Load from INI file
     */
    static ShadowConfig load_from_ini(const std::string& filepath);

    /**
     * @brief Save to INI file
     */
    void save_to_ini(const std::string& filepath) const;

    /**
     * @brief Quality presets
     */
    static ShadowConfig create_low_quality();
    static ShadowConfig create_medium_quality();
    static ShadowConfig create_high_quality();
    static ShadowConfig create_ultra_quality();
    static ShadowConfig create_mirrors_edge_crisp();  // Sharp, contrasty shadows
};

/**
 * @brief Cascade shadow map data
 */
struct ShadowCascade {
    VkImage shadow_map = VK_NULL_HANDLE;
    VkImageView shadow_map_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    // Light-space matrix for this cascade
    math::Mat4 view_proj_matrix;

    // Split distances (near and far)
    float split_near = 0.0f;
    float split_far = 0.0f;
};

/**
 * @brief All shadow maps for a directional light
 */
struct DirectionalShadowMaps {
    std::array<ShadowCascade, 4> cascades;
    uint32_t active_cascades = 0;

    VkRenderPass shadow_render_pass = VK_NULL_HANDLE;
    VkPipeline shadow_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout shadow_pipeline_layout = VK_NULL_HANDLE;

    VkSampler shadow_sampler = VK_NULL_HANDLE;  // PCF-enabled sampler
    VkDescriptorSet shadow_descriptor_set = VK_NULL_HANDLE;
};

/**
 * @brief Deferred rendering pipeline
 *
 * Implements a full deferred rendering system with:
 * - G-Buffer generation (geometry pass)
 * - PBR lighting pass with multiple light types
 * - HDR rendering with tone mapping
 * - Support for shadows, SSAO, and post-processing
 */
class DeferredRenderer {
public:
    DeferredRenderer(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator);
    ~DeferredRenderer();

    // Initialization
    void initialize(VkExtent2D extent, VkFormat swapchain_format);
    void shutdown();

    // Resize handling
    void resize(VkExtent2D new_extent);

    // Rendering
    void begin_geometry_pass(VkCommandBuffer cmd);
    void end_geometry_pass(VkCommandBuffer cmd);

    void begin_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_index);
    void end_shadow_pass(VkCommandBuffer cmd);

    void begin_lighting_pass(VkCommandBuffer cmd, VkImage target_image, VkImageView target_view);
    void end_lighting_pass(VkCommandBuffer cmd);

    // Light management
    uint32_t add_light(const Light& light);
    void update_light(uint32_t light_id, const Light& light);
    void remove_light(uint32_t light_id);
    void clear_lights();

    // Shadow management
    void set_shadow_config(const ShadowConfig& config);
    const ShadowConfig& get_shadow_config() const { return shadow_config_; }
    void update_shadow_cascades(const math::Vec3& camera_pos, const math::Vec3& camera_forward,
                                const math::Vec3& light_dir, float camera_near, float camera_far);
    const DirectionalShadowMaps& get_shadow_maps() const { return shadow_maps_; }

    // Material management
    uint32_t register_material(const PBRMaterial& material);
    const PBRMaterial& get_material(uint32_t material_id) const;

    // Getters
    const GBuffer& get_gbuffer() const { return gbuffer_; }
    VkRenderPass get_geometry_render_pass() const { return gbuffer_.render_pass; }
    VkPipeline get_geometry_pipeline() const { return geometry_pipeline_; }
    VkPipelineLayout get_geometry_pipeline_layout() const { return geometry_pipeline_layout_; }

    // Statistics
    struct Stats {
        uint32_t lights_count = 0;
        uint32_t materials_count = 0;
        float geometry_pass_ms = 0.0f;
        float lighting_pass_ms = 0.0f;
    };
    Stats get_stats() const { return stats_; }

private:
    // G-Buffer creation
    void create_gbuffer(VkExtent2D extent);
    void destroy_gbuffer();

    GBufferAttachment create_attachment(VkExtent2D extent, VkFormat format,
                                       VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void destroy_attachment(GBufferAttachment& attachment);

    // Render passes
    void create_geometry_render_pass();
    void create_lighting_render_pass(VkFormat swapchain_format);

    // Pipelines
    void create_geometry_pipeline();
    void create_lighting_pipeline();

    // Descriptors
    void create_descriptor_sets();
    void update_gbuffer_descriptor_set();
    void update_lights_descriptor_set();

    // Buffers
    void create_light_buffer();
    void update_light_buffer();

    // Vulkan handles
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;

    // G-Buffer
    GBuffer gbuffer_;

    // Geometry pass
    VkRenderPass geometry_render_pass_ = VK_NULL_HANDLE;
    VkPipeline geometry_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout geometry_pipeline_layout_ = VK_NULL_HANDLE;

    // Lighting pass
    VkRenderPass lighting_render_pass_ = VK_NULL_HANDLE;
    VkPipeline lighting_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout lighting_pipeline_layout_ = VK_NULL_HANDLE;

    // Framebuffer cache for lighting pass (keyed by VkImageView)
    std::unordered_map<VkImageView, VkFramebuffer> lighting_framebuffer_cache_;
    VkImageView last_lighting_target_ = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout gbuffer_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout lights_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet gbuffer_descriptor_set_ = VK_NULL_HANDLE;
    VkDescriptorSet lights_descriptor_set_ = VK_NULL_HANDLE;
    VkSampler gbuffer_sampler_ = VK_NULL_HANDLE;

    // Light data
    std::vector<Light> lights_;
    VkBuffer light_buffer_ = VK_NULL_HANDLE;
    VmaAllocation light_buffer_allocation_ = VK_NULL_HANDLE;
    bool lights_dirty_ = false;

    // Material data
    std::vector<PBRMaterial> materials_;

    // Shadow configuration and resources
    ShadowConfig shadow_config_;
    DirectionalShadowMaps shadow_maps_;

    // Shadow pipeline creation
    void create_shadow_pipeline();
    void create_shadow_maps();
    void destroy_shadow_maps();
    void create_shadow_sampler();
    void update_shadow_descriptor_set();

    // Statistics
    Stats stats_;

    // Initialization state
    bool initialized_ = false;
};

} // namespace lore::graphics