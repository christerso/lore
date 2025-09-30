#include <lore/graphics/deferred_renderer.hpp>
#include <lore/core/log.hpp>

#include <stdexcept>
#include <array>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace lore::graphics {

namespace {
    // Helper function to check Vulkan results
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

    // Create Vulkan shader module from SPIR-V code
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
}

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════

DeferredRenderer::DeferredRenderer(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator)
    : device_(device)
    , physical_device_(physical_device)
    , allocator_(allocator) {

    LOG_INFO(Graphics, "DeferredRenderer created");
}

DeferredRenderer::~DeferredRenderer() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization / Shutdown
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::initialize(VkExtent2D extent, VkFormat swapchain_format) {
    if (initialized_) {
        LOG_WARNING(Graphics, "DeferredRenderer already initialized");
        return;
    }

    LOG_INFO(Graphics, "Initializing DeferredRenderer ({}x{})", extent.width, extent.height);

    // Create G-Buffer
    create_gbuffer(extent);

    // Create render passes
    create_geometry_render_pass();
    create_lighting_render_pass(swapchain_format);

    // Create descriptor resources
    create_descriptor_sets();

    // Create pipelines
    create_geometry_pipeline();
    create_lighting_pipeline();

    // Create light buffer
    create_light_buffer();

    // Register default material (index 0)
    PBRMaterial default_material{};
    default_material.albedo = math::Vec3(0.8f, 0.8f, 0.8f);
    default_material.metallic = 0.0f;
    default_material.roughness = 0.5f;
    default_material.ao = 1.0f;
    materials_.push_back(default_material);

    initialized_ = true;
    LOG_INFO(Graphics, "DeferredRenderer initialized successfully");
}

void DeferredRenderer::shutdown() {
    if (!initialized_) {
        return;
    }

    LOG_INFO(Graphics, "Shutting down DeferredRenderer");

    vkDeviceWaitIdle(device_);

    // Destroy pipelines
    if (geometry_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, geometry_pipeline_, nullptr);
        geometry_pipeline_ = VK_NULL_HANDLE;
    }
    if (geometry_pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, geometry_pipeline_layout_, nullptr);
        geometry_pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (lighting_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, lighting_pipeline_, nullptr);
        lighting_pipeline_ = VK_NULL_HANDLE;
    }
    if (lighting_pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, lighting_pipeline_layout_, nullptr);
        lighting_pipeline_layout_ = VK_NULL_HANDLE;
    }

    // Destroy render passes
    if (geometry_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, geometry_render_pass_, nullptr);
        geometry_render_pass_ = VK_NULL_HANDLE;
    }
    if (lighting_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, lighting_render_pass_, nullptr);
        lighting_render_pass_ = VK_NULL_HANDLE;
    }

    // Destroy all cached lighting framebuffers
    for (auto& [view, framebuffer] : lighting_framebuffer_cache_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    lighting_framebuffer_cache_.clear();
    last_lighting_target_ = VK_NULL_HANDLE;

    // Destroy descriptor sets and layouts
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    if (gbuffer_descriptor_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, gbuffer_descriptor_layout_, nullptr);
        gbuffer_descriptor_layout_ = VK_NULL_HANDLE;
    }
    if (lights_descriptor_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, lights_descriptor_layout_, nullptr);
        lights_descriptor_layout_ = VK_NULL_HANDLE;
    }
    if (ibl_descriptor_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, ibl_descriptor_layout_, nullptr);
        ibl_descriptor_layout_ = VK_NULL_HANDLE;
    }
    if (gbuffer_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, gbuffer_sampler_, nullptr);
        gbuffer_sampler_ = VK_NULL_HANDLE;
    }
    if (ibl_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, ibl_sampler_, nullptr);
        ibl_sampler_ = VK_NULL_HANDLE;
    }

    // Destroy IBL UBO buffer
    if (ibl_ubo_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, ibl_ubo_buffer_, ibl_ubo_allocation_);
        ibl_ubo_buffer_ = VK_NULL_HANDLE;
        ibl_ubo_allocation_ = VK_NULL_HANDLE;
    }

    // Destroy light buffer
    if (light_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, light_buffer_, light_buffer_allocation_);
        light_buffer_ = VK_NULL_HANDLE;
        light_buffer_allocation_ = VK_NULL_HANDLE;
    }

    // Destroy G-Buffer
    destroy_gbuffer();

    initialized_ = false;
    LOG_INFO(Graphics, "DeferredRenderer shutdown complete");
}

