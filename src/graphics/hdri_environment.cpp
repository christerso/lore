#include <lore/graphics/hdri_environment.hpp>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/constants.hpp>

#include <stdexcept>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>

namespace lore {

namespace {

// Helper to check Vulkan results
void check_vk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan error in ") + operation + ": " + std::to_string(result));
    }
}

// Helper to find memory type index
uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

// Create image view helper
VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format,
                               VkImageViewType view_type, uint32_t mip_levels,
                               uint32_t layer_count = 1) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = view_type;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = layer_count;

    VkImageView view;
    check_vk(vkCreateImageView(device, &view_info, nullptr, &view), "create image view");
    return view;
}

// Transition image layout
void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
                             VkImageLayout new_layout, uint32_t mip_levels, uint32_t layer_count = 1) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags dest_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_GENERAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, source_stage, dest_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Load SPIR-V shader from file
std::vector<uint32_t> load_spirv(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
    return buffer;
}

// Create shader module
VkShaderModule create_shader_module(VkDevice device, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    VkShaderModule shader_module;
    check_vk(vkCreateShaderModule(device, &create_info, nullptr, &shader_module), "create shader module");
    return shader_module;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIQualityConfig - Quality Presets
// ═══════════════════════════════════════════════════════════════════════════════

HDRIQualityConfig HDRIQualityConfig::create_ultra() {
    HDRIQualityConfig config;
    config.environment_resolution = 2048;
    config.irradiance_resolution = 64;
    config.prefiltered_mip_levels = 6;
    config.brdf_lut_resolution = 512;
    config.irradiance_sample_count = 2048;
    config.prefilter_sample_count = 2048;
    config.brdf_sample_count = 512;
    return config;
}

HDRIQualityConfig HDRIQualityConfig::create_high() {
    HDRIQualityConfig config;
    config.environment_resolution = 1024;
    config.irradiance_resolution = 32;
    config.prefiltered_mip_levels = 5;
    config.brdf_lut_resolution = 512;
    config.irradiance_sample_count = 1024;
    config.prefilter_sample_count = 1024;
    config.brdf_sample_count = 512;
    return config;
}

HDRIQualityConfig HDRIQualityConfig::create_medium() {
    HDRIQualityConfig config;
    config.environment_resolution = 512;
    config.irradiance_resolution = 32;
    config.prefiltered_mip_levels = 5;
    config.brdf_lut_resolution = 256;
    config.irradiance_sample_count = 512;
    config.prefilter_sample_count = 512;
    config.brdf_sample_count = 256;
    return config;
}

HDRIQualityConfig HDRIQualityConfig::create_low() {
    HDRIQualityConfig config;
    config.environment_resolution = 256;
    config.irradiance_resolution = 16;
    config.prefiltered_mip_levels = 4;
    config.brdf_lut_resolution = 128;
    config.irradiance_sample_count = 256;
    config.prefilter_sample_count = 256;
    config.brdf_sample_count = 128;
    return config;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════════

HDRIEnvironment::~HDRIEnvironment() {
    // Destructor should not call destroy() - user must call explicitly with context
    // This avoids storing VulkanContext reference in the class
}

HDRIEnvironment::HDRIEnvironment(HDRIEnvironment&& other) noexcept
    : m_file_path(std::move(other.m_file_path))
    , m_quality(other.m_quality)
    , m_params(other.m_params)
    , m_textures(other.m_textures)
    , m_ibl_generated(other.m_ibl_generated)
    , m_sun_info(other.m_sun_info)
    , m_sun_detected(other.m_sun_detected)
    , m_pixel_data(std::move(other.m_pixel_data))
    , m_pixel_width(other.m_pixel_width)
    , m_pixel_height(other.m_pixel_height)
    , m_equirect_to_cubemap_layout(other.m_equirect_to_cubemap_layout)
    , m_equirect_to_cubemap_pipeline(other.m_equirect_to_cubemap_pipeline)
    , m_irradiance_layout(other.m_irradiance_layout)
    , m_irradiance_pipeline(other.m_irradiance_pipeline)
    , m_prefilter_layout(other.m_prefilter_layout)
    , m_prefilter_pipeline(other.m_prefilter_pipeline)
    , m_brdf_lut_layout(other.m_brdf_lut_layout)
    , m_brdf_lut_pipeline(other.m_brdf_lut_pipeline)
    , m_descriptor_layout(other.m_descriptor_layout)
    , m_descriptor_pool(other.m_descriptor_pool)
    , m_descriptor_set(other.m_descriptor_set) {

    // Clear other's handles
    other.m_textures = {};
    other.m_sun_detected = false;
    other.m_pixel_data.clear();
    other.m_pixel_width = 0;
    other.m_pixel_height = 0;
    other.m_equirect_to_cubemap_layout = VK_NULL_HANDLE;
    other.m_equirect_to_cubemap_pipeline = VK_NULL_HANDLE;
    other.m_irradiance_layout = VK_NULL_HANDLE;
    other.m_irradiance_pipeline = VK_NULL_HANDLE;
    other.m_prefilter_layout = VK_NULL_HANDLE;
    other.m_prefilter_pipeline = VK_NULL_HANDLE;
    other.m_brdf_lut_layout = VK_NULL_HANDLE;
    other.m_brdf_lut_pipeline = VK_NULL_HANDLE;
    other.m_descriptor_layout = VK_NULL_HANDLE;
    other.m_descriptor_pool = VK_NULL_HANDLE;
    other.m_descriptor_set = VK_NULL_HANDLE;
    other.m_ibl_generated = false;
}

HDRIEnvironment& HDRIEnvironment::operator=(HDRIEnvironment&& other) noexcept {
    if (this != &other) {
        m_file_path = std::move(other.m_file_path);
        m_quality = other.m_quality;
        m_params = other.m_params;
        m_textures = other.m_textures;
        m_ibl_generated = other.m_ibl_generated;
        m_sun_info = other.m_sun_info;
        m_sun_detected = other.m_sun_detected;
        m_pixel_data = std::move(other.m_pixel_data);
        m_pixel_width = other.m_pixel_width;
        m_pixel_height = other.m_pixel_height;
        m_equirect_to_cubemap_layout = other.m_equirect_to_cubemap_layout;
        m_equirect_to_cubemap_pipeline = other.m_equirect_to_cubemap_pipeline;
        m_irradiance_layout = other.m_irradiance_layout;
        m_irradiance_pipeline = other.m_irradiance_pipeline;
        m_prefilter_layout = other.m_prefilter_layout;
        m_prefilter_pipeline = other.m_prefilter_pipeline;
        m_brdf_lut_layout = other.m_brdf_lut_layout;
        m_brdf_lut_pipeline = other.m_brdf_lut_pipeline;
        m_descriptor_layout = other.m_descriptor_layout;
        m_descriptor_pool = other.m_descriptor_pool;
        m_descriptor_set = other.m_descriptor_set;

        other.m_textures = {};
        other.m_sun_detected = false;
        other.m_pixel_data.clear();
        other.m_pixel_width = 0;
        other.m_pixel_height = 0;
        other.m_equirect_to_cubemap_layout = VK_NULL_HANDLE;
        other.m_equirect_to_cubemap_pipeline = VK_NULL_HANDLE;
        other.m_irradiance_layout = VK_NULL_HANDLE;
        other.m_irradiance_pipeline = VK_NULL_HANDLE;
        other.m_prefilter_layout = VK_NULL_HANDLE;
        other.m_prefilter_pipeline = VK_NULL_HANDLE;
        other.m_brdf_lut_layout = VK_NULL_HANDLE;
        other.m_brdf_lut_pipeline = VK_NULL_HANDLE;
        other.m_descriptor_layout = VK_NULL_HANDLE;
        other.m_descriptor_pool = VK_NULL_HANDLE;
        other.m_descriptor_set = VK_NULL_HANDLE;
        other.m_ibl_generated = false;
    }
    return *this;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - HDR Image Loading
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<float> HDRIEnvironment::load_hdr_image(const std::string& file_path, uint32_t& width, uint32_t& height) {
    // Determine file format by extension
    std::string ext = file_path.substr(file_path.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::vector<float> pixels;

    if (ext == ".exr") {
        // Load EXR using tinyexr
        float* rgba_data = nullptr;
        const char* err = nullptr;

        int ret = LoadEXR(&rgba_data, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height),
                         file_path.c_str(), &err);

        if (ret != TINYEXR_SUCCESS) {
            std::string error_msg = err ? err : "Unknown error";
            if (err) {
                FreeEXRErrorMessage(err);
            }
            throw std::runtime_error("Failed to load EXR file: " + error_msg);
        }

        // Copy pixel data
        size_t pixel_count = static_cast<size_t>(width) * height * 4;
        pixels.resize(pixel_count);
        std::memcpy(pixels.data(), rgba_data, pixel_count * sizeof(float));

        free(rgba_data);
    }
    else if (ext == ".hdr") {
        // Load Radiance HDR using stb_image
        int w = 0, h = 0, channels = 0;
        float* data = stbi_loadf(file_path.c_str(), &w, &h, &channels, 4);

        if (!data) {
            throw std::runtime_error("Failed to load HDR file: " + std::string(stbi_failure_reason()));
        }

        width = static_cast<uint32_t>(w);
        height = static_cast<uint32_t>(h);

        size_t pixel_count = static_cast<size_t>(width) * height * 4;
        pixels.resize(pixel_count);
        std::memcpy(pixels.data(), data, pixel_count * sizeof(float));

        stbi_image_free(data);
    }
    else {
        throw std::runtime_error("Unsupported HDR file format: " + ext + " (supported: .exr, .hdr)");
    }

    return pixels;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Loading from File
// ═══════════════════════════════════════════════════════════════════════════════

HDRIEnvironment HDRIEnvironment::load_from_file(VulkanContext& context, const std::string& file_path,
                                                 const HDRIQualityConfig& quality) {
    HDRIEnvironment hdri;
    hdri.m_file_path = file_path;
    hdri.m_quality = quality;

    // Load HDR image data
    uint32_t width = 0, height = 0;
    std::vector<float> pixel_data = load_hdr_image(file_path, width, height);

    // Store pixel data for sun detection
    hdri.m_pixel_data = pixel_data;
    hdri.m_pixel_width = width;
    hdri.m_pixel_height = height;

    // Create equirectangular texture on GPU
    hdri.create_equirectangular_texture(context, pixel_data, width, height);

    // Create output textures (environment cubemap, irradiance, prefiltered, BRDF LUT)
    hdri.create_output_textures(context);

    // Create samplers
    hdri.create_samplers(context);

    // Create compute pipelines
    hdri.create_compute_pipelines(context);

    return hdri;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Texture Creation
// ═══════════════════════════════════════════════════════════════════════════════

void HDRIEnvironment::create_equirectangular_texture(VulkanContext& context, const std::vector<float>& pixel_data,
                                                      uint32_t width, uint32_t height) {
    m_textures.equirect_width = width;
    m_textures.equirect_height = height;

    // Convert RGBA32F to RGBA16F for GPU efficiency using GLM's packHalf2x16
    std::vector<uint16_t> rgba16f_data(width * height * 4);
    for (size_t i = 0; i < pixel_data.size(); i += 2) {
        // Pack two float32 values into one uint32 containing two float16 values
        uint32_t packed = glm::packHalf2x16(glm::vec2(pixel_data[i], pixel_data[i + 1]));
        rgba16f_data[i] = static_cast<uint16_t>(packed & 0xFFFF);
        rgba16f_data[i + 1] = static_cast<uint16_t>((packed >> 16) & 0xFFFF);
    }

    VkDeviceSize image_size = rgba16f_data.size() * sizeof(uint16_t);

    // Create staging buffer
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = image_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo staging_alloc_info;
    check_vk(vmaCreateBuffer(context.allocator, &buffer_info, &alloc_info,
                             &staging_buffer, &staging_allocation, &staging_alloc_info),
             "create staging buffer");

    std::memcpy(staging_alloc_info.pMappedData, rgba16f_data.data(), image_size);

    // Create image
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo image_alloc_info{};
    image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check_vk(vmaCreateImage(context.allocator, &image_info, &image_alloc_info,
                            &m_textures.equirect_image, &m_textures.equirect_allocation, nullptr),
             "create equirectangular image");

    // Create command buffer for upload
    VkCommandBufferAllocateInfo alloc_cmd_info{};
    alloc_cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_cmd_info.commandBufferCount = 1;

    VkCommandPool temp_pool;
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = context.graphics_queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check_vk(vkCreateCommandPool(context.device, &pool_info, nullptr, &temp_pool), "create temp command pool");

    alloc_cmd_info.commandPool = temp_pool;
    VkCommandBuffer cmd;
    check_vk(vkAllocateCommandBuffers(context.device, &alloc_cmd_info, &cmd), "allocate command buffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Transition image to transfer destination
    transition_image_layout(cmd, m_textures.equirect_image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, staging_buffer, m_textures.equirect_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    transition_image_layout(cmd, m_textures.equirect_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    check_vk(vkQueueSubmit(context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE), "queue submit");
    check_vk(vkQueueWaitIdle(context.graphics_queue), "queue wait idle");

    // Cleanup
    vkFreeCommandBuffers(context.device, temp_pool, 1, &cmd);
    vkDestroyCommandPool(context.device, temp_pool, nullptr);
    vmaDestroyBuffer(context.allocator, staging_buffer, staging_allocation);

    // Create image view
    m_textures.equirect_view = create_image_view(context.device, m_textures.equirect_image,
                                                  VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);
}

void HDRIEnvironment::create_output_textures(VulkanContext& context) {
    // Create environment cubemap
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = m_quality.environment_resolution;
        image_info.extent.height = m_quality.environment_resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = m_quality.prefiltered_mip_levels;
        image_info.arrayLayers = 6;
        image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        check_vk(vmaCreateImage(context.allocator, &image_info, &alloc_info,
                               &m_textures.environment_image, &m_textures.environment_allocation, nullptr),
                "create environment cubemap");

        m_textures.environment_view = create_image_view(context.device, m_textures.environment_image,
                                                        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_CUBE,
                                                        m_quality.prefiltered_mip_levels, 6);
    }

    // Create irradiance cubemap
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = m_quality.irradiance_resolution;
        image_info.extent.height = m_quality.irradiance_resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 6;
        image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        check_vk(vmaCreateImage(context.allocator, &image_info, &alloc_info,
                               &m_textures.irradiance_image, &m_textures.irradiance_allocation, nullptr),
                "create irradiance cubemap");

        m_textures.irradiance_view = create_image_view(context.device, m_textures.irradiance_image,
                                                       VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_CUBE, 1, 6);
    }

    // Create prefiltered cubemap
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = m_quality.environment_resolution;
        image_info.extent.height = m_quality.environment_resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = m_quality.prefiltered_mip_levels;
        image_info.arrayLayers = 6;
        image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        check_vk(vmaCreateImage(context.allocator, &image_info, &alloc_info,
                               &m_textures.prefiltered_image, &m_textures.prefiltered_allocation, nullptr),
                "create prefiltered cubemap");

        m_textures.prefiltered_view = create_image_view(context.device, m_textures.prefiltered_image,
                                                        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_CUBE,
                                                        m_quality.prefiltered_mip_levels, 6);
    }

    // Create BRDF LUT
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = m_quality.brdf_lut_resolution;
        image_info.extent.height = m_quality.brdf_lut_resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_R16G16_SFLOAT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        check_vk(vmaCreateImage(context.allocator, &image_info, &alloc_info,
                               &m_textures.brdf_lut_image, &m_textures.brdf_lut_allocation, nullptr),
                "create BRDF LUT");

        m_textures.brdf_lut_view = create_image_view(context.device, m_textures.brdf_lut_image,
                                                      VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Sampler Creation
// ═══════════════════════════════════════════════════════════════════════════════

void HDRIEnvironment::create_samplers(VulkanContext& context) {
    // Environment sampler (trilinear filtering)
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = static_cast<float>(m_quality.prefiltered_mip_levels);
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        check_vk(vkCreateSampler(context.device, &sampler_info, nullptr, &m_textures.environment_sampler),
                "create environment sampler");
    }

    // Irradiance sampler (linear, no mipmaps)
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        check_vk(vkCreateSampler(context.device, &sampler_info, nullptr, &m_textures.irradiance_sampler),
                "create irradiance sampler");
    }

    // Prefiltered sampler (trilinear with full mip chain)
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = static_cast<float>(m_quality.prefiltered_mip_levels);
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        check_vk(vkCreateSampler(context.device, &sampler_info, nullptr, &m_textures.prefiltered_sampler),
                "create prefiltered sampler");
    }

    // BRDF LUT sampler (linear, clamp to edge)
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        check_vk(vkCreateSampler(context.device, &sampler_info, nullptr, &m_textures.brdf_lut_sampler),
                "create BRDF LUT sampler");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Compute Pipeline Creation
// ═══════════════════════════════════════════════════════════════════════════════

void HDRIEnvironment::create_compute_pipelines(VulkanContext& context) {
    // Create descriptor set layout (shared by all pipelines)
    {
        std::array<VkDescriptorSetLayoutBinding, 6> bindings{};

        // Binding 0: Equirectangular input
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 1: Environment cubemap output/input
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 2: Irradiance cubemap output
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 3: Prefiltered cubemap output
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 4: BRDF LUT output
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 5: Environment cubemap sampler (for prefilter and irradiance)
        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        check_vk(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &m_descriptor_layout),
                "create descriptor set layout");
    }

    // Create descriptor pool
    {
        std::array<VkDescriptorPoolSize, 2> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[0].descriptorCount = 2;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_sizes[1].descriptorCount = 4;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = 1;

        check_vk(vkCreateDescriptorPool(context.device, &pool_info, nullptr, &m_descriptor_pool),
                "create descriptor pool");
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &m_descriptor_layout;

        check_vk(vkAllocateDescriptorSets(context.device, &alloc_info, &m_descriptor_set),
                "allocate descriptor set");
    }

    // 1. Equirect to Cubemap Pipeline
    {
        auto shader_code = load_spirv("shaders/compiled/ibl/equirect_to_cubemap.spv");
        VkShaderModule shader_module = create_shader_module(context.device, shader_code);

        VkPushConstantRange push_constant{};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(uint32_t); // Face index

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &m_descriptor_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant;

        check_vk(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &m_equirect_to_cubemap_layout),
                "create equirect to cubemap pipeline layout");

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = m_equirect_to_cubemap_layout;

        check_vk(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                         &m_equirect_to_cubemap_pipeline),
                "create equirect to cubemap pipeline");

        vkDestroyShaderModule(context.device, shader_module, nullptr);
    }

    // 2. Irradiance Convolution Pipeline
    {
        auto shader_code = load_spirv("shaders/compiled/ibl/irradiance_convolution.spv");
        VkShaderModule shader_module = create_shader_module(context.device, shader_code);

        struct PushConstants {
            uint32_t sample_count;
        };

        VkPushConstantRange push_constant{};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &m_descriptor_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant;

        check_vk(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &m_irradiance_layout),
                "create irradiance pipeline layout");

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = m_irradiance_layout;

        check_vk(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                         &m_irradiance_pipeline),
                "create irradiance pipeline");

        vkDestroyShaderModule(context.device, shader_module, nullptr);
    }

    // 3. Prefilter Environment Pipeline
    {
        auto shader_code = load_spirv("shaders/compiled/ibl/prefilter_environment.spv");
        VkShaderModule shader_module = create_shader_module(context.device, shader_code);

        struct PushConstants {
            float roughness;
            uint32_t sample_count;
            uint32_t resolution;
        };

        VkPushConstantRange push_constant{};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &m_descriptor_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant;

        check_vk(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &m_prefilter_layout),
                "create prefilter pipeline layout");

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = m_prefilter_layout;

        check_vk(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                         &m_prefilter_pipeline),
                "create prefilter pipeline");

        vkDestroyShaderModule(context.device, shader_module, nullptr);
    }

    // 4. BRDF Integration Pipeline
    {
        auto shader_code = load_spirv("shaders/compiled/ibl/brdf_integration.spv");
        VkShaderModule shader_module = create_shader_module(context.device, shader_code);

        struct PushConstants {
            uint32_t sample_count;
        };

        VkPushConstantRange push_constant{};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &m_descriptor_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant;

        check_vk(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &m_brdf_lut_layout),
                "create BRDF LUT pipeline layout");

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = m_brdf_lut_layout;

        check_vk(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                         &m_brdf_lut_pipeline),
                "create BRDF LUT pipeline");

        vkDestroyShaderModule(context.device, shader_module, nullptr);
    }
}

