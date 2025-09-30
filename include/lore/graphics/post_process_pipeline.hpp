#pragma once

#include <lore/math/math.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <string>
#include <memory>

namespace lore::graphics {

// Forward declarations
class GraphicsSystem;

/**
 * @brief Tone mapping operator selection
 *
 * Tone mapping converts HDR (high dynamic range) to LDR (low dynamic range)
 * for display output. Different operators produce different aesthetics.
 *
 * References:
 * - ACES: Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
 * - Reinhard: Reinhard et al. 2002, "Photographic Tone Reproduction"
 * - Uncharted2: Hable 2010, "Uncharted 2: HDR Lighting"
 * - Hejl2015: Hejl 2015, "HDR color grading and display in Frostbite"
 */
enum class ToneMappingMode : uint32_t {
    Linear = 0,           // No tone mapping (clamps to [0,1])
    Reinhard = 1,         // Simple Reinhard (L/(1+L))
    ReinhardExtended = 2, // Extended Reinhard with white point
    ACES = 3,             // ACES filmic (Narkowicz approximation) - RECOMMENDED
    Uncharted2 = 4,       // Uncharted 2 filmic curve (Hable)
    Hejl2015 = 5          // Hejl-Dawson filmic approximation
};

/**
 * @brief Post-processing configuration
 *
 * Complete set of post-processing parameters for real-time tweaking.
 * All parameters are hot-reloadable and can be modified at runtime.
 *
 * INI Configuration: data/config/post_process.ini
 * Example:
 * ```ini
 * [ToneMapping]
 * mode = aces                    # linear, reinhard, reinhard_extended, aces, uncharted2, hejl2015
 * exposure_mode = manual         # manual, auto
 * exposure_ev = 0.0              # EV compensation (-5 to +5)
 * exposure_min = 0.03            # Minimum luminance (auto mode)
 * exposure_max = 8.0             # Maximum luminance (auto mode)
 * auto_exposure_speed = 3.0      # Adaptation speed (0.1 to 10.0)
 * white_point = 11.2             # White point for Reinhard Extended
 *
 * [ColorGrading]
 * temperature = 0.0              # Color temperature (-1 to +1, 0 = 6500K neutral)
 * tint = 0.0                     # Green/Magenta tint (-1 to +1)
 * saturation = 1.0               # Saturation multiplier (0 to 2)
 * vibrance = 0.0                 # Vibrance (affects less-saturated colors more)
 * contrast = 1.0                 # Contrast (0.5 to 2.0)
 * brightness = 0.0               # Brightness offset (-1 to +1)
 * gamma = 2.2                    # Gamma correction (1.0 to 3.0)
 *
 * [ColorBalance]
 * ; Lift affects shadows (dark tones)
 * lift_r = 0.0
 * lift_g = 0.0
 * lift_b = 0.0
 * ; Gamma affects midtones
 * gamma_r = 1.0
 * gamma_g = 1.0
 * gamma_b = 1.0
 * ; Gain affects highlights (bright tones)
 * gain_r = 1.0
 * gain_g = 1.0
 * gain_b = 1.0
 *
 * [Vignette]
 * intensity = 0.0                # Vignette strength (0 to 1)
 * smoothness = 0.5               # Falloff smoothness (0 to 1)
 * roundness = 1.0                # Aspect ratio (0 to 1)
 *
 * [Quality]
 * dithering = true               # Dithering to prevent banding
 * dither_strength = 0.004        # Dither amount (0.001 to 0.01)
 * ```
 */
struct PostProcessConfig {
    // ========================================================================
    // TONE MAPPING
    // ========================================================================

    ToneMappingMode tone_mapping_mode = ToneMappingMode::ACES;

    // Exposure control
    bool auto_exposure = false;         // Automatic exposure adaptation
    float exposure_ev = 0.0f;           // EV compensation (-5 to +5)
                                        // +1 EV = 2× brighter, -1 EV = 2× darker
    float exposure_min = 0.03f;         // Minimum luminance for auto exposure
    float exposure_max = 8.0f;          // Maximum luminance for auto exposure
    float auto_exposure_speed = 3.0f;   // Adaptation speed (0.1 to 10.0)

    // Tone mapping parameters
    float white_point = 11.2f;          // White point for Reinhard Extended

    // ========================================================================
    // COLOR GRADING
    // ========================================================================

    // Color temperature and tint
    // Temperature shifts along blue-orange axis (Planckian locus)
    // Tint shifts along green-magenta axis
    float temperature = 0.0f;           // -1 (2000K blue) to +1 (10000K orange), 0 = 6500K neutral
    float tint = 0.0f;                  // -1 (green) to +1 (magenta), 0 = neutral