void DeferredRenderer::resize(VkExtent2D new_extent) {
    if (!initialized_) {
        LOG_ERROR(Graphics, "Cannot resize: DeferredRenderer not initialized");
        return;
    }

    LOG_INFO(Graphics, "Resizing DeferredRenderer to {}x{}", new_extent.width, new_extent.height);

    vkDeviceWaitIdle(device_);

    // Clear framebuffer cache since dimensions changed
    for (auto& [view, framebuffer] : lighting_framebuffer_cache_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    lighting_framebuffer_cache_.clear();
    last_lighting_target_ = VK_NULL_HANDLE;

    // Destroy old G-Buffer
    destroy_gbuffer();

    // Recreate G-Buffer with new size
    create_gbuffer(new_extent);

    // Update G-Buffer descriptor set with new images
    update_gbuffer_descriptor_set();

    LOG_INFO(Graphics, "DeferredRenderer resized successfully");
}

// ═══════════════════════════════════════════════════════════════════════════════
// G-Buffer Management
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_gbuffer(VkExtent2D extent) {
    LOG_DEBUG(Graphics, "Creating G-Buffer ({}x{})", extent.width, extent.height);

    gbuffer_.extent = extent;

    // Create attachments
    gbuffer_.albedo_metallic = create_attachment(
        extent,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    gbuffer_.normal_roughness = create_attachment(
        extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    gbuffer_.position_ao = create_attachment(
        extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    gbuffer_.emissive = create_attachment(
        extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    gbuffer_.depth = create_attachment(
        extent,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
    );

    // Create framebuffer for geometry pass
    std::array<VkImageView, 5> attachments = {
        gbuffer_.albedo_metallic.image_view,
        gbuffer_.normal_roughness.image_view,
        gbuffer_.position_ao.image_view,
        gbuffer_.emissive.image_view,
        gbuffer_.depth.image_view
    };

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = geometry_render_pass_;
    framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = 1;

    check_vk_result(
        vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &gbuffer_.framebuffer),
        "create G-Buffer framebuffer"
    );

    LOG_DEBUG(Graphics, "G-Buffer created successfully");
}

void DeferredRenderer::destroy_gbuffer() {
    if (gbuffer_.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, gbuffer_.framebuffer, nullptr);
        gbuffer_.framebuffer = VK_NULL_HANDLE;
    }

    destroy_attachment(gbuffer_.albedo_metallic);
    destroy_attachment(gbuffer_.normal_roughness);
    destroy_attachment(gbuffer_.position_ao);
    destroy_attachment(gbuffer_.emissive);
    destroy_attachment(gbuffer_.depth);
}

GBufferAttachment DeferredRenderer::create_attachment(
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect) {

    GBufferAttachment attachment;
    attachment.format = format;
    attachment.extent = extent;

    // Create image
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent.width = extent.width;
    image_info.extent.height = extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    check_vk_result(
        vmaCreateImage(allocator_, &image_info, &alloc_info,
                      &attachment.image, &attachment.allocation, nullptr),
        "create G-Buffer attachment image"
    );

    // Create image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = attachment.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    check_vk_result(
        vkCreateImageView(device_, &view_info, nullptr, &attachment.image_view),
        "create G-Buffer attachment image view"
    );

    return attachment;
}

void DeferredRenderer::destroy_attachment(GBufferAttachment& attachment) {
    if (attachment.image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, attachment.image_view, nullptr);
        attachment.image_view = VK_NULL_HANDLE;
    }

    if (attachment.image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, attachment.image, attachment.allocation);
        attachment.image = VK_NULL_HANDLE;
        attachment.allocation = VK_NULL_HANDLE;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Render Pass Creation
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_geometry_render_pass() {
    LOG_DEBUG(Graphics, "Creating geometry render pass");

    // Define attachments
    std::array<VkAttachmentDescription, 5> attachments{};

    // Attachment 0: Albedo + Metallic
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Attachment 1: Normal + Roughness
    attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Attachment 2: Position + AO
    attachments[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Attachment 3: Emissive
    attachments[3].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Attachment 4: Depth + Stencil
    attachments[4].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color attachment references
    std::array<VkAttachmentReference, 4> color_refs{};
    color_refs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    color_refs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    color_refs[2] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    color_refs[3] = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    // Depth attachment reference
    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 4;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.data();
    subpass.pDepthStencilAttachment = &depth_ref;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    // Dependency from external to subpass 0
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Dependency from subpass 0 to external (for shader reads in lighting pass)
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create render pass
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    check_vk_result(
        vkCreateRenderPass(device_, &render_pass_info, nullptr, &geometry_render_pass_),
        "create geometry render pass"
    );

    gbuffer_.render_pass = geometry_render_pass_;

    LOG_DEBUG(Graphics, "Geometry render pass created");
}

void DeferredRenderer::create_lighting_render_pass(VkFormat swapchain_format) {
    LOG_DEBUG(Graphics, "Creating lighting render pass");

    // Single color attachment (HDR output)
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    check_vk_result(
        vkCreateRenderPass(device_, &render_pass_info, nullptr, &lighting_render_pass_),
        "create lighting render pass"
    );

    LOG_DEBUG(Graphics, "Lighting render pass created");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pipeline Creation
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_geometry_pipeline() {
    LOG_DEBUG(Graphics, "Creating geometry pipeline");

    // Load SPIR-V shaders
    auto vert_spirv = load_spirv_shader("shaders/deferred_geometry.vert.spv");
    auto frag_spirv = load_spirv_shader("shaders/deferred_geometry.frag.spv");

    VkShaderModule vert_module = create_shader_module(device_, vert_spirv);
    VkShaderModule frag_module = create_shader_module(device_, frag_spirv);

    // Shader stage creation
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage, frag_stage};

    // Vertex input state (position, normal, tangent, texcoord)
    std::array<VkVertexInputBindingDescription, 1> binding_descriptions{};
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(float) * 11; // 3 + 3 + 3 + 2
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions{};
    // Position (location = 0)
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = 0;

    // Normal (location = 1)
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = sizeof(float) * 3;

    // Tangent (location = 2)
    attribute_descriptions[2].binding = 0;
    attribute_descriptions[2].location = 2;
    attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[2].offset = sizeof(float) * 6;

    // TexCoord (location = 3)
    attribute_descriptions[3].binding = 0;
    attribute_descriptions[3].location = 3;
    attribute_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[3].offset = sizeof(float) * 9;

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
    vertex_input.pVertexBindingDescriptions = binding_descriptions.data();
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input.pVertexAttributeDescriptions = attribute_descriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    // Multisampling (disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // Color blending (4 MRTs, no blending)
    std::array<VkPipelineColorBlendAttachmentState, 4> color_blend_attachments{};
    for (auto& attachment : color_blend_attachments) {
        attachment.blendEnable = VK_FALSE;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size());
    color_blending.pAttachments = color_blend_attachments.data();

    // Dynamic state (viewport and scissor)
    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Pipeline layout (descriptor sets will be added in create_descriptor_sets)
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;

    check_vk_result(
        vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &geometry_pipeline_layout_),
        "create geometry pipeline layout"
    );

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = geometry_pipeline_layout_;
    pipeline_info.renderPass = geometry_render_pass_;
    pipeline_info.subpass = 0;

    check_vk_result(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &geometry_pipeline_),
        "create geometry pipeline"
    );

    // Cleanup shader modules
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);

    LOG_INFO(Graphics, "Geometry pipeline created successfully");
}