void HDRIEnvironment::destroy_compute_pipelines(VulkanContext& context) {
    if (m_equirect_to_cubemap_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, m_equirect_to_cubemap_pipeline, nullptr);
        m_equirect_to_cubemap_pipeline = VK_NULL_HANDLE;
    }
    if (m_equirect_to_cubemap_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, m_equirect_to_cubemap_layout, nullptr);
        m_equirect_to_cubemap_layout = VK_NULL_HANDLE;
    }

    if (m_irradiance_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, m_irradiance_pipeline, nullptr);
        m_irradiance_pipeline = VK_NULL_HANDLE;
    }
    if (m_irradiance_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, m_irradiance_layout, nullptr);
        m_irradiance_layout = VK_NULL_HANDLE;
    }

    if (m_prefilter_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, m_prefilter_pipeline, nullptr);
        m_prefilter_pipeline = VK_NULL_HANDLE;
    }
    if (m_prefilter_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, m_prefilter_layout, nullptr);
        m_prefilter_layout = VK_NULL_HANDLE;
    }

    if (m_brdf_lut_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, m_brdf_lut_pipeline, nullptr);
        m_brdf_lut_pipeline = VK_NULL_HANDLE;
    }
    if (m_brdf_lut_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, m_brdf_lut_layout, nullptr);
        m_brdf_lut_layout = VK_NULL_HANDLE;
    }

    if (m_descriptor_set != VK_NULL_HANDLE) {
        // Descriptor sets are freed automatically when pool is destroyed
        m_descriptor_set = VK_NULL_HANDLE;
    }

    if (m_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, m_descriptor_pool, nullptr);
        m_descriptor_pool = VK_NULL_HANDLE;
    }

    if (m_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, m_descriptor_layout, nullptr);
        m_descriptor_layout = VK_NULL_HANDLE;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - IBL Generation
