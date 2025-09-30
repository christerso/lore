/**
 * SHADOW MAPPING SYSTEM - C++ USAGE EXAMPLE
 * ===========================================
 *
 * This example demonstrates how to integrate the shadow mapping system
 * into your Vulkan application using the Lore Engine deferred renderer.
 *
 * File: examples/shadow_system_example.cpp
 */

#include <lore/graphics/deferred_renderer.hpp>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>

namespace lore {

// ═══════════════════════════════════════════════════════════════════════════
// SHADOW UNIFORM BUFFER STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Shadow UBO Structure (must match GLSL layout exactly!)
 * Binding: Set 2, Binding 4
 */
struct ShadowUBO {
    glm::mat4 cascadeViewProj[4];  // 64 bytes × 4 = 256 bytes
    glm::vec4 cascadeSplits;       // 16 bytes (total: 272)
    glm::vec4 shadowParams;        // 16 bytes (total: 288)
    glm::vec4 shadowSettings;      // 16 bytes (total: 304)
    glm::vec3 lightDirection;      // 12 bytes
    float padding;                 // 4 bytes (total: 320 bytes)
};
static_assert(sizeof(ShadowUBO) == 320, "ShadowUBO size must match shader layout!");

// ═══════════════════════════════════════════════════════════════════════════
// SHADOW MAP MANAGER CLASS
// ═══════════════════════════════════════════════════════════════════════════

class ShadowMapManager {
public:
    // Configuration
    struct Config {
        uint32_t shadowMapResolution = 2048;  // Resolution per cascade (2K default)
        uint32_t numCascades = 4;             // Number of cascades
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;  // 32-bit float depth
    };

    // Shadow quality presets
    enum class Quality {
        Low,     // 3×3 PCF, 1K resolution
        Medium,  // 5×5 PCF, 2K resolution (recommended)
        High,    // 7×7 PCF, 2K resolution
        Ultra    // Poisson, 4K resolution
    };

    ShadowMapManager(VkDevice device, VkPhysicalDevice physicalDevice, const Config& config);
    ~ShadowMapManager();

    // Setup and configuration
    void initialize();
    void cleanup();
    void setQuality(Quality quality);

    // Per-frame updates
    void updateCascades(const Camera& camera, const glm::vec3& lightDir);
    void beginShadowPass(VkCommandBuffer cmd, uint32_t cascadeIndex);
    void endShadowPass(VkCommandBuffer cmd);
    void renderObject(VkCommandBuffer cmd, const glm::mat4& modelMatrix);

    // Getters
    VkImageView getShadowMapView(uint32_t cascade) const { return m_shadowMapViews[cascade]; }
    VkSampler getShadowSampler() const { return m_shadowSampler; }
    const ShadowUBO& getUniformData() const { return m_shadowUBO; }
    VkBuffer getUniformBuffer() const { return m_uniformBuffer; }

private:
    // Vulkan resources
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    Config m_config;

    // Shadow maps (one per cascade)
    std::array<VkImage, 4> m_shadowMaps;
    std::array<VkDeviceMemory, 4> m_shadowMapMemory;
    std::array<VkImageView, 4> m_shadowMapViews;
    std::array<VkFramebuffer, 4> m_shadowFramebuffers;

    // Shadow sampler (comparison sampler)
    VkSampler m_shadowSampler;

    // Render pass for shadow depth rendering
    VkRenderPass m_shadowRenderPass;

    // Pipeline for shadow depth rendering
    VkPipeline m_shadowPipeline;
    VkPipelineLayout m_shadowPipelineLayout;

    // Uniform buffer
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferMemory;
    ShadowUBO m_shadowUBO;