void DeferredRenderer::create_lighting_pipeline() {
    LOG_DEBUG(Graphics, "Creating lighting pipeline");

    // Load SPIR-V shaders
    auto vert_spirv = load_spirv_shader("shaders/deferred_lighting.vert.spv");
    auto frag_spirv = load_spirv_shader("shaders/deferred_lighting.frag.spv");

    VkShaderModule vert_module = create_shader_module(device_, vert_spirv);
    VkShaderModule frag_module = create_shader_module(device_, frag_spirv);

    // Shader stage creation
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage, frag_stage};

    // Vertex input state (empty - full-screen triangle uses vertex pulling)
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    // Multisampling (disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    // Depth testing disabled for lighting pass
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;

    // Color blending (additive blending for HDR)
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Dynamic state
    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Pipeline layout with descriptor sets for G-Buffer and lights
    std::array<VkDescriptorSetLayout, 2> set_layouts = {
        gbuffer_descriptor_layout_,
        lights_descriptor_layout_
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_info.pSetLayouts = set_layouts.data();

    check_vk_result(
        vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &lighting_pipeline_layout_),
        "create lighting pipeline layout"
    );

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = lighting_pipeline_layout_;
    pipeline_info.renderPass = lighting_render_pass_;
    pipeline_info.subpass = 0;

    check_vk_result(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &lighting_pipeline_),
        "create lighting pipeline"
    );

    // Cleanup shader modules
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);

    LOG_INFO(Graphics, "Lighting pipeline created successfully");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Descriptor Sets
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_descriptor_sets() {
    LOG_DEBUG(Graphics, "Creating descriptor sets");

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 3> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[0].descriptorCount = 7; // 4 G-Buffer textures + 3 IBL cubemaps

    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[1].descriptorCount = 1; // 1 lights buffer

    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[2].descriptorCount = 1; // 1 IBL UBO

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 3; // G-Buffer set + lights set + IBL set

    check_vk_result(
        vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
        "create descriptor pool"
    );

    // Create G-Buffer descriptor set layout (4 combined image samplers)
    std::array<VkDescriptorSetLayoutBinding, 4> gbuffer_bindings{};
    for (uint32_t i = 0; i < 4; i++) {
        gbuffer_bindings[i].binding = i;
        gbuffer_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        gbuffer_bindings[i].descriptorCount = 1;
        gbuffer_bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        gbuffer_bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo gbuffer_layout_info{};
    gbuffer_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    gbuffer_layout_info.bindingCount = static_cast<uint32_t>(gbuffer_bindings.size());
    gbuffer_layout_info.pBindings = gbuffer_bindings.data();

    check_vk_result(
        vkCreateDescriptorSetLayout(device_, &gbuffer_layout_info, nullptr, &gbuffer_descriptor_layout_),
        "create G-Buffer descriptor set layout"
    );

    // Create lights descriptor set layout (1 storage buffer)
    VkDescriptorSetLayoutBinding lights_binding{};
    lights_binding.binding = 0;
    lights_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lights_binding.descriptorCount = 1;
    lights_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lights_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo lights_layout_info{};
    lights_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lights_layout_info.bindingCount = 1;
    lights_layout_info.pBindings = &lights_binding;

    check_vk_result(
        vkCreateDescriptorSetLayout(device_, &lights_layout_info, nullptr, &lights_descriptor_layout_),
        "create lights descriptor set layout"
    );

    // Create IBL descriptor set layout (3 cubemaps + 1 UBO)
    std::array<VkDescriptorSetLayoutBinding, 4> ibl_bindings{};

    // Binding 0: Irradiance cubemap
    ibl_bindings[0].binding = 0;
    ibl_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ibl_bindings[0].descriptorCount = 1;
    ibl_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ibl_bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Prefiltered environment cubemap
    ibl_bindings[1].binding = 1;
    ibl_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ibl_bindings[1].descriptorCount = 1;
    ibl_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ibl_bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: BRDF LUT
    ibl_bindings[2].binding = 2;
    ibl_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ibl_bindings[2].descriptorCount = 1;
    ibl_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ibl_bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: IBL UBO
    ibl_bindings[3].binding = 3;
    ibl_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ibl_bindings[3].descriptorCount = 1;
    ibl_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ibl_bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo ibl_layout_info{};
    ibl_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ibl_layout_info.bindingCount = static_cast<uint32_t>(ibl_bindings.size());
    ibl_layout_info.pBindings = ibl_bindings.data();

    check_vk_result(
        vkCreateDescriptorSetLayout(device_, &ibl_layout_info, nullptr, &ibl_descriptor_layout_),
        "create IBL descriptor set layout"
    );

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, 3> layouts = {
        gbuffer_descriptor_layout_,
        lights_descriptor_layout_,
        ibl_descriptor_layout_
    };

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, 3> descriptor_sets;
    check_vk_result(
        vkAllocateDescriptorSets(device_, &alloc_info, descriptor_sets.data()),
        "allocate descriptor sets"
    );

    gbuffer_descriptor_set_ = descriptor_sets[0];
    lights_descriptor_set_ = descriptor_sets[1];
    ibl_descriptor_set_ = descriptor_sets[2];

    // Create texture sampler for G-Buffer
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    check_vk_result(
        vkCreateSampler(device_, &sampler_info, nullptr, &gbuffer_sampler_),
        "create G-Buffer sampler"
    );

    // Create texture sampler for IBL cubemaps (linear filtering, mipmaps)
    VkSamplerCreateInfo ibl_sampler_info{};
    ibl_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ibl_sampler_info.magFilter = VK_FILTER_LINEAR;
    ibl_sampler_info.minFilter = VK_FILTER_LINEAR;
    ibl_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ibl_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ibl_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ibl_sampler_info.anisotropyEnable = VK_FALSE;
    ibl_sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    ibl_sampler_info.unnormalizedCoordinates = VK_FALSE;
    ibl_sampler_info.compareEnable = VK_FALSE;
    ibl_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ibl_sampler_info.minLod = 0.0f;
    ibl_sampler_info.maxLod = VK_LOD_CLAMP_NONE;  // Allow all mip levels

    check_vk_result(
        vkCreateSampler(device_, &ibl_sampler_info, nullptr, &ibl_sampler_),
        "create IBL sampler"
    );

    // Create IBL UBO buffer
    VkBufferCreateInfo ibl_ubo_info{};
    ibl_ubo_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibl_ubo_info.size = sizeof(IBLConfig);  // 4 floats = 16 bytes
    ibl_ubo_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ibl_ubo_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ibl_alloc_info{};
    ibl_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    ibl_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    check_vk_result(
        vmaCreateBuffer(allocator_, &ibl_ubo_info, &ibl_alloc_info,
                       &ibl_ubo_buffer_, &ibl_ubo_allocation_, nullptr),
        "create IBL UBO buffer"
    );

    // Update descriptor sets with initial data
    update_gbuffer_descriptor_set();
    update_lights_descriptor_set();
    // Note: IBL descriptor set will be updated when textures are set via set_ibl_textures()

    LOG_INFO(Graphics, "Descriptor sets created successfully");
}

void DeferredRenderer::update_gbuffer_descriptor_set() {
    if (gbuffer_descriptor_set_ == VK_NULL_HANDLE) {
        return;
    }

    // Prepare descriptor image info for G-Buffer textures
    std::array<VkDescriptorImageInfo, 4> image_infos{};
    image_infos[0].sampler = gbuffer_sampler_;
    image_infos[0].imageView = gbuffer_.albedo_metallic.image_view;
    image_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_infos[1].sampler = gbuffer_sampler_;
    image_infos[1].imageView = gbuffer_.normal_roughness.image_view;
    image_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_infos[2].sampler = gbuffer_sampler_;
    image_infos[2].imageView = gbuffer_.position_ao.image_view;
    image_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_infos[3].sampler = gbuffer_sampler_;
    image_infos[3].imageView = gbuffer_.emissive.image_view;
    image_infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Write descriptor updates
    std::array<VkWriteDescriptorSet, 4> descriptor_writes{};
    for (uint32_t i = 0; i < 4; i++) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].dstSet = gbuffer_descriptor_set_;
        descriptor_writes[i].dstBinding = i;
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].pImageInfo = &image_infos[i];
    }

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptor_writes.size()),
                          descriptor_writes.data(), 0, nullptr);

    LOG_DEBUG(Graphics, "G-Buffer descriptor set updated");
}