    // Saturation and vibrance
    float saturation = 1.0f;            // Saturation multiplier (0 to 2)
                                        // 0 = grayscale, 1 = neutral, 2 = oversaturated
    float vibrance = 0.0f;              // Vibrance (-1 to +1)
                                        // Affects less-saturated colors more than saturated ones

    // Contrast and brightness
    float contrast = 1.0f;              // Contrast (0.5 to 2.0)
                                        // <1 = less contrast, >1 = more contrast
    float brightness = 0.0f;            // Brightness offset (-1 to +1)
    float gamma = 2.2f;                 // Gamma correction (1.0 to 3.0)
                                        // Lower = brighter midtones, higher = darker midtones

    // ========================================================================
    // COLOR BALANCE (Lift/Gamma/Gain)
    // ========================================================================

    // Traditional color grading controls (ASC CDL model)
    math::Vec3 lift{0.0f, 0.0f, 0.0f};  // Shadows color shift (-1 to +1)
    math::Vec3 gamma_color{1.0f, 1.0f, 1.0f};  // Midtones color shift (0.5 to 2.0)
    math::Vec3 gain{1.0f, 1.0f, 1.0f};  // Highlights color shift (0.5 to 2.0)

    // ========================================================================
    // VIGNETTE
    // ========================================================================

    float vignette_intensity = 0.0f;    // Vignette strength (0 to 1)
    float vignette_smoothness = 0.5f;   // Falloff smoothness (0 to 1)
    float vignette_roundness = 1.0f;    // Aspect ratio (0 to 1)
                                        // 0 = stretched, 1 = circular

    // ========================================================================
    // QUALITY SETTINGS
    // ========================================================================

    bool dithering = true;              // Dithering to prevent banding
    float dither_strength = 0.004f;     // Dither amount (0.001 to 0.01)

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Load configuration from INI file
     * @param filepath Path to INI file (e.g., "data/config/post_process.ini")
     * @return Loaded configuration
     */
    static PostProcessConfig load_from_ini(const std::string& filepath);

    /**
     * @brief Save configuration to INI file
     * @param filepath Path to INI file
     */
    void save_to_ini(const std::string& filepath) const;

    /**
     * @brief Create preset configurations
     */
    static PostProcessConfig create_neutral();          // No grading, neutral ACES
    static PostProcessConfig create_mirrors_edge();     // High contrast, bright, crisp
    static PostProcessConfig create_warm_cinematic();   // Warm orange tones
    static PostProcessConfig create_cool_cinematic();   // Cool blue tones
    static PostProcessConfig create_vintage();          // Low contrast, desaturated
    static PostProcessConfig create_vibrant();          // High saturation, punchy
};

/**
 * @brief Post-processing pipeline for deferred renderer
 *
 * Implements a complete GPU-based post-processing stack:
 * 1. Tone mapping (HDR → LDR)
 * 2. Color grading (temperature, tint, saturation, contrast)
 * 3. Color balance (lift/gamma/gain)
 * 4. Vignette effect
 * 5. Dithering (prevents color banding)
 *
 * All operations are performed in a single compute shader pass for maximum
 * performance. The pipeline processes the HDR output from the deferred
 * lighting pass and produces final LDR output suitable for display.
 *
 * Mathematical References:
 * - ACES: Academy Color Encoding System (Narkowicz 2015 approximation)
 * - Color Temperature: CIE 1931 chromaticity coordinates
 * - Lift/Gamma/Gain: ASC CDL (Color Decision List) model
 * - Dithering: Triangular probability density function (TPDF)
 *
 * Usage:
 * @code
 * PostProcessPipeline post_process(device, physical_device, allocator);
 * post_process.initialize(extent);
 *
 * // Configure
 * PostProcessConfig config = PostProcessConfig::create_mirrors_edge();
 * config.exposure_ev = 0.5f;  // Slightly brighter
 * post_process.set_config(config);
 *
 * // Render
 * post_process.apply(cmd, hdr_image, hdr_view, ldr_image, ldr_view);
 * @endcode
 */
class PostProcessPipeline {
public:
    PostProcessPipeline(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator);
    ~PostProcessPipeline();

    // Prevent copying
    PostProcessPipeline(const PostProcessPipeline&) = delete;
    PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize post-processing pipeline
     * @param extent Resolution of images to process
     */
    void initialize(VkExtent2D extent);

    /**
     * @brief Shutdown and cleanup all resources
     */
    void shutdown();

