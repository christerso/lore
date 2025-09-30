#include <lore/graphics/post_process_pipeline.hpp>
#include <lore/core/log.hpp>

#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <unordered_map>

namespace lore::graphics {

namespace {
    // Helper to check Vulkan results
    void check_vk_result(VkResult result, const char* operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::string("Vulkan operation failed: ") + operation);
        }
    }

    // Load SPIR-V shader from file
    std::vector<uint32_t> load_spirv_shader(const std::filesystem::path& filepath) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error(std::string("Failed to open shader file: ") + filepath.string());
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);
        file.close();

        return buffer;
    }

    // Create Vulkan shader module
    VkShaderModule create_shader_module(VkDevice device, const std::vector<uint32_t>& spirv_code) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
        create_info.pCode = spirv_code.data();

        VkShaderModule shader_module;
        check_vk_result(
            vkCreateShaderModule(device, &create_info, nullptr, &shader_module),
            "create shader module"
        );

        return shader_module;
    }

    // Simple INI parser for configuration
    class INIParser {
    public:
        explicit INIParser(const std::string& filepath) {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                return;
            }

            std::string line;
            std::string current_section;

            while (std::getline(file, line)) {
                // Remove whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                // Skip empty lines and comments
                if (line.empty() || line[0] == ';' || line[0] == '#') {
                    continue;
                }

                // Parse section header
                if (line[0] == '[' && line.back() == ']') {
                    current_section = line.substr(1, line.size() - 2);
                    continue;
                }

                // Parse key-value pair
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = line.substr(0, eq_pos);
                    std::string value = line.substr(eq_pos + 1);

                    // Remove whitespace from key and value
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    // Store with section prefix
                    std::string full_key = current_section.empty() ? key : current_section + "." + key;
                    data_[full_key] = value;
                }
            }
        }

        bool has(const std::string& key) const {
            return data_.find(key) != data_.end();
        }

        std::string get_string(const std::string& key, const std::string& default_value = "") const {
            auto it = data_.find(key);
            return (it != data_.end()) ? it->second : default_value;
        }

        float get_float(const std::string& key, float default_value = 0.0f) const {
            auto it = data_.find(key);
            if (it != data_.end()) {
                try {
                    return std::stof(it->second);
                } catch (...) {
                    return default_value;
                }
            }
            return default_value;
        }

        int get_int(const std::string& key, int default_value = 0) const {
            auto it = data_.find(key);
            if (it != data_.end()) {
                try {
                    return std::stoi(it->second);
                } catch (...) {
                    return default_value;
                }
            }
            return default_value;
        }

        bool get_bool(const std::string& key, bool default_value = false) const {
            auto it = data_.find(key);
            if (it != data_.end()) {
                std::string value = it->second;
                // Convert to lowercase
                for (char& c : value) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                return value == "true" || value == "1" || value == "yes" || value == "on";
            }
            return default_value;
        }

    private:
        std::unordered_map<std::string, std::string> data_;
    };

    // Convert string to ToneMappingMode
    ToneMappingMode parse_tone_mapping_mode(const std::string& str) {
        std::string lower = str;
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (lower == "linear") return ToneMappingMode::Linear;
        if (lower == "reinhard") return ToneMappingMode::Reinhard;
        if (lower == "reinhard_extended") return ToneMappingMode::ReinhardExtended;
        if (lower == "aces") return ToneMappingMode::ACES;
        if (lower == "uncharted2") return ToneMappingMode::Uncharted2;
        if (lower == "hejl2015") return ToneMappingMode::Hejl2015;

        return ToneMappingMode::ACES;  // Default
    }

    // Convert ToneMappingMode to string
    std::string tone_mapping_mode_to_string(ToneMappingMode mode) {
        switch (mode) {
            case ToneMappingMode::Linear: return "linear";
            case ToneMappingMode::Reinhard: return "reinhard";
            case ToneMappingMode::ReinhardExtended: return "reinhard_extended";
            case ToneMappingMode::ACES: return "aces";
            case ToneMappingMode::Uncharted2: return "uncharted2";
            case ToneMappingMode::Hejl2015: return "hejl2015";
            default: return "aces";
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessConfig - Configuration Loading/Saving
// ═══════════════════════════════════════════════════════════════════════════════

PostProcessConfig PostProcessConfig::load_from_ini(const std::string& filepath) {
    PostProcessConfig config;

    if (!std::filesystem::exists(filepath)) {
        LOG_WARNING(Graphics, "Post-process config file not found: {}", filepath);
        return config;
    }

    INIParser parser(filepath);

    // Tone mapping
    config.tone_mapping_mode = parse_tone_mapping_mode(
        parser.get_string("ToneMapping.mode", "aces")
    );
    config.auto_exposure = parser.get_string("ToneMapping.exposure_mode", "manual") == "auto";
    config.exposure_ev = parser.get_float("ToneMapping.exposure_ev", 0.0f);
    config.exposure_min = parser.get_float("ToneMapping.exposure_min", 0.03f);
    config.exposure_max = parser.get_float("ToneMapping.exposure_max", 8.0f);
    config.auto_exposure_speed = parser.get_float("ToneMapping.auto_exposure_speed", 3.0f);
    config.white_point = parser.get_float("ToneMapping.white_point", 11.2f);

    // Color grading
    config.temperature = parser.get_float("ColorGrading.temperature", 0.0f);
    config.tint = parser.get_float("ColorGrading.tint", 0.0f);
    config.saturation = parser.get_float("ColorGrading.saturation", 1.0f);
    config.vibrance = parser.get_float("ColorGrading.vibrance", 0.0f);
    config.contrast = parser.get_float("ColorGrading.contrast", 1.0f);
    config.brightness = parser.get_float("ColorGrading.brightness", 0.0f);
    config.gamma = parser.get_float("ColorGrading.gamma", 2.2f);

    // Color balance
    config.lift.x = parser.get_float("ColorBalance.lift_r", 0.0f);
    config.lift.y = parser.get_float("ColorBalance.lift_g", 0.0f);
    config.lift.z = parser.get_float("ColorBalance.lift_b", 0.0f);
    config.gamma_color.x = parser.get_float("ColorBalance.gamma_r", 1.0f);
    config.gamma_color.y = parser.get_float("ColorBalance.gamma_g", 1.0f);
    config.gamma_color.z = parser.get_float("ColorBalance.gamma_b", 1.0f);
    config.gain.x = parser.get_float("ColorBalance.gain_r", 1.0f);
    config.gain.y = parser.get_float("ColorBalance.gain_g", 1.0f);
    config.gain.z = parser.get_float("ColorBalance.gain_b", 1.0f);

    // Vignette
    config.vignette_intensity = parser.get_float("Vignette.intensity", 0.0f);
    config.vignette_smoothness = parser.get_float("Vignette.smoothness", 0.5f);
    config.vignette_roundness = parser.get_float("Vignette.roundness", 1.0f);

    // Quality
    config.dithering = parser.get_bool("Quality.dithering", true);
    config.dither_strength = parser.get_float("Quality.dither_strength", 0.004f);

    LOG_INFO(Graphics, "Loaded post-process config from: {}", filepath);
    return config;
}

void PostProcessConfig::save_to_ini(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR(Graphics, "Failed to save post-process config: {}", filepath);
        return;
    }

    file << "[ToneMapping]\n";
    file << "mode = " << tone_mapping_mode_to_string(tone_mapping_mode) << "\n";
    file << "exposure_mode = " << (auto_exposure ? "auto" : "manual") << "\n";
    file << "exposure_ev = " << exposure_ev << "\n";
    file << "exposure_min = " << exposure_min << "\n";
    file << "exposure_max = " << exposure_max << "\n";
    file << "auto_exposure_speed = " << auto_exposure_speed << "\n";
    file << "white_point = " << white_point << "\n\n";

    file << "[ColorGrading]\n";
    file << "temperature = " << temperature << "\n";
    file << "tint = " << tint << "\n";
    file << "saturation = " << saturation << "\n";
    file << "vibrance = " << vibrance << "\n";
    file << "contrast = " << contrast << "\n";
    file << "brightness = " << brightness << "\n";
    file << "gamma = " << gamma << "\n\n";

    file << "[ColorBalance]\n";
    file << "lift_r = " << lift.x << "\n";
    file << "lift_g = " << lift.y << "\n";
    file << "lift_b = " << lift.z << "\n";
    file << "gamma_r = " << gamma_color.x << "\n";
    file << "gamma_g = " << gamma_color.y << "\n";
    file << "gamma_b = " << gamma_color.z << "\n";
    file << "gain_r = " << gain.x << "\n";
    file << "gain_g = " << gain.y << "\n";
    file << "gain_b = " << gain.z << "\n\n";

    file << "[Vignette]\n";
    file << "intensity = " << vignette_intensity << "\n";
    file << "smoothness = " << vignette_smoothness << "\n";
    file << "roundness = " << vignette_roundness << "\n\n";

    file << "[Quality]\n";
    file << "dithering = " << (dithering ? "true" : "false") << "\n";
    file << "dither_strength = " << dither_strength << "\n";

    LOG_INFO(Graphics, "Saved post-process config to: {}", filepath);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessConfig - Preset Configurations
// ═══════════════════════════════════════════════════════════════════════════════

PostProcessConfig PostProcessConfig::create_neutral() {
    PostProcessConfig config;
    // All defaults are neutral
    return config;
}

PostProcessConfig PostProcessConfig::create_mirrors_edge() {
    PostProcessConfig config;
    config.tone_mapping_mode = ToneMappingMode::ACES;
    config.exposure_ev = 0.3f;           // Slightly brighter
    config.contrast = 1.25f;             // Increased contrast
    config.saturation = 0.95f;           // Slightly desaturated
    config.brightness = 0.05f;           // Slightly brighter
    config.temperature = 0.05f;          // Slightly warm
    config.vignette_intensity = 0.15f;   // Subtle vignette
    config.vignette_smoothness = 0.6f;
    return config;
}

PostProcessConfig PostProcessConfig::create_warm_cinematic() {
    PostProcessConfig config;
    config.tone_mapping_mode = ToneMappingMode::Uncharted2;
    config.exposure_ev = 0.2f;
    config.contrast = 1.15f;
    config.saturation = 1.1f;
    config.temperature = 0.3f;           // Warm orange
    config.tint = -0.05f;                // Slight green tint
    config.vignette_intensity = 0.25f;
    config.vignette_smoothness = 0.5f;
    // Color balance for cinematic look
    config.lift = math::Vec3(0.02f, 0.0f, -0.02f);  // Lift shadows toward orange
    config.gain = math::Vec3(1.05f, 1.0f, 0.95f);   // Highlights more orange
    return config;
}

PostProcessConfig PostProcessConfig::create_cool_cinematic() {
    PostProcessConfig config;
    config.tone_mapping_mode = ToneMappingMode::Uncharted2;
    config.exposure_ev = -0.1f;          // Slightly darker
    config.contrast = 1.2f;              // Higher contrast
    config.saturation = 0.9f;            // Slightly desaturated
    config.temperature = -0.25f;         // Cool blue
    config.tint = 0.05f;                 // Slight magenta tint
    config.vignette_intensity = 0.3f;
    config.vignette_smoothness = 0.4f;
    // Color balance for cool look
    config.lift = math::Vec3(-0.02f, 0.0f, 0.02f);  // Lift shadows toward blue
    config.gain = math::Vec3(0.95f, 1.0f, 1.05f);   // Highlights more blue
    return config;
}

PostProcessConfig PostProcessConfig::create_vintage() {
    PostProcessConfig config;
    config.tone_mapping_mode = ToneMappingMode::ReinhardExtended;
    config.white_point = 8.0f;
    config.exposure_ev = 0.1f;
    config.contrast = 0.85f;             // Reduced contrast
    config.saturation = 0.75f;           // Desaturated
    config.brightness = 0.1f;            // Slightly brighter
    config.temperature = 0.15f;          // Warm
    config.gamma = 2.0f;                 // Brighter midtones
    config.vignette_intensity = 0.4f;    // Strong vignette
    config.vignette_smoothness = 0.3f;
    // Vintage color balance
    config.lift = math::Vec3(0.05f, 0.03f, 0.0f);   // Warm shadows
    config.gamma_color = math::Vec3(1.1f, 1.05f, 0.95f);  // Warm midtones
    return config;
}

PostProcessConfig PostProcessConfig::create_vibrant() {
    PostProcessConfig config;
    config.tone_mapping_mode = ToneMappingMode::ACES;
    config.exposure_ev = 0.25f;
    config.contrast = 1.15f;
    config.saturation = 1.35f;           // High saturation
    config.vibrance = 0.3f;              // Boost less-saturated colors
    config.brightness = 0.05f;
    config.temperature = 0.05f;          // Slightly warm
    config.vignette_intensity = 0.1f;    // Minimal vignette
    return config;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessPipeline - Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════

PostProcessPipeline::PostProcessPipeline(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator)
    : device_(device)
    , physical_device_(physical_device)
    , allocator_(allocator) {

    LOG_INFO(Graphics, "PostProcessPipeline created");
}

PostProcessPipeline::~PostProcessPipeline() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessPipeline - Initialization / Shutdown
// ═══════════════════════════════════════════════════════════════════════════════

void PostProcessPipeline::initialize(VkExtent2D extent) {
    if (initialized_) {
        LOG_WARNING(Graphics, "PostProcessPipeline already initialized");
        return;
    }

    LOG_INFO(Graphics, "Initializing PostProcessPipeline ({}x{})", extent.width, extent.height);

    extent_ = extent;

    // Create resources
    create_descriptor_set_layout();
    create_uniform_buffer();
    create_pipeline();

    // Create input sampler (linear filtering)
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    check_vk_result(
        vkCreateSampler(device_, &sampler_info, nullptr, &input_sampler_),
        "create post-process sampler"
    );

    initialized_ = true;
    LOG_INFO(Graphics, "PostProcessPipeline initialized successfully");
}

void PostProcessPipeline::shutdown() {
    if (!initialized_) {
        return;
    }

    LOG_INFO(Graphics, "Shutting down PostProcessPipeline");

    vkDeviceWaitIdle(device_);

    // Destroy pipeline
    if (compute_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, compute_pipeline_, nullptr);
        compute_pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    // Destroy descriptor resources
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    // Destroy sampler
    if (input_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, input_sampler_, nullptr);
        input_sampler_ = VK_NULL_HANDLE;
    }

    // Destroy uniform buffer
    if (uniform_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, uniform_buffer_, uniform_buffer_allocation_);
        uniform_buffer_ = VK_NULL_HANDLE;
        uniform_buffer_allocation_ = VK_NULL_HANDLE;
        uniform_buffer_mapped_ = nullptr;
    }

    initialized_ = false;
    LOG_INFO(Graphics, "PostProcessPipeline shutdown complete");
}

void PostProcessPipeline::resize(VkExtent2D new_extent) {
    if (!initialized_) {
        LOG_ERROR(Graphics, "Cannot resize: PostProcessPipeline not initialized");
        return;
    }

    LOG_INFO(Graphics, "Resizing PostProcessPipeline to {}x{}", new_extent.width, new_extent.height);

    extent_ = new_extent;

    // Invalidate cached descriptor set bindings
    last_hdr_view_ = VK_NULL_HANDLE;
    last_ldr_view_ = VK_NULL_HANDLE;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessPipeline - Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void PostProcessPipeline::set_config(const PostProcessConfig& config) {
    config_ = config;
}

bool PostProcessPipeline::reload_config_from_ini(const std::string& filepath) {
    try {
        config_ = PostProcessConfig::load_from_ini(filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Graphics, "Failed to reload post-process config: {}", e.what());
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessPipeline - Rendering
// ═══════════════════════════════════════════════════════════════════════════════

void PostProcessPipeline::apply(VkCommandBuffer cmd,
                                VkImage hdr_image, VkImageView hdr_view,
                                VkImage ldr_image, VkImageView ldr_view) {
    if (!initialized_) {
        LOG_ERROR(Graphics, "Cannot apply post-processing: not initialized");
        return;
    }

    // Update descriptor set if images changed
    if (last_hdr_view_ != hdr_view || last_ldr_view_ != ldr_view) {
        update_descriptor_set(hdr_view, ldr_view);
        last_hdr_view_ = hdr_view;
        last_ldr_view_ = ldr_view;
    }

    // Update uniform buffer with current configuration
    UniformData ubo{};
    ubo.tone_mapping_mode = static_cast<uint32_t>(config_.tone_mapping_mode);
    ubo.exposure = std::pow(2.0f, config_.exposure_ev);  // Convert EV to linear exposure
    ubo.white_point = config_.white_point;

    ubo.temperature = config_.temperature;
    ubo.tint = config_.tint;
    ubo.saturation = config_.saturation;
    ubo.vibrance = config_.vibrance;
    ubo.contrast = config_.contrast;
    ubo.brightness = config_.brightness;
    ubo.gamma = config_.gamma;

    ubo.lift = config_.lift;
    ubo.gamma_color = config_.gamma_color;
    ubo.gain = config_.gain;

    ubo.vignette_intensity = config_.vignette_intensity;
    ubo.vignette_smoothness = config_.vignette_smoothness;
    ubo.vignette_roundness = config_.vignette_roundness;

    ubo.dithering = config_.dithering ? 1 : 0;
    ubo.dither_strength = config_.dither_strength;

    ubo.screen_width = static_cast<float>(extent_.width);
    ubo.screen_height = static_cast<float>(extent_.height);

    // Copy to GPU (persistently mapped)
    std::memcpy(uniform_buffer_mapped_, &ubo, sizeof(UniformData));

    // Transition HDR image to shader read layout
    VkImageMemoryBarrier hdr_barrier{};
    hdr_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hdr_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdr_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    hdr_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdr_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdr_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdr_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdr_barrier.image = hdr_image;
    hdr_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    hdr_barrier.subresourceRange.baseMipLevel = 0;
    hdr_barrier.subresourceRange.levelCount = 1;
    hdr_barrier.subresourceRange.baseArrayLayer = 0;
    hdr_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &hdr_barrier
    );

    // Transition LDR image to general layout for shader write
    VkImageMemoryBarrier ldr_barrier{};
    ldr_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ldr_barrier.srcAccessMask = 0;
    ldr_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    ldr_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ldr_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ldr_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ldr_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ldr_barrier.image = ldr_image;
    ldr_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ldr_barrier.subresourceRange.baseMipLevel = 0;
    ldr_barrier.subresourceRange.levelCount = 1;
    ldr_barrier.subresourceRange.baseArrayLayer = 0;
    ldr_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &ldr_barrier
    );

    // Bind compute pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_,
                           0, 1, &descriptor_set_, 0, nullptr);

    // Dispatch compute shader (8×8 workgroups)
    uint32_t group_count_x = (extent_.width + 7) / 8;
    uint32_t group_count_y = (extent_.height + 7) / 8;
    vkCmdDispatch(cmd, group_count_x, group_count_y, 1);

    // Transition LDR image to present layout
    VkImageMemoryBarrier present_barrier{};
    present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    present_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    present_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    present_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    present_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    present_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    present_barrier.image = ldr_image;
    present_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    present_barrier.subresourceRange.baseMipLevel = 0;
    present_barrier.subresourceRange.levelCount = 1;
    present_barrier.subresourceRange.baseArrayLayer = 0;
    present_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &present_barrier
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// PostProcessPipeline - Resource Creation
// ═══════════════════════════════════════════════════════════════════════════════

void PostProcessPipeline::create_descriptor_set_layout() {
    // Layout bindings:
    // Binding 0: HDR input image (sampled)
    // Binding 1: LDR output image (storage)
    // Binding 2: Uniform buffer (config)

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: HDR input (combined image sampler)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: LDR output (storage image)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Uniform buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    check_vk_result(
        vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout_),
        "create post-process descriptor set layout"
    );

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 3> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[1].descriptorCount = 1;
    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    check_vk_result(
        vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
        "create post-process descriptor pool"
    );

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    check_vk_result(
        vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set_),
        "allocate post-process descriptor set"
    );
}