void DeferredRenderer::update_lights_descriptor_set() {
    if (lights_descriptor_set_ == VK_NULL_HANDLE || light_buffer_ == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = light_buffer_;
    buffer_info.offset = 0;
    buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = lights_descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);

    LOG_DEBUG(Graphics, "Lights descriptor set updated");
}

void DeferredRenderer::update_ibl_descriptor_set() {
    if (ibl_descriptor_set_ == VK_NULL_HANDLE) {
        return;
    }

    // Check if IBL textures are bound
    if (ibl_textures_.irradiance == VK_NULL_HANDLE ||
        ibl_textures_.prefiltered == VK_NULL_HANDLE ||
        ibl_textures_.brdf_lut == VK_NULL_HANDLE) {
        LOG_WARNING(Graphics, "IBL textures not fully bound, skipping descriptor update");
        return;
    }

    // Prepare descriptor image info for IBL textures
    std::array<VkDescriptorImageInfo, 3> image_infos{};

    // Irradiance cubemap
    image_infos[0].sampler = ibl_sampler_;
    image_infos[0].imageView = ibl_textures_.irradiance;
    image_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Prefiltered environment cubemap
    image_infos[1].sampler = ibl_sampler_;
    image_infos[1].imageView = ibl_textures_.prefiltered;
    image_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // BRDF integration LUT
    image_infos[2].sampler = ibl_sampler_;
    image_infos[2].imageView = ibl_textures_.brdf_lut;
    image_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Prepare descriptor buffer info for IBL UBO
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = ibl_ubo_buffer_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(IBLConfig);

    // Write descriptor updates
    std::array<VkWriteDescriptorSet, 4> descriptor_writes{};

    // Texture descriptors (bindings 0-2)
    for (uint32_t i = 0; i < 3; i++) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].dstSet = ibl_descriptor_set_;
        descriptor_writes[i].dstBinding = i;
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].pImageInfo = &image_infos[i];
    }

    // UBO descriptor (binding 3)
    descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[3].dstSet = ibl_descriptor_set_;
    descriptor_writes[3].dstBinding = 3;
    descriptor_writes[3].dstArrayElement = 0;
    descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_writes[3].descriptorCount = 1;
    descriptor_writes[3].pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptor_writes.size()),
                          descriptor_writes.data(), 0, nullptr);

    LOG_DEBUG(Graphics, "IBL descriptor set updated");
}

// ═══════════════════════════════════════════════════════════════════════════════
// IBL Management
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::set_ibl_textures(const IBLTextures& textures) {
    LOG_DEBUG(Graphics, "Setting IBL textures");

    ibl_textures_ = textures;

    // Update descriptor set with new textures
    update_ibl_descriptor_set();

    LOG_INFO(Graphics, "IBL textures set successfully");
}

void DeferredRenderer::set_ibl_config(const IBLConfig& config) {
    LOG_DEBUG(Graphics, "Updating IBL configuration");

    ibl_config_ = config;

    // Update UBO with new configuration
    void* data;
    vmaMapMemory(allocator_, ibl_ubo_allocation_, &data);
    memcpy(data, &ibl_config_, sizeof(IBLConfig));
    vmaUnmapMemory(allocator_, ibl_ubo_allocation_);

    LOG_DEBUG(Graphics, "IBL configuration updated: intensity={}, rotation={}, blend={}, max_lod={}",
             config.intensity, config.rotation_y, config.atmospheric_blend, config.max_reflection_lod);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Light Buffer Management
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_light_buffer() {
    LOG_DEBUG(Graphics, "Creating light buffer");

    // Initial buffer size for 256 lights
    VkDeviceSize buffer_size = sizeof(uint32_t) * 4 + sizeof(Light) * 256;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    check_vk_result(
        vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                       &light_buffer_, &light_buffer_allocation_, nullptr),
        "create light buffer"
    );

    LOG_DEBUG(Graphics, "Light buffer created");
}

