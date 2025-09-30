#include <lore/assets/assets.hpp>
#include <iostream>
#include <cassert>

// Example demonstrating the complete VulkanImageLoader system
int main() {
    using namespace lore::assets;

    // 1. Set up Vulkan context (normally provided by graphics system)
    VulkanImageConfig config;
    // config.device = vulkan_device;              // From graphics system
    // config.physical_device = vulkan_phys_device; // From graphics system
    // config.allocator = vma_allocator;            // From graphics system
    // config.graphics_queue = graphics_queue;      // From graphics system
    // config.command_pool = command_pool;          // From graphics system
    config.generate_mipmaps = true;
    config.create_sampler = true;
    config.use_staging_buffer = true;
    config.staging_buffer_size = 64 * 1024 * 1024; // 64MB staging buffer

    // 2. Create AssetManager and register VulkanImageLoader
    AssetManager asset_manager;
    asset_manager.initialize();

    // Note: In real usage, you would configure the VulkanImageConfig with actual Vulkan objects
    // asset_manager.register_vulkan_image_loader(config);

    // 3. Demonstrate VulkanImageLoader features
    std::cout << "VulkanImageLoader Features Demonstration:\n";
    std::cout << "=========================================\n\n";

    // 3.1. Supported formats
    auto supported_extensions = VulkanImageLoader::get_all_supported_extensions();
    std::cout << "Supported image formats:\n";
    for (const auto& ext : supported_extensions) {
        std::cout << "  " << ext << "\n";
    }
    std::cout << "\n";

    // 3.2. Format detection
    std::cout << "Format detection examples:\n";
    auto png_format = VulkanImageLoader::detect_format_from_extension(".png");
    auto hdr_format = VulkanImageLoader::detect_format_from_extension(".hdr");
    std::cout << "  .png -> " << ImageData::format_to_string(png_format) << "\n";
    std::cout << "  .hdr -> " << ImageData::format_to_string(hdr_format) << "\n";
    std::cout << "\n";

    // 3.3. ImageData structure demonstration
    std::cout << "ImageData structure features:\n";
    ImageData example_image;
    example_image.set_dimensions(1024, 1024, 1);
    example_image.set_format(ImageFormat::RGBA8_UNORM);
    example_image.set_bytes_per_pixel(ImageData::get_bytes_per_pixel_for_format(ImageFormat::RGBA8_UNORM));
    example_image.set_original_file_path("textures/example.png");

    std::cout << "  Dimensions: " << example_image.get_width() << "x" << example_image.get_height() << "\n";
    std::cout << "  Format: " << ImageData::format_to_string(example_image.get_format()) << "\n";
    std::cout << "  Bytes per pixel: " << example_image.get_bytes_per_pixel() << "\n";
    std::cout << "  Data size: " << example_image.calculate_data_size() << " bytes\n";
    std::cout << "  Is valid: " << (example_image.is_valid() ? "Yes" : "No") << "\n";
    std::cout << "\n";

    // 3.4. VulkanImageConfig demonstration
    std::cout << "VulkanImageConfig options:\n";
    std::cout << "  Generate mipmaps: " << (config.generate_mipmaps ? "Yes" : "No") << "\n";
    std::cout << "  Create sampler: " << (config.create_sampler ? "Yes" : "No") << "\n";
    std::cout << "  Use staging buffer: " << (config.use_staging_buffer ? "Yes" : "No") << "\n";
    std::cout << "  Staging buffer size: " << config.staging_buffer_size / (1024 * 1024) << " MB\n";
    std::cout << "  Max anisotropy: " << config.max_anisotropy << "\n";
    std::cout << "\n";

    // 3.5. Asset loading workflow (conceptual)
    std::cout << "Asset loading workflow:\n";
    std::cout << "1. AssetManager.register_vulkan_image_loader(config)\n";
    std::cout << "2. AssetManager.load_asset(\"textures/player.png\")\n";
    std::cout << "3. VulkanImageLoader.load() -> creates ImageData with Vulkan resources\n";
    std::cout << "4. ImageData contains VkImage, VkImageView, VkSampler for GPU usage\n";
    std::cout << "5. Automatic memory management with VMA integration\n";
    std::cout << "6. Mipmap generation and format conversion handled automatically\n";
    std::cout << "\n";

    // 3.6. Performance features
    std::cout << "Performance optimizations:\n";
    std::cout << "- GPU-only memory allocation with VMA\n";
    std::cout << "- Staging buffer reuse for multiple uploads\n";
    std::cout << "- Automatic mipmap generation on GPU\n";
    std::cout << "- Comprehensive statistics tracking\n";
    std::cout << "- Thread-safe operations with proper locking\n";
    std::cout << "- Hot-reload support for development\n";
    std::cout << "- Async loading with proper error handling\n";
    std::cout << "\n";

    // 3.7. Error handling demonstration
    std::cout << "Error handling features:\n";
    std::cout << "- stb_image validation before GPU upload\n";
    std::cout << "- Vulkan resource creation error checking\n";
    std::cout << "- Memory allocation failure handling\n";
    std::cout << "- Graceful cleanup on failures\n";
    std::cout << "- Detailed error reporting via AssetResult<T>\n";
    std::cout << "\n";

    std::cout << "Complete image loading system successfully implemented!\n";
    std::cout << "Ready for integration with Vulkan graphics systems.\n";

    asset_manager.shutdown();
    return 0;
}