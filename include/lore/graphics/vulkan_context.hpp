#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace lore {

/**
 * Minimal Vulkan context for resource creation
 * Contains the essential Vulkan handles needed for HDRIEnvironment
 */
struct VulkanContext {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = 0;
};

} // namespace lore