void DeferredRenderer::update_light_buffer() {
    if (!lights_dirty_ || lights_.empty()) {
        return;
    }

    // Map buffer and write light data
    void* data;
    vmaMapMemory(allocator_, light_buffer_allocation_, &data);

    // Write light count
    uint32_t light_count = static_cast<uint32_t>(lights_.size());
    std::memcpy(data, &light_count, sizeof(uint32_t));

    // Write light data
    size_t offset = sizeof(uint32_t) * 4; // Account for padding
    std::memcpy(static_cast<char*>(data) + offset, lights_.data(), lights_.size() * sizeof(Light));

    vmaUnmapMemory(allocator_, light_buffer_allocation_);

    lights_dirty_ = false;
    stats_.lights_count = light_count;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Rendering Functions (Core interface - implementation requires pipelines)
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::begin_geometry_pass(VkCommandBuffer cmd) {
    // Update light buffer if dirty
    update_light_buffer();

    // Clear values for G-Buffer
    std::array<VkClearValue, 5> clear_values{};
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // Albedo
    clear_values[1].color = {{0.5f, 0.5f, 1.0f, 0.5f}};  // Normal (encoded)
    clear_values[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // Position
    clear_values[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Emissive
    clear_values[4].depthStencil = {1.0f, 0};             // Depth/Stencil

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = geometry_render_pass_;
    render_pass_info.framebuffer = gbuffer_.framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = gbuffer_.extent;
    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void DeferredRenderer::end_geometry_pass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void DeferredRenderer::begin_lighting_pass(VkCommandBuffer cmd, VkImage target_image, VkImageView target_view) {
    // Check if we have a cached framebuffer for this target view
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    auto it = lighting_framebuffer_cache_.find(target_view);
    if (it != lighting_framebuffer_cache_.end()) {
        // Use cached framebuffer
        framebuffer = it->second;
    } else {
        // Create new framebuffer and cache it
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = lighting_render_pass_;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &target_view;
        framebuffer_info.width = gbuffer_.extent.width;
        framebuffer_info.height = gbuffer_.extent.height;
        framebuffer_info.layers = 1;

        check_vk_result(
            vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffer),
            "create lighting framebuffer"
        );

        lighting_framebuffer_cache_[target_view] = framebuffer;
        LOG_DEBUG(Graphics, "Created and cached lighting framebuffer for target view");
    }

    last_lighting_target_ = target_view;

    // Begin render pass
    VkClearValue clear_value{};
    clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = lighting_render_pass_;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = gbuffer_.extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind lighting pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_pipeline_);

    // Set viewport and scissor dynamically
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(gbuffer_.extent.width);
    viewport.height = static_cast<float>(gbuffer_.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = gbuffer_.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind descriptor sets
    std::array<VkDescriptorSet, 2> descriptor_sets = {
        gbuffer_descriptor_set_,
        lights_descriptor_set_
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_pipeline_layout_,
                           0, static_cast<uint32_t>(descriptor_sets.size()),
                           descriptor_sets.data(), 0, nullptr);

    // Draw full-screen triangle (3 vertices, no vertex buffer needed)
    // The vertex shader will generate positions procedurally
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void DeferredRenderer::end_lighting_pass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Light Management
// ═══════════════════════════════════════════════════════════════════════════════

uint32_t DeferredRenderer::add_light(const Light& light) {
    uint32_t light_id = static_cast<uint32_t>(lights_.size());
    lights_.push_back(light);
    lights_dirty_ = true;
    return light_id;
}

void DeferredRenderer::update_light(uint32_t light_id, const Light& light) {
    if (light_id >= lights_.size()) {
        LOG_ERROR(Graphics, "Invalid light ID: {}", light_id);
        return;
    }
    lights_[light_id] = light;
    lights_dirty_ = true;
}

void DeferredRenderer::remove_light(uint32_t light_id) {
    if (light_id >= lights_.size()) {
        LOG_ERROR(Graphics, "Invalid light ID: {}", light_id);
        return;
    }
    lights_.erase(lights_.begin() + light_id);
    lights_dirty_ = true;
}

void DeferredRenderer::clear_lights() {
    lights_.clear();
    lights_dirty_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Material Management
// ═══════════════════════════════════════════════════════════════════════════════

uint32_t DeferredRenderer::register_material(const PBRMaterial& material) {
    uint32_t material_id = static_cast<uint32_t>(materials_.size());
    materials_.push_back(material);
    stats_.materials_count = static_cast<uint32_t>(materials_.size());
    return material_id;
}

const PBRMaterial& DeferredRenderer::get_material(uint32_t material_id) const {
    if (material_id >= materials_.size()) {
        LOG_ERROR(Graphics, "Invalid material ID: {}, returning default", material_id);
        return materials_[0];  // Return default material
    }
    return materials_[material_id];
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shadow Configuration Presets
// ═══════════════════════════════════════════════════════════════════════════════

ShadowConfig ShadowConfig::create_low_quality() {
    ShadowConfig config;
    config.cascade_count = 1;
    config.cascade_resolution = 1024;
    config.quality = ShadowQuality::Hard;
    config.shadow_bias = 0.001f;
    config.shadow_normal_bias = 0.002f;
    config.shadow_slope_bias = 0.0002f;
    config.cascade_split_lambda = 0.5f;
    config.max_shadow_distance = 50.0f;
    config.pcf_radius = 0.0f;
    config.soft_shadow_scale = 1.0f;
    config.use_poisson_disk = false;
    config.enable_early_exit = true;
    config.enable_backface_culling = true;

    LOG_INFO(Graphics, "Created Low Quality shadow preset (1 cascade, 1024 res, Hard shadows)");
    return config;
}

ShadowConfig ShadowConfig::create_medium_quality() {
    ShadowConfig config;
    config.cascade_count = 2;
    config.cascade_resolution = 2048;
    config.quality = ShadowQuality::PCF_3x3;
    config.shadow_bias = 0.0007f;
    config.shadow_normal_bias = 0.0015f;
    config.shadow_slope_bias = 0.00015f;
    config.cascade_split_lambda = 0.5f;
    config.max_shadow_distance = 75.0f;
    config.pcf_radius = 1.0f;
    config.soft_shadow_scale = 1.0f;
    config.use_poisson_disk = true;
    config.enable_early_exit = true;
    config.enable_backface_culling = true;

    LOG_INFO(Graphics, "Created Medium Quality shadow preset (2 cascades, 2048 res, PCF 3x3)");
    return config;
}

ShadowConfig ShadowConfig::create_high_quality() {
    ShadowConfig config;
    config.cascade_count = 4;
    config.cascade_resolution = 2048;
    config.quality = ShadowQuality::PCF_5x5;
    config.shadow_bias = 0.0005f;
    config.shadow_normal_bias = 0.001f;
    config.shadow_slope_bias = 0.0001f;
    config.cascade_split_lambda = 0.5f;
    config.max_shadow_distance = 100.0f;
    config.shadow_fade_start = 0.8f;
    config.shadow_fade_end = 1.0f;
    config.pcf_radius = 1.5f;
    config.soft_shadow_scale = 1.0f;
    config.use_poisson_disk = true;
    config.enable_early_exit = true;
    config.enable_backface_culling = true;

    LOG_INFO(Graphics, "Created High Quality shadow preset (4 cascades, 2048 res, PCF 5x5)");
    return config;
}

ShadowConfig ShadowConfig::create_ultra_quality() {
    ShadowConfig config;
    config.cascade_count = 4;
    config.cascade_resolution = 4096;
    config.quality = ShadowQuality::PCF_7x7;
    config.shadow_bias = 0.0003f;
    config.shadow_normal_bias = 0.0007f;
    config.shadow_slope_bias = 0.00007f;
    config.cascade_split_lambda = 0.6f;  // More logarithmic split for better quality
    config.max_shadow_distance = 150.0f;
    config.shadow_fade_start = 0.85f;
    config.shadow_fade_end = 1.0f;
    config.pcf_radius = 2.0f;
    config.soft_shadow_scale = 1.2f;
    config.use_poisson_disk = true;
    config.enable_early_exit = true;
    config.enable_backface_culling = true;

    LOG_INFO(Graphics, "Created Ultra Quality shadow preset (4 cascades, 4096 res, PCF 7x7)");
    return config;
}

ShadowConfig ShadowConfig::create_mirrors_edge_crisp() {
    ShadowConfig config;
    config.cascade_count = 4;
    config.cascade_resolution = 2048;
    config.quality = ShadowQuality::Hard;  // Crisp, sharp shadows
    config.shadow_bias = 0.0003f;  // Minimal bias for crisp edges
    config.shadow_normal_bias = 0.0005f;
    config.shadow_slope_bias = 0.00005f;
    config.cascade_split_lambda = 0.7f;  // More logarithmic for quality
    config.max_shadow_distance = 100.0f;
    config.shadow_fade_start = 0.95f;  // Very late fade for strong contrast
    config.shadow_fade_end = 1.0f;
    config.pcf_radius = 0.0f;  // No filtering
    config.soft_shadow_scale = 0.5f;  // Sharp
    config.use_poisson_disk = false;
    config.enable_early_exit = true;
    config.enable_backface_culling = true;

    LOG_INFO(Graphics, "Created Mirror's Edge Crisp shadow preset (4 cascades, 2048 res, Hard shadows, high contrast)");
    return config;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shadow Pipeline Creation
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::create_shadow_pipeline() {
    LOG_DEBUG(Graphics, "Creating shadow mapping pipeline");

    // Create shadow render pass (depth-only)
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = VK_FORMAT_D32_SFLOAT;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 0;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;  // Depth-only pass
    subpass.pDepthStencilAttachment = &depth_ref;

    // Subpass dependencies for proper layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    // Dependency from external to subpass (depth writes)
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Dependency from subpass to external (shader reads)
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &depth_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    check_vk_result(
        vkCreateRenderPass(device_, &render_pass_info, nullptr, &shadow_maps_.shadow_render_pass),
        "create shadow render pass"
    );

    LOG_DEBUG(Graphics, "Shadow render pass created");

    // Load shadow shaders
    auto vert_spirv = load_spirv_shader("shaders/shadow_depth.vert.spv");
    auto frag_spirv = load_spirv_shader("shaders/shadow_depth.frag.spv");

    VkShaderModule vert_module = create_shader_module(device_, vert_spirv);
    VkShaderModule frag_module = create_shader_module(device_, frag_spirv);

    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage, frag_stage};

    // Vertex input (position only for shadow pass)
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(float) * 3;  // Position only
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr_desc{};
    attr_desc.binding = 0;
    attr_desc.location = 0;
    attr_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_desc.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attr_desc;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_TRUE;  // Enable depth clamping for better shadow quality
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = shadow_config_.enable_backface_culling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;  // Enable depth bias for shadow acne prevention
    rasterizer.depthBiasConstantFactor = 0.0f;  // Set dynamically via vkCmdSetDepthBias
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;
    rasterizer.lineWidth = 1.0f;

    // Multisampling (disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // Color blending (no color attachments)
    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 0;

    // Dynamic state
    std::array<VkDynamicState, 3> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Push constant for light-space matrix
    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(math::Mat4);  // Light-space view-projection matrix

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant;

    check_vk_result(
        vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &shadow_maps_.shadow_pipeline_layout),
        "create shadow pipeline layout"
    );

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = shadow_maps_.shadow_pipeline_layout;
    pipeline_info.renderPass = shadow_maps_.shadow_render_pass;
    pipeline_info.subpass = 0;

    check_vk_result(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &shadow_maps_.shadow_pipeline),
        "create shadow pipeline"
    );

    // Cleanup shader modules
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);

    LOG_INFO(Graphics, "Shadow pipeline created successfully");
}

void DeferredRenderer::create_shadow_maps() {
    LOG_DEBUG(Graphics, "Creating shadow map cascades ({} cascades, {}x{} resolution)",
              shadow_config_.cascade_count, shadow_config_.cascade_resolution, shadow_config_.cascade_resolution);

    shadow_maps_.active_cascades = shadow_config_.cascade_count;

    for (uint32_t i = 0; i < shadow_config_.cascade_count; ++i) {
        auto& cascade = shadow_maps_.cascades[i];

        // Create depth image
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_D32_SFLOAT;
        image_info.extent.width = shadow_config_.cascade_resolution;
        image_info.extent.height = shadow_config_.cascade_resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        check_vk_result(
            vmaCreateImage(allocator_, &image_info, &alloc_info,
                          &cascade.shadow_map, &cascade.allocation, nullptr),
            "create shadow map image"
        );

        // Create image view
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = cascade.shadow_map;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_D32_SFLOAT;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        check_vk_result(
            vkCreateImageView(device_, &view_info, nullptr, &cascade.shadow_map_view),
            "create shadow map image view"
        );

        // Create framebuffer for this cascade
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = shadow_maps_.shadow_render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &cascade.shadow_map_view;
        framebuffer_info.width = shadow_config_.cascade_resolution;
        framebuffer_info.height = shadow_config_.cascade_resolution;
        framebuffer_info.layers = 1;

        check_vk_result(
            vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &cascade.framebuffer),
            "create shadow framebuffer"
        );

        LOG_DEBUG(Graphics, "Created shadow map cascade {} ({}x{})",
                  i, shadow_config_.cascade_resolution, shadow_config_.cascade_resolution);
    }

    LOG_INFO(Graphics, "Shadow map cascades created successfully ({} cascades)", shadow_config_.cascade_count);
}

void DeferredRenderer::destroy_shadow_maps() {
    LOG_DEBUG(Graphics, "Destroying shadow maps");

    for (uint32_t i = 0; i < 4; ++i) {
        auto& cascade = shadow_maps_.cascades[i];

        if (cascade.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, cascade.framebuffer, nullptr);
            cascade.framebuffer = VK_NULL_HANDLE;
        }

        if (cascade.shadow_map_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, cascade.shadow_map_view, nullptr);
            cascade.shadow_map_view = VK_NULL_HANDLE;
        }

        if (cascade.shadow_map != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, cascade.shadow_map, cascade.allocation);
            cascade.shadow_map = VK_NULL_HANDLE;
            cascade.allocation = VK_NULL_HANDLE;
        }
    }

    if (shadow_maps_.shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, shadow_maps_.shadow_sampler, nullptr);
        shadow_maps_.shadow_sampler = VK_NULL_HANDLE;
    }

    if (shadow_maps_.shadow_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, shadow_maps_.shadow_pipeline, nullptr);
        shadow_maps_.shadow_pipeline = VK_NULL_HANDLE;
    }

    if (shadow_maps_.shadow_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, shadow_maps_.shadow_pipeline_layout, nullptr);
        shadow_maps_.shadow_pipeline_layout = VK_NULL_HANDLE;
    }

    if (shadow_maps_.shadow_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, shadow_maps_.shadow_render_pass, nullptr);
        shadow_maps_.shadow_render_pass = VK_NULL_HANDLE;
    }

    shadow_maps_.active_cascades = 0;

    LOG_DEBUG(Graphics, "Shadow maps destroyed");
}