    // Helper methods
    void createShadowMaps();
    void createShadowSampler();
    void createShadowRenderPass();
    void createShadowPipeline();
    void createUniformBuffer();
    glm::mat4 calculateCascadeViewProj(const Camera& camera, const glm::vec3& lightDir,
                                       float nearPlane, float farPlane);
};

// ═══════════════════════════════════════════════════════════════════════════
// IMPLEMENTATION: INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

ShadowMapManager::ShadowMapManager(VkDevice device, VkPhysicalDevice physicalDevice, const Config& config)
    : m_device(device), m_physicalDevice(physicalDevice), m_config(config) {
}

void ShadowMapManager::initialize() {
    createShadowMaps();
    createShadowSampler();
    createShadowRenderPass();
    createShadowPipeline();
    createUniformBuffer();

    // Set default quality to Medium
    setQuality(Quality::Medium);
}

void ShadowMapManager::createShadowMaps() {
    for (uint32_t i = 0; i < m_config.numCascades; ++i) {
        // Create depth image
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_config.depthFormat;
        imageInfo.extent.width = m_config.shadowMapResolution;
        imageInfo.extent.height = m_config.shadowMapResolution;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT;  // For sampling in shader
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCreateImage(m_device, &imageInfo, nullptr, &m_shadowMaps[i]);

        // Allocate memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, m_shadowMaps[i], &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(m_device, &allocInfo, nullptr, &m_shadowMapMemory[i]);
        vkBindImageMemory(m_device, m_shadowMaps[i], m_shadowMapMemory[i], 0);

        // Create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_shadowMaps[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_config.depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_device, &viewInfo, nullptr, &m_shadowMapViews[i]);
    }
}

void ShadowMapManager::createShadowSampler() {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // Linear filtering for smoother shadow edges
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    // Clamp to border (white = fully lit outside shadow map)
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    // Enable comparison mode for sampler2DShadow
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;  // Pass if fragment depth < shadow map depth

    // No mipmaps for shadow maps
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    // Anisotropic filtering not needed for depth
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    vkCreateSampler(m_device, &samplerInfo, nullptr, &m_shadowSampler);
}

void ShadowMapManager::createUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(ShadowUBO);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_uniformBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_uniformBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_uniformBufferMemory);
    vkBindBufferMemory(m_device, m_uniformBuffer, m_uniformBufferMemory, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// IMPLEMENTATION: QUALITY PRESETS
// ═══════════════════════════════════════════════════════════════════════════

void ShadowMapManager::setQuality(Quality quality) {
    switch (quality) {
        case Quality::Low:
            // 3×3 PCF, 1K resolution
            m_shadowUBO.shadowParams = glm::vec4(0.002f, 2.0f, 1.0f, 0.8f);
            m_shadowUBO.shadowSettings = glm::vec4(0.0f, 0.0f, 200.0f, 20.0f);
            break;

        case Quality::Medium:
            // 5×5 PCF, 2K resolution (recommended)
            m_shadowUBO.shadowParams = glm::vec4(0.002f, 2.0f, 1.5f, 0.8f);
            m_shadowUBO.shadowSettings = glm::vec4(1.0f, 0.0f, 200.0f, 20.0f);
            break;

        case Quality::High:
            // 7×7 PCF, 2K resolution
            m_shadowUBO.shadowParams = glm::vec4(0.002f, 2.0f, 2.0f, 0.8f);
            m_shadowUBO.shadowSettings = glm::vec4(2.0f, 0.0f, 200.0f, 20.0f);
            break;

        case Quality::Ultra:
            // Poisson disk, 4K resolution
            m_shadowUBO.shadowParams = glm::vec4(0.002f, 2.0f, 2.5f, 0.9f);
            m_shadowUBO.shadowSettings = glm::vec4(0.0f, 1.0f, 300.0f, 30.0f);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// IMPLEMENTATION: CASCADE CALCULATION
// ═══════════════════════════════════════════════════════════════════════════

void ShadowMapManager::updateCascades(const Camera& camera, const glm::vec3& lightDir) {
    // Define cascade split distances (exponential distribution)
    const float nearPlane = camera.getNearPlane();
    const float farPlane = camera.getFarPlane();

    // Logarithmic distribution for better quality
    std::array<float, 4> cascadeSplits;
    cascadeSplits[0] = nearPlane + (farPlane - nearPlane) * 0.05f;   // 5%
    cascadeSplits[1] = nearPlane + (farPlane - nearPlane) * 0.15f;   // 15%
    cascadeSplits[2] = nearPlane + (farPlane - nearPlane) * 0.40f;   // 40%
    cascadeSplits[3] = farPlane;                                      // 100%

    // Store in UBO
    m_shadowUBO.cascadeSplits = glm::vec4(
        cascadeSplits[0], cascadeSplits[1],
        cascadeSplits[2], cascadeSplits[3]
    );

    // Calculate view-projection matrix for each cascade
    float lastSplit = nearPlane;
    for (uint32_t i = 0; i < 4; ++i) {
        m_shadowUBO.cascadeViewProj[i] = calculateCascadeViewProj(
            camera, lightDir, lastSplit, cascadeSplits[i]
        );
        lastSplit = cascadeSplits[i];
    }

    // Store light direction
    m_shadowUBO.lightDirection = glm::normalize(lightDir);

    // Upload to GPU
    void* data;
    vkMapMemory(m_device, m_uniformBufferMemory, 0, sizeof(ShadowUBO), 0, &data);
    memcpy(data, &m_shadowUBO, sizeof(ShadowUBO));
    vkUnmapMemory(m_device, m_uniformBufferMemory);
}

glm::mat4 ShadowMapManager::calculateCascadeViewProj(
    const Camera& camera, const glm::vec3& lightDir,
    float nearPlane, float farPlane)
{
    // Get frustum corners for this cascade range
    std::array<glm::vec3, 8> frustumCorners =
        camera.getFrustumCornersWorldSpace(nearPlane, farPlane);

    // Calculate frustum center
    glm::vec3 center = glm::vec3(0.0f);
    for (const auto& corner : frustumCorners) {
        center += corner;
    }
    center /= static_cast<float>(frustumCorners.size());

    // Create light view matrix
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(
        center - lightDir * 100.0f,  // Position light far back
        center,                       // Look at frustum center
        up
    );

    // Calculate AABB of frustum in light space
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    for (const auto& corner : frustumCorners) {
        const glm::vec4 lightSpaceCorner = lightView * glm::vec4(corner, 1.0f);
        minX = std::min(minX, lightSpaceCorner.x);
        maxX = std::max(maxX, lightSpaceCorner.x);
        minY = std::min(minY, lightSpaceCorner.y);
        maxY = std::max(maxY, lightSpaceCorner.y);
        minZ = std::min(minZ, lightSpaceCorner.z);
        maxZ = std::max(maxZ, lightSpaceCorner.z);
    }

    // Extend Z range to include shadow casters behind frustum
    minZ -= 50.0f;

    // Create orthographic projection that tightly fits frustum
    const glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    // Snap to texel boundaries to reduce shimmering when camera moves
    const float shadowMapSize = static_cast<float>(m_config.shadowMapResolution);
    glm::mat4 shadowMatrix = lightProj * lightView;

    glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shadowOrigin *= shadowMapSize / 2.0f;

    glm::vec4 roundedOrigin = glm::round(shadowOrigin);
    glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
    roundOffset *= 2.0f / shadowMapSize;

    glm::mat4 snappedProj = lightProj;
    snappedProj[3] += roundOffset;

    return snappedProj * lightView;
}

// ═══════════════════════════════════════════════════════════════════════════
// IMPLEMENTATION: RENDERING
// ═══════════════════════════════════════════════════════════════════════════

void ShadowMapManager::beginShadowPass(VkCommandBuffer cmd, uint32_t cascadeIndex) {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_shadowRenderPass;
    renderPassInfo.framebuffer = m_shadowFramebuffers[cascadeIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_config.shadowMapResolution, m_config.shadowMapResolution};

    VkClearValue clearValue;
    clearValue.depthStencil = {1.0f, 0};  // Clear to max depth (far plane)

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);

    // Set viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_config.shadowMapResolution);
    viewport.height = static_cast<float>(m_config.shadowMapResolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {m_config.shadowMapResolution, m_config.shadowMapResolution};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void ShadowMapManager::endShadowPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void ShadowMapManager::renderObject(VkCommandBuffer cmd, const glm::mat4& modelMatrix) {
    // Push constants: light view-projection and model matrix
    struct PushConstants {
        glm::mat4 lightViewProj;
        glm::mat4 model;
    } pc;

    // lightViewProj was already set in updateCascades
    // Here we just push the model matrix
    pc.model = modelMatrix;

    vkCmdPushConstants(cmd, m_shadowPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(PushConstants), &pc);

    // Actual draw call would happen here (vkCmdDrawIndexed, etc.)
}

// ═══════════════════════════════════════════════════════════════════════════
// USAGE EXAMPLE IN MAIN RENDERING LOOP
// ═══════════════════════════════════════════════════════════════════════════

void renderFrame(ShadowMapManager& shadowManager,
                 const Camera& camera,
                 const std::vector<RenderObject>& objects) {

    VkCommandBuffer cmd = getCurrentCommandBuffer();

    // ───────────────────────────────────────────────────────────
    // STEP 1: Update shadow cascades based on camera
    // ───────────────────────────────────────────────────────────
    glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.3f));
    shadowManager.updateCascades(camera, lightDirection);

    // ───────────────────────────────────────────────────────────
    // STEP 2: Render shadow maps (one pass per cascade)
    // ───────────────────────────────────────────────────────────
    for (uint32_t cascade = 0; cascade < 4; ++cascade) {
        shadowManager.beginShadowPass(cmd, cascade);

        // Render all shadow-casting objects
        for (const auto& object : objects) {
            if (object.castsShadows) {
                shadowManager.renderObject(cmd, object.transform);
                // ... bind vertex/index buffers and draw
            }
        }

        shadowManager.endShadowPass(cmd);
    }

    // ───────────────────────────────────────────────────────────
    // STEP 3: Render main scene with deferred renderer
    // ───────────────────────────────────────────────────────────
    // Geometry pass (writes to G-Buffer)
    renderGeometryPass(cmd, camera, objects);

    // Lighting pass (reads G-Buffer and shadow maps)
    renderLightingPass(cmd, camera, shadowManager);
}

} // namespace lore

/**
 * KEY TAKEAWAYS:
 * ==============
 *
 * 1. Shadow maps are rendered in a separate depth-only pass BEFORE the main scene
 * 2. Four cascades provide high quality near camera, acceptable quality far away
 * 3. Cascade splits use logarithmic distribution for better quality distribution
 * 4. Texel snapping prevents shimmering when camera moves
 * 5. Comparison sampler (sampler2DShadow) performs hardware depth comparison
 * 6. Uniform buffer stores all cascade data and shadow parameters
 * 7. Quality presets allow easy switching between performance/quality
 *
 * PERFORMANCE TIPS:
 * =================
 *
 * - Use lower resolution for distant cascades (e.g., 2K/2K/1K/1K)
 * - Cull objects outside shadow frustum before rendering
 * - Use lower LOD models when rendering to shadow maps
 * - Cache shadow maps for static geometry
 * - Use front-face culling (render back faces only) to reduce shadow acne
 */