void PostProcessPipeline::create_pipeline() {
    // Load compute shader
    std::filesystem::path shader_path = "shaders/post_process.comp.spv";
    std::vector<uint32_t> shader_code = load_spirv_shader(shader_path);
    VkShaderModule shader_module = create_shader_module(device_, shader_code);

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_set_layout_;
    layout_info.pushConstantRangeCount = 0;

    check_vk_result(
        vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_),
        "create post-process pipeline layout"
    );

    // Create compute pipeline
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader_module;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = pipeline_layout_;

    check_vk_result(
        vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &compute_pipeline_),
        "create post-process compute pipeline"
    );

    // Cleanup shader module
    vkDestroyShaderModule(device_, shader_module, nullptr);

    LOG_INFO(Graphics, "Post-process pipeline created");
}

void PostProcessPipeline::create_uniform_buffer() {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(UniformData);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;  // Persistently mapped

    VmaAllocationInfo allocation_info{};
    check_vk_result(
        vmaCreateBuffer(allocator_, &buffer_info, &alloc_info, &uniform_buffer_,
                       &uniform_buffer_allocation_, &allocation_info),
        "create post-process uniform buffer"
    );

    uniform_buffer_mapped_ = allocation_info.pMappedData;

    // Update descriptor set with uniform buffer (binding 2)
    VkDescriptorBufferInfo buffer_desc{};
    buffer_desc.buffer = uniform_buffer_;
    buffer_desc.offset = 0;
    buffer_desc.range = sizeof(UniformData);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set_;
    write.dstBinding = 2;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_desc;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void PostProcessPipeline::update_descriptor_set(VkImageView hdr_view, VkImageView ldr_view) {
    std::array<VkWriteDescriptorSet, 2> writes{};

    // Binding 0: HDR input
    VkDescriptorImageInfo hdr_info{};
    hdr_info.sampler = input_sampler_;
    hdr_info.imageView = hdr_view;
    hdr_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptor_set_;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &hdr_info;

    // Binding 1: LDR output
    VkDescriptorImageInfo ldr_info{};
    ldr_info.imageView = ldr_view;
    ldr_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptor_set_;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &ldr_info;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace lore::graphics