void DeferredRenderer::create_shadow_sampler() {
    LOG_DEBUG(Graphics, "Creating shadow sampler with comparison mode");

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;  // Linear filtering for PCF
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  // Outside shadow = lit
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_TRUE;  // Enable depth comparison for PCF
    sampler_info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    check_vk_result(
        vkCreateSampler(device_, &sampler_info, nullptr, &shadow_maps_.shadow_sampler),
        "create shadow sampler"
    );

    LOG_DEBUG(Graphics, "Shadow sampler created with PCF comparison mode");
}

void DeferredRenderer::update_shadow_descriptor_set() {
    if (shadow_maps_.shadow_descriptor_set == VK_NULL_HANDLE) {
        LOG_WARNING(Graphics, "Shadow descriptor set is null, skipping update");
        return;
    }

    LOG_DEBUG(Graphics, "Updating shadow descriptor set with {} cascades", shadow_maps_.active_cascades);

    // Prepare descriptor image infos for all cascade shadow maps
    std::array<VkDescriptorImageInfo, 4> image_infos{};
    for (uint32_t i = 0; i < shadow_maps_.active_cascades; ++i) {
        image_infos[i].sampler = shadow_maps_.shadow_sampler;
        image_infos[i].imageView = shadow_maps_.cascades[i].shadow_map_view;
        image_infos[i].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    // Write descriptor updates
    std::array<VkWriteDescriptorSet, 4> descriptor_writes{};
    for (uint32_t i = 0; i < shadow_maps_.active_cascades; ++i) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].dstSet = shadow_maps_.shadow_descriptor_set;
        descriptor_writes[i].dstBinding = i;
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].pImageInfo = &image_infos[i];
    }

    vkUpdateDescriptorSets(device_, shadow_maps_.active_cascades,
                          descriptor_writes.data(), 0, nullptr);

    LOG_DEBUG(Graphics, "Shadow descriptor set updated successfully");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cascade Calculation
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::update_shadow_cascades(
    const math::Vec3& camera_pos,
    const math::Vec3& camera_forward,
    const math::Vec3& light_dir,
    float camera_near,
    float camera_far) {

    if (shadow_maps_.active_cascades == 0) {
        LOG_WARNING(Graphics, "No active shadow cascades to update");
        return;
    }

    // Clamp far plane to max shadow distance
    const float shadow_far = std::min(camera_far, shadow_config_.max_shadow_distance);

    // Calculate cascade split distances using logarithmic/uniform blend
    std::array<float, 5> split_distances;
    split_distances[0] = camera_near;

    const float lambda = shadow_config_.cascade_split_lambda;
    const float range = shadow_far - camera_near;
    const float ratio = shadow_far / camera_near;

    for (uint32_t i = 1; i < shadow_maps_.active_cascades; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(shadow_maps_.active_cascades);

        // Logarithmic split
        const float log_split = camera_near * std::pow(ratio, p);

        // Uniform split
        const float uniform_split = camera_near + range * p;

        // Blend between logarithmic and uniform based on lambda
        split_distances[i] = lambda * log_split + (1.0f - lambda) * uniform_split;
    }
    split_distances[shadow_maps_.active_cascades] = shadow_far;

    // Update shadow config with calculated splits for shader access
    for (uint32_t i = 0; i < shadow_maps_.active_cascades; ++i) {
        shadow_config_.cascade_splits[i] = split_distances[i + 1];
    }

    LOG_DEBUG(Graphics, "Cascade split distances: {}, {}, {}, {}",
              shadow_config_.cascade_splits[0],
              shadow_config_.cascade_splits[1],
              shadow_config_.cascade_splits[2],
              shadow_config_.cascade_splits[3]);

    // Normalize light direction
    const math::Vec3 light_dir_norm = glm::normalize(light_dir);

    // Calculate camera right and up vectors
    const math::Vec3 camera_up(0.0f, 1.0f, 0.0f);
    const math::Vec3 camera_right = glm::normalize(glm::cross(camera_forward, camera_up));
    const math::Vec3 camera_up_corrected = glm::normalize(glm::cross(camera_right, camera_forward));

    // Calculate light view matrix (looking from light towards scene)
    const math::Vec3 light_up = std::abs(light_dir_norm.y) > 0.99f
        ? math::Vec3(0.0f, 0.0f, 1.0f)  // Use Z-up if light is nearly vertical
        : math::Vec3(0.0f, 1.0f, 0.0f);

    const math::Mat4 light_view = glm::lookAt(math::Vec3(0.0f), light_dir_norm, light_up);

    // Calculate view frustum corners for each cascade
    for (uint32_t cascade_idx = 0; cascade_idx < shadow_maps_.active_cascades; ++cascade_idx) {
        auto& cascade = shadow_maps_.cascades[cascade_idx];

        const float near_dist = split_distances[cascade_idx];
        const float far_dist = split_distances[cascade_idx + 1];

        // Store split distances
        cascade.split_near = near_dist;
        cascade.split_far = far_dist;

        // Calculate frustum corners in world space
        // Use fixed FOV (can be passed as parameter if needed)
        const float aspect = static_cast<float>(gbuffer_.extent.width) / static_cast<float>(gbuffer_.extent.height);
        const float tan_half_fov = std::tan(glm::radians(45.0f) * 0.5f);

        const float near_height = 2.0f * tan_half_fov * near_dist;
        const float near_width = near_height * aspect;
        const float far_height = 2.0f * tan_half_fov * far_dist;
        const float far_width = far_height * aspect;

        std::array<math::Vec3, 8> frustum_corners;

        // Near plane corners
        frustum_corners[0] = camera_pos + camera_forward * near_dist - camera_right * (near_width * 0.5f) - camera_up_corrected * (near_height * 0.5f);
        frustum_corners[1] = camera_pos + camera_forward * near_dist + camera_right * (near_width * 0.5f) - camera_up_corrected * (near_height * 0.5f);
        frustum_corners[2] = camera_pos + camera_forward * near_dist + camera_right * (near_width * 0.5f) + camera_up_corrected * (near_height * 0.5f);
        frustum_corners[3] = camera_pos + camera_forward * near_dist - camera_right * (near_width * 0.5f) + camera_up_corrected * (near_height * 0.5f);

        // Far plane corners
        frustum_corners[4] = camera_pos + camera_forward * far_dist - camera_right * (far_width * 0.5f) - camera_up_corrected * (far_height * 0.5f);
        frustum_corners[5] = camera_pos + camera_forward * far_dist + camera_right * (far_width * 0.5f) - camera_up_corrected * (far_height * 0.5f);
        frustum_corners[6] = camera_pos + camera_forward * far_dist + camera_right * (far_width * 0.5f) + camera_up_corrected * (far_height * 0.5f);
        frustum_corners[7] = camera_pos + camera_forward * far_dist - camera_right * (far_width * 0.5f) + camera_up_corrected * (far_height * 0.5f);

        // Transform frustum corners to light space
        math::Vec3 light_space_min(std::numeric_limits<float>::max());
        math::Vec3 light_space_max(std::numeric_limits<float>::lowest());

        for (const auto& corner : frustum_corners) {
            const math::Vec4 light_space_corner = light_view * math::Vec4(corner, 1.0f);
            light_space_min = glm::min(light_space_min, math::Vec3(light_space_corner));
            light_space_max = glm::max(light_space_max, math::Vec3(light_space_corner));
        }

        // Extend the Z range to include shadow casters behind the frustum
        const float z_extension = light_space_max.z - light_space_min.z;
        light_space_min.z -= z_extension * 2.0f;  // Extend backwards to capture shadow casters

        // Snap shadow map to texel-sized increments to reduce flickering
        const float world_units_per_texel = (light_space_max.x - light_space_min.x) / static_cast<float>(shadow_config_.cascade_resolution);

        light_space_min.x = std::floor(light_space_min.x / world_units_per_texel) * world_units_per_texel;
        light_space_min.y = std::floor(light_space_min.y / world_units_per_texel) * world_units_per_texel;
        light_space_max.x = std::floor(light_space_max.x / world_units_per_texel) * world_units_per_texel;
        light_space_max.y = std::floor(light_space_max.y / world_units_per_texel) * world_units_per_texel;

        // Create orthographic projection for this cascade
        const math::Mat4 light_proj = glm::ortho(
            light_space_min.x, light_space_max.x,
            light_space_min.y, light_space_max.y,
            light_space_min.z, light_space_max.z
        );

        // Store final light-space view-projection matrix
        cascade.view_proj_matrix = light_proj * light_view;

        LOG_DEBUG(Graphics, "Cascade {} bounds: [{}, {}] x [{}, {}] x [{}, {}]",
                  cascade_idx,
                  light_space_min.x, light_space_max.x,
                  light_space_min.y, light_space_max.y,
                  light_space_min.z, light_space_max.z);
    }

    LOG_DEBUG(Graphics, "Shadow cascades updated successfully");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shadow Pass Rendering
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::begin_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_index) {
    if (cascade_index >= shadow_maps_.active_cascades) {
        LOG_ERROR(Graphics, "Invalid cascade index: {} (active cascades: {})",
                  cascade_index, shadow_maps_.active_cascades);
        return;
    }

    auto& cascade = shadow_maps_.cascades[cascade_index];

    // Clear depth buffer
    VkClearValue clear_value{};
    clear_value.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = shadow_maps_.shadow_render_pass;
    render_pass_info.framebuffer = cascade.framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {shadow_config_.cascade_resolution, shadow_config_.cascade_resolution};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind shadow pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_maps_.shadow_pipeline);

    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadow_config_.cascade_resolution);
    viewport.height = static_cast<float>(shadow_config_.cascade_resolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {shadow_config_.cascade_resolution, shadow_config_.cascade_resolution};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Set depth bias dynamically based on configuration
    vkCmdSetDepthBias(
        cmd,
        shadow_config_.shadow_bias,
        0.0f,  // Bias clamp
        shadow_config_.shadow_slope_bias
    );

    // Push light-space view-projection matrix as push constant
    vkCmdPushConstants(
        cmd,
        shadow_maps_.shadow_pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(math::Mat4),
        &cascade.view_proj_matrix
    );

    LOG_DEBUG(Graphics, "Shadow pass begun for cascade {}", cascade_index);
}