// ═══════════════════════════════════════════════════════════════════════════════

void HDRIEnvironment::generate_ibl_maps(VulkanContext& context, VkCommandBuffer cmd) {
    if (m_ibl_generated) {
        return; // Already generated
    }

    // Update descriptor set with current textures
    {
        std::array<VkWriteDescriptorSet, 6> writes{};

        VkDescriptorImageInfo equirect_info{};
        equirect_info.imageView = m_textures.equirect_view;
        equirect_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        equirect_info.sampler = m_textures.environment_sampler;

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptor_set;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &equirect_info;

        VkDescriptorImageInfo env_storage_info{};
        env_storage_info.imageView = m_textures.environment_view;
        env_storage_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descriptor_set;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &env_storage_info;

        VkDescriptorImageInfo irr_storage_info{};
        irr_storage_info.imageView = m_textures.irradiance_view;
        irr_storage_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_descriptor_set;
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &irr_storage_info;

        VkDescriptorImageInfo pref_storage_info{};
        pref_storage_info.imageView = m_textures.prefiltered_view;
        pref_storage_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = m_descriptor_set;
        writes[3].dstBinding = 3;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[3].descriptorCount = 1;
        writes[3].pImageInfo = &pref_storage_info;

        VkDescriptorImageInfo brdf_storage_info{};
        brdf_storage_info.imageView = m_textures.brdf_lut_view;
        brdf_storage_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = m_descriptor_set;
        writes[4].dstBinding = 4;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[4].descriptorCount = 1;
        writes[4].pImageInfo = &brdf_storage_info;

        VkDescriptorImageInfo env_sampler_info{};
        env_sampler_info.imageView = m_textures.environment_view;
        env_sampler_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        env_sampler_info.sampler = m_textures.environment_sampler;

        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = m_descriptor_set;
        writes[5].dstBinding = 5;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[5].descriptorCount = 1;
        writes[5].pImageInfo = &env_sampler_info;

        vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // Step 1: Convert equirectangular to cubemap
    convert_to_cubemap(context, cmd);

    // Step 2: Generate irradiance map
    generate_irradiance_map(context, cmd);

    // Step 3: Generate prefiltered environment map
    generate_prefiltered_map(context, cmd);

    // Step 4: Generate BRDF integration LUT
    generate_brdf_lut(context, cmd);

    m_ibl_generated = true;
}

void HDRIEnvironment::convert_to_cubemap(VulkanContext& context, VkCommandBuffer cmd) {
    // Transition environment image to GENERAL layout for storage
    transition_image_layout(cmd, m_textures.environment_image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL, m_quality.prefiltered_mip_levels, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_equirect_to_cubemap_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_equirect_to_cubemap_layout,
                           0, 1, &m_descriptor_set, 0, nullptr);

    // Dispatch for each cubemap face (mip level 0 only)
    for (uint32_t face = 0; face < 6; ++face) {
        vkCmdPushConstants(cmd, m_equirect_to_cubemap_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(uint32_t), &face);

        uint32_t group_count_x = (m_quality.environment_resolution + 7) / 8;
        uint32_t group_count_y = (m_quality.environment_resolution + 7) / 8;
        vkCmdDispatch(cmd, group_count_x, group_count_y, 1);
    }

    // Barrier: environment cubemap write → read for next stage
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textures.environment_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void HDRIEnvironment::generate_irradiance_map(VulkanContext& context, VkCommandBuffer cmd) {
    // Transition irradiance image to GENERAL layout
    transition_image_layout(cmd, m_textures.irradiance_image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL, 1, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradiance_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradiance_layout,
                           0, 1, &m_descriptor_set, 0, nullptr);

    uint32_t sample_count = m_quality.irradiance_sample_count;
    vkCmdPushConstants(cmd, m_irradiance_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(uint32_t), &sample_count);

    uint32_t group_count_x = (m_quality.irradiance_resolution + 7) / 8;
    uint32_t group_count_y = (m_quality.irradiance_resolution + 7) / 8;
    vkCmdDispatch(cmd, group_count_x, group_count_y, 6);

    // Barrier: irradiance write → read
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textures.irradiance_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void HDRIEnvironment::generate_prefiltered_map(VulkanContext& context, VkCommandBuffer cmd) {
    // Transition prefiltered image to GENERAL layout
    transition_image_layout(cmd, m_textures.prefiltered_image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL, m_quality.prefiltered_mip_levels, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_prefilter_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_prefilter_layout,
                           0, 1, &m_descriptor_set, 0, nullptr);

    // Generate each mip level with increasing roughness
    for (uint32_t mip = 0; mip < m_quality.prefiltered_mip_levels; ++mip) {
        float roughness = static_cast<float>(mip) / static_cast<float>(m_quality.prefiltered_mip_levels - 1);
        uint32_t mip_resolution = m_quality.environment_resolution >> mip;

        struct {
            float roughness;
            uint32_t sample_count;
            uint32_t resolution;
        } push_constants;

        push_constants.roughness = roughness;
        push_constants.sample_count = m_quality.prefilter_sample_count;
        push_constants.resolution = mip_resolution;

        vkCmdPushConstants(cmd, m_prefilter_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(push_constants), &push_constants);

        uint32_t group_count_x = (mip_resolution + 7) / 8;
        uint32_t group_count_y = (mip_resolution + 7) / 8;
        vkCmdDispatch(cmd, group_count_x, group_count_y, 6);

        // Barrier between mip levels
        if (mip < m_quality.prefiltered_mip_levels - 1) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_textures.prefiltered_image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = mip;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 6;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    // Final barrier: prefiltered write → read
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textures.prefiltered_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_quality.prefiltered_mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void HDRIEnvironment::generate_brdf_lut(VulkanContext& context, VkCommandBuffer cmd) {
    // Transition BRDF LUT to GENERAL layout
    transition_image_layout(cmd, m_textures.brdf_lut_image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL, 1, 1);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdf_lut_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdf_lut_layout,
                           0, 1, &m_descriptor_set, 0, nullptr);

    uint32_t sample_count = m_quality.brdf_sample_count;
    vkCmdPushConstants(cmd, m_brdf_lut_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(uint32_t), &sample_count);

    uint32_t group_count_x = (m_quality.brdf_lut_resolution + 7) / 8;
    uint32_t group_count_y = (m_quality.brdf_lut_resolution + 7) / 8;
    vkCmdDispatch(cmd, group_count_x, group_count_y, 1);

    // Barrier: BRDF LUT write → read
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textures.brdf_lut_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Helper Methods
// ═══════════════════════════════════════════════════════════════════════════════

EnvironmentUBO HDRIEnvironment::get_environment_ubo(EnvironmentMode mode) const {
    EnvironmentUBO ubo{};
    ubo.mode = static_cast<uint32_t>(mode);
    ubo.hdri_intensity = m_params.intensity;
    ubo.hdri_rotation_y = m_params.rotation_y;
    ubo.atmospheric_blend = m_params.atmospheric_blend;
    ubo.hdri_tint = m_params.tint;
    ubo.hdri_saturation = m_params.saturation;
    ubo.hdri_contrast = m_params.contrast;
    ubo.apply_fog = m_params.apply_fog ? 1u : 0u;
    ubo.apply_aerial_perspective = m_params.apply_aerial_perspective ? 1u : 0u;
    return ubo;
}

float HDRIEnvironment::calculate_average_luminance() const {
    // This would require downloading the texture from GPU
    // For now, return a default value
    // A complete implementation would use a compute shader to calculate this
    return 1.0f;
}

HDRISunInfo HDRIEnvironment::detect_sun() const {
    // Return cached result if already detected
    if (m_sun_detected) {
        return m_sun_info;
    }

    // Initialize result with default values (no sun detected)
    HDRISunInfo info{};
    info.direction = glm::vec3(0.0f, 1.0f, 0.0f);  // Default upward direction
    info.azimuth = 0.0f;
    info.elevation = glm::half_pi<float>();
    info.peak_luminance = 0.0f;
    info.average_luminance = 0.0f;
    info.has_sun = false;

    // Validate pixel data availability
    if (m_pixel_data.empty() || m_pixel_width == 0 || m_pixel_height == 0) {
        m_sun_info = info;
        m_sun_detected = true;
        return info;
    }

    // Single-pass scan: find peak luminance and accumulate average
    float max_luminance = 0.0f;
    uint32_t max_u = 0;
    uint32_t max_v = 0;
    double luminance_sum = 0.0;  // Use double for numerical stability
    uint32_t pixel_count = m_pixel_width * m_pixel_height;

    // Rec. 709 luminance coefficients (standard for HDR)
    constexpr float r_coef = 0.2126f;
    constexpr float g_coef = 0.7152f;
    constexpr float b_coef = 0.0722f;

    for (uint32_t v = 0; v < m_pixel_height; ++v) {
        for (uint32_t u = 0; u < m_pixel_width; ++u) {
            size_t pixel_index = (static_cast<size_t>(v) * m_pixel_width + u) * 4;

            // Validate array bounds
            if (pixel_index + 2 >= m_pixel_data.size()) {
                continue;
            }

            float r = m_pixel_data[pixel_index + 0];
            float g = m_pixel_data[pixel_index + 1];
            float b = m_pixel_data[pixel_index + 2];

            // Calculate relative luminance
            float luminance = r_coef * r + g_coef * g + b_coef * b;

            // Clamp negative values (invalid HDR data)
            if (luminance < 0.0f) {
                luminance = 0.0f;
            }

            // Track peak luminance
            if (luminance > max_luminance) {
                max_luminance = luminance;
                max_u = u;
                max_v = v;
            }

            // Accumulate for average
            luminance_sum += static_cast<double>(luminance);
        }
    }

    // Calculate average luminance
    float average_luminance = pixel_count > 0
        ? static_cast<float>(luminance_sum / static_cast<double>(pixel_count))
        : 0.0f;

    info.peak_luminance = max_luminance;
    info.average_luminance = average_luminance;

    // Determine if this is an outdoor HDRI with sun
    // Outdoor HDRIs have a bright sun disk that's significantly brighter than average
    // Indoor HDRIs have more even lighting
    float luminance_ratio = (average_luminance > 0.0f)
        ? (max_luminance / average_luminance)
        : 0.0f;

    info.has_sun = (luminance_ratio >= m_params.sun_detection_threshold);

    if (!info.has_sun) {
        // Indoor HDRI: return default upward direction
        m_sun_info = info;
        m_sun_detected = true;
        return info;
    }

    // Convert pixel coordinates to normalized UV [0, 1]
    float u_norm = (static_cast<float>(max_u) + 0.5f) / static_cast<float>(m_pixel_width);
    float v_norm = (static_cast<float>(max_v) + 0.5f) / static_cast<float>(m_pixel_height);

    // Convert UV to spherical angles
    // Equirectangular mapping:
    // u [0,1] maps to azimuth phi [0, 2π]
    // v [0,1] maps to elevation theta [π/2, -π/2] (top to bottom)
    float phi = u_norm * glm::two_pi<float>();  // Azimuth [0, 2π]
    float theta = (glm::half_pi<float>() - v_norm * glm::pi<float>());  // Elevation [π/2, -π/2]

    info.azimuth = phi;
    info.elevation = theta;

    // Convert spherical angles to Cartesian direction vector
    // Standard spherical coordinate conversion:
    // x = cos(elevation) * cos(azimuth)
    // y = sin(elevation)
    // z = cos(elevation) * sin(azimuth)
    float cos_theta = std::cos(theta);
    float sin_theta = std::sin(theta);
    float cos_phi = std::cos(phi);
    float sin_phi = std::sin(phi);

    info.direction.x = cos_theta * cos_phi;
    info.direction.y = sin_theta;
    info.direction.z = cos_theta * sin_phi;

    // Normalize to ensure unit vector (guard against floating-point errors)
    float length = glm::length(info.direction);
    if (length > 0.0001f) {
        info.direction = glm::normalize(info.direction);
    }

    // Cache result
    m_sun_info = info;
    m_sun_detected = true;

    return info;
}

void HDRIEnvironment::align_sun_to_direction(const glm::vec3& target_sun_direction) {
    // Detect sun if not already detected
    if (!m_sun_detected) {
        detect_sun();
    }

    // Early exit if no sun detected (indoor HDRI)
    if (!m_sun_info.has_sun) {
        return;
    }

    // Validate target direction
    float target_length = glm::length(target_sun_direction);
    if (target_length < 0.0001f) {
        // Invalid target direction, cannot align
        return;
    }

    // Normalize target direction
    glm::vec3 target_normalized = glm::normalize(target_sun_direction);
    glm::vec3 current_normalized = glm::normalize(m_sun_info.direction);

    // Calculate rotation around Y-axis (azimuth adjustment only)
    // This preserves the elevation angle of the sun in the HDRI
    // We only rotate horizontally to align the sun disk direction

    // Project both directions onto XZ plane (horizontal plane)
    glm::vec2 current_xz(current_normalized.x, current_normalized.z);
    glm::vec2 target_xz(target_normalized.x, target_normalized.z);

    // Handle cases where sun is directly overhead/below
    float current_xz_length = glm::length(current_xz);
    float target_xz_length = glm::length(target_xz);

    if (current_xz_length < 0.0001f || target_xz_length < 0.0001f) {
        // Sun is at zenith or nadir, rotation is ambiguous
        // Keep current rotation
        return;
    }

    // Normalize horizontal projections
    current_xz = glm::normalize(current_xz);
    target_xz = glm::normalize(target_xz);

    // Calculate angle between horizontal projections using atan2
    // atan2(z, x) gives angle in range [-π, π]
    float current_angle = std::atan2(current_xz.y, current_xz.x);
    float target_angle = std::atan2(target_xz.y, target_xz.x);

    // Calculate rotation needed (difference between angles)
    float rotation_y = target_angle - current_angle;

    // Normalize rotation to [-π, π] range
    constexpr float pi = glm::pi<float>();
    constexpr float two_pi = glm::two_pi<float>();

    while (rotation_y > pi) {
        rotation_y -= two_pi;
    }
    while (rotation_y < -pi) {
        rotation_y += two_pi;
    }

    // Update rotation parameter
    m_params.rotation_y = rotation_y;

    // Update cached sun info with new rotated direction
    // Apply Y-axis rotation to the original sun direction
    float cos_rot = std::cos(rotation_y);
    float sin_rot = std::sin(rotation_y);

    glm::vec3 rotated_direction;
    rotated_direction.x = current_normalized.x * cos_rot - current_normalized.z * sin_rot;
    rotated_direction.y = current_normalized.y;  // Y unchanged (horizontal rotation)
    rotated_direction.z = current_normalized.x * sin_rot + current_normalized.z * cos_rot;

    m_sun_info.direction = glm::normalize(rotated_direction);

    // Update azimuth (elevation stays the same)
    m_sun_info.azimuth = std::atan2(rotated_direction.z, rotated_direction.x);
    if (m_sun_info.azimuth < 0.0f) {
        m_sun_info.azimuth += two_pi;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDRIEnvironment - Resource Cleanup
// ═══════════════════════════════════════════════════════════════════════════════

void HDRIEnvironment::destroy(VulkanContext& context) {
    // Destroy samplers
    if (m_textures.environment_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, m_textures.environment_sampler, nullptr);
        m_textures.environment_sampler = VK_NULL_HANDLE;
    }
    if (m_textures.irradiance_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, m_textures.irradiance_sampler, nullptr);
        m_textures.irradiance_sampler = VK_NULL_HANDLE;
    }
    if (m_textures.prefiltered_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, m_textures.prefiltered_sampler, nullptr);
        m_textures.prefiltered_sampler = VK_NULL_HANDLE;
    }
    if (m_textures.brdf_lut_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, m_textures.brdf_lut_sampler, nullptr);
        m_textures.brdf_lut_sampler = VK_NULL_HANDLE;
    }

    // Destroy image views
    if (m_textures.equirect_view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, m_textures.equirect_view, nullptr);
        m_textures.equirect_view = VK_NULL_HANDLE;
    }
    if (m_textures.environment_view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, m_textures.environment_view, nullptr);
        m_textures.environment_view = VK_NULL_HANDLE;
    }
    if (m_textures.irradiance_view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, m_textures.irradiance_view, nullptr);
        m_textures.irradiance_view = VK_NULL_HANDLE;
    }
    if (m_textures.prefiltered_view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, m_textures.prefiltered_view, nullptr);
        m_textures.prefiltered_view = VK_NULL_HANDLE;
    }
    if (m_textures.brdf_lut_view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, m_textures.brdf_lut_view, nullptr);
        m_textures.brdf_lut_view = VK_NULL_HANDLE;
    }

    // Destroy images and free allocations
    if (m_textures.equirect_image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, m_textures.equirect_image, m_textures.equirect_allocation);
        m_textures.equirect_image = VK_NULL_HANDLE;
        m_textures.equirect_allocation = VK_NULL_HANDLE;
    }
    if (m_textures.environment_image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, m_textures.environment_image, m_textures.environment_allocation);
        m_textures.environment_image = VK_NULL_HANDLE;
        m_textures.environment_allocation = VK_NULL_HANDLE;
    }
    if (m_textures.irradiance_image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, m_textures.irradiance_image, m_textures.irradiance_allocation);
        m_textures.irradiance_image = VK_NULL_HANDLE;
        m_textures.irradiance_allocation = VK_NULL_HANDLE;
    }
    if (m_textures.prefiltered_image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, m_textures.prefiltered_image, m_textures.prefiltered_allocation);
        m_textures.prefiltered_image = VK_NULL_HANDLE;
        m_textures.prefiltered_allocation = VK_NULL_HANDLE;
    }
    if (m_textures.brdf_lut_image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, m_textures.brdf_lut_image, m_textures.brdf_lut_allocation);
        m_textures.brdf_lut_image = VK_NULL_HANDLE;
        m_textures.brdf_lut_allocation = VK_NULL_HANDLE;
    }

    // Destroy compute pipelines
    destroy_compute_pipelines(context);

    m_ibl_generated = false;
}

} // namespace lore