    /**
     * @brief Resize for new resolution
     * @param new_extent New resolution
     */
    void resize(VkExtent2D new_extent);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set post-processing configuration
     * @param config Configuration to apply
     */
    void set_config(const PostProcessConfig& config);

    /**
     * @brief Get current configuration
     */
    const PostProcessConfig& get_config() const { return config_; }

    /**
     * @brief Hot-reload configuration from INI file
     * @param filepath Path to INI file
     * @return true if successfully loaded
     */
    bool reload_config_from_ini(const std::string& filepath);

    // ========================================================================
    // RENDERING
    // ========================================================================

    /**
     * @brief Apply post-processing to HDR image
     *
     * Processes the HDR image through the complete post-processing stack:
     * - Tone mapping (HDR → LDR)
     * - Color grading
     * - Vignette
     * - Dithering
     *
     * @param cmd Command buffer (must be in recording state)
     * @param hdr_image Input HDR image (RGBA16F format)
     * @param hdr_view Input HDR image view
     * @param ldr_image Output LDR image (RGBA8 or swapchain format)
     * @param ldr_view Output LDR image view
     *
     * Note: This function handles all necessary image layout transitions.
     */
    void apply(VkCommandBuffer cmd,
               VkImage hdr_image, VkImageView hdr_view,
               VkImage ldr_image, VkImageView ldr_view);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    struct Stats {
        float last_frame_ms = 0.0f;     // Time spent in post-processing
        float average_luminance = 0.5f;  // Average scene luminance (auto exposure)
    };

    Stats get_stats() const { return stats_; }

private:
    // ========================================================================
    // RESOURCE CREATION
    // ========================================================================

    void create_descriptor_set_layout();
    void create_pipeline();
    void create_uniform_buffer();
    void update_descriptor_set(VkImageView hdr_view, VkImageView ldr_view);

    // ========================================================================
    // GPU DATA STRUCTURES
    // ========================================================================

    /**
     * @brief Uniform buffer data for compute shader
     * Must match layout in post_process.comp
     */
    struct alignas(16) UniformData {
        // Tone mapping
        uint32_t tone_mapping_mode;     // ToneMappingMode enum
        float exposure;                 // Exposure multiplier (2^EV)
        float white_point;              // White point for Reinhard Extended
        uint32_t _pad0;                 // Alignment padding

        // Color grading
        float temperature;              // Color temperature (-1 to +1)
        float tint;                     // Green/Magenta tint
        float saturation;               // Saturation multiplier
        float vibrance;                 // Vibrance

        float contrast;                 // Contrast
        float brightness;               // Brightness offset
        float gamma;                    // Gamma correction
        uint32_t _pad1;                 // Alignment padding

        // Color balance (lift/gamma/gain)
        math::Vec3 lift;                // Shadows
        float _pad2;                    // Alignment
        math::Vec3 gamma_color;         // Midtones
        float _pad3;                    // Alignment
        math::Vec3 gain;                // Highlights
        float _pad4;                    // Alignment

        // Vignette
        float vignette_intensity;       // Vignette strength
        float vignette_smoothness;      // Falloff smoothness
        float vignette_roundness;       // Aspect ratio
        uint32_t _pad5;                 // Alignment

        // Quality
        uint32_t dithering;             // 0 or 1
        float dither_strength;          // Dither amount
        uint32_t _pad6[2];              // Alignment padding

        // Screen dimensions
        float screen_width;             // Screen width
        float screen_height;            // Screen height
        uint32_t _pad7[2];              // Alignment padding
    };

    static_assert(sizeof(UniformData) % 16 == 0, "UniformData must be 16-byte aligned");

    // ========================================================================
    // VULKAN RESOURCES
    // ========================================================================

    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;

    // Pipeline resources
    VkPipeline compute_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    // Descriptor resources
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    // Sampler for HDR input
    VkSampler input_sampler_ = VK_NULL_HANDLE;

    // Uniform buffer
    VkBuffer uniform_buffer_ = VK_NULL_HANDLE;
    VmaAllocation uniform_buffer_allocation_ = VK_NULL_HANDLE;
    void* uniform_buffer_mapped_ = nullptr;  // Persistently mapped

    // Configuration
    PostProcessConfig config_;
    VkExtent2D extent_{};

    // Statistics
    Stats stats_;

    // State
    bool initialized_ = false;

    // Last descriptor set bindings (for caching)
    VkImageView last_hdr_view_ = VK_NULL_HANDLE;
    VkImageView last_ldr_view_ = VK_NULL_HANDLE;
};

} // namespace lore::graphics