void DeferredRenderer::end_shadow_pass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
    LOG_DEBUG(Graphics, "Shadow pass ended");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shadow Configuration Management
// ═══════════════════════════════════════════════════════════════════════════════

void DeferredRenderer::set_shadow_config(const ShadowConfig& config) {
    LOG_INFO(Graphics, "Updating shadow configuration ({} cascades, {}x{} resolution, quality: {})",
             config.cascade_count, config.cascade_resolution, config.cascade_resolution,
             static_cast<uint32_t>(config.quality));

    // Check if we need to recreate shadow resources
    const bool needs_recreation =
        (config.cascade_count != shadow_config_.cascade_count) ||
        (config.cascade_resolution != shadow_config_.cascade_resolution);

    shadow_config_ = config;

    if (needs_recreation && initialized_) {
        LOG_INFO(Graphics, "Shadow configuration changed, recreating shadow resources");

        // Wait for device idle before destroying resources
        vkDeviceWaitIdle(device_);

        // Destroy old shadow resources
        destroy_shadow_maps();

        // Recreate with new configuration
        create_shadow_pipeline();
        create_shadow_maps();
        create_shadow_sampler();
        update_shadow_descriptor_set();

        LOG_INFO(Graphics, "Shadow resources recreated successfully");
    } else {
        LOG_DEBUG(Graphics, "Shadow configuration updated (no resource recreation needed)");
    }
}

} // namespace lore::graphics
// ═══════════════════════════════════════════════════════════════════════════════
// Post-Processing Integration
// ═══════════════════════════════════════════════════════════════════════════════

#include <lore/graphics/post_process_pipeline.hpp>

void DeferredRenderer::create_hdr_buffer(VkExtent2D extent) {
    // Create HDR buffer for post-processing (RGBA16F format)
    hdr_buffer_ = create_attachment(
        extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    LOG_INFO(Graphics, "Created HDR buffer for post-processing ({}x{})", extent.width, extent.height);
}

void DeferredRenderer::destroy_hdr_buffer() {
    destroy_attachment(hdr_buffer_);
}

void DeferredRenderer::set_post_process_config(const PostProcessConfig& config) {
    if (post_process_pipeline_) {
        post_process_pipeline_->set_config(config);
    }
}

const PostProcessConfig& DeferredRenderer::get_post_process_config() const {
    if (post_process_pipeline_) {
        return post_process_pipeline_->get_config();
    }
    static PostProcessConfig default_config;
    return default_config;
}

PostProcessConfig& DeferredRenderer::get_post_process_config_mut() {
    if (!post_process_pipeline_) {
        throw std::runtime_error("Post-processing pipeline not initialized");
    }
    // Return mutable reference through const_cast (safe because we own the pipeline)
    return const_cast<PostProcessConfig&>(post_process_pipeline_->get_config());
}

