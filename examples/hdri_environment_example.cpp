/**
 * HDRI Environment Example
 *
 * Demonstrates loading and using HDRI environments with the Lore Engine.
 * Shows three modes:
 * 1. Pure HDRI - Photorealistic image-based lighting
 * 2. Pure Atmospheric - Procedural sky simulation
 * 3. Hybrid - HDRI skybox with atmospheric fog overlay
 *
 * Uses the user's HDRIs from C:/Users/chris/Desktop/hdri/
 */

#include <lore/graphics/hdri_environment.hpp>
#include <lore/graphics/vulkan_context.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <chrono>

using namespace lore;

class HDRIDemo {
public:
    HDRIDemo() {
        init_vulkan();
        load_hdris();
    }

    ~HDRIDemo() {
        cleanup();
    }

    void run() {
        std::cout << "\n=== HDRI Environment Demo ===\n";
        std::cout << "Controls:\n";
        std::cout << "  1: Pure HDRI mode (photorealistic)\n";
        std::cout << "  2: Pure Atmospheric mode (procedural)\n";
        std::cout << "  3: Hybrid mode (HDRI + atmospheric fog)\n";
        std::cout << "  Q/E: Cycle through HDRIs\n";
        std::cout << "  -/+: Adjust HDRI intensity\n";
        std::cout << "  [/]: Adjust atmospheric blend (hybrid mode)\n";
        std::cout << "  ESC: Exit\n\n";

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            process_input();
            render_frame();
        }
    }

private:
    void init_vulkan() {
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1280, 720, "HDRI Environment Demo", nullptr, nullptr);

        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        // Store this pointer in window user data for callbacks
        glfwSetWindowUserPointer(window, this);

        // In a real application, you would initialize Vulkan here
        // For this example, we create a minimal VulkanContext
        // NOTE: This is a simplified example - see complete_rendering_demo.cpp for full Vulkan setup

        std::cout << "Vulkan context initialized (simplified example)\n";
    }

    void load_hdris() {
        std::cout << "\n=== Loading HDRIs ===\n";

        // Define HDRI paths from user's desktop
        const std::vector<std::string> hdri_paths = {
            "C:/Users/chris/Desktop/hdri/belfast_sunset_puresky_4k.exr",
            "C:/Users/chris/Desktop/hdri/citrus_orchard_road_puresky_4k.exr",
            "C:/Users/chris/Desktop/hdri/quarry_04_puresky_4k.exr",
            "C:/Users/chris/Desktop/hdri/qwantani_dusk_2_puresky_4k.exr"
        };

        // Use high quality preset for best visuals
        auto quality = HDRIQualityConfig::create_high();

        std::cout << "Quality settings:\n";
        std::cout << "  Environment cubemap: " << quality.environment_resolution << "×" << quality.environment_resolution << "\n";
        std::cout << "  Irradiance map: " << quality.irradiance_resolution << "×" << quality.irradiance_resolution << "\n";
        std::cout << "  Pre-filter mip levels: " << quality.prefiltered_mip_levels << "\n";
        std::cout << "  Sample counts: irradiance=" << quality.irradiance_sample_count
                  << ", prefilter=" << quality.prefilter_sample_count
                  << ", brdf=" << quality.brdf_sample_count << "\n\n";

        // Load each HDRI
        for (const auto& path : hdri_paths) {
            try {
                std::cout << "Loading: " << path << "\n";
                auto start = std::chrono::high_resolution_clock::now();

                // Load HDRI
                auto hdri = HDRIEnvironment::load_from_file(context, path, quality);

                // Generate IBL maps (pre-computation)
                // In a real application, you would get a command buffer from your renderer
                VkCommandBuffer cmd = nullptr; // Simplified - would be real command buffer
                hdri.generate_ibl_maps(context, cmd);

                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                std::cout << "  ✓ Loaded and processed in " << duration.count() << "ms\n";
                std::cout << "  Average luminance: " << hdri.calculate_average_luminance() << "\n\n";

                hdris.push_back(std::move(hdri));
            }
            catch (const std::exception& e) {
                std::cerr << "  ✗ Failed to load: " << e.what() << "\n\n";
            }
        }

        if (hdris.empty()) {
            throw std::runtime_error("No HDRIs loaded successfully");
        }

        std::cout << "Successfully loaded " << hdris.size() << " HDRI environments\n";
        current_hdri = 0;
    }

    void process_input() {
        // Mode selection
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key_pressed[GLFW_KEY_1]) {
            current_mode = EnvironmentMode::HDRI;
            std::cout << "Mode: Pure HDRI (photorealistic)\n";
            key_pressed[GLFW_KEY_1] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) key_pressed[GLFW_KEY_1] = false;

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key_pressed[GLFW_KEY_2]) {
            current_mode = EnvironmentMode::Atmospheric;
            std::cout << "Mode: Pure Atmospheric (procedural)\n";
            key_pressed[GLFW_KEY_2] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) key_pressed[GLFW_KEY_2] = false;

        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !key_pressed[GLFW_KEY_3]) {
            current_mode = EnvironmentMode::Hybrid;
            std::cout << "Mode: Hybrid (HDRI + atmospheric fog)\n";
            key_pressed[GLFW_KEY_3] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) key_pressed[GLFW_KEY_3] = false;

        // HDRI selection
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS && !key_pressed[GLFW_KEY_Q]) {
            current_hdri = (current_hdri + hdris.size() - 1) % hdris.size();
            std::cout << "HDRI: " << hdris[current_hdri].file_path() << "\n";
            key_pressed[GLFW_KEY_Q] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_RELEASE) key_pressed[GLFW_KEY_Q] = false;

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !key_pressed[GLFW_KEY_E]) {
            current_hdri = (current_hdri + 1) % hdris.size();
            std::cout << "HDRI: " << hdris[current_hdri].file_path() << "\n";
            key_pressed[GLFW_KEY_E] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE) key_pressed[GLFW_KEY_E] = false;

        // HDRI intensity
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            auto& params = hdris[current_hdri].params();
            params.intensity = std::max(0.1f, params.intensity - 0.05f);
            std::cout << "HDRI intensity: " << params.intensity << "\n";
        }

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
            auto& params = hdris[current_hdri].params();
            params.intensity = std::min(3.0f, params.intensity + 0.05f);
            std::cout << "HDRI intensity: " << params.intensity << "\n";
        }

        // Atmospheric blend (hybrid mode)
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            auto& params = hdris[current_hdri].params();
            params.atmospheric_blend = std::max(0.0f, params.atmospheric_blend - 0.05f);
            std::cout << "Atmospheric blend: " << params.atmospheric_blend << "\n";
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            auto& params = hdris[current_hdri].params();
            params.atmospheric_blend = std::min(1.0f, params.atmospheric_blend + 0.05f);
            std::cout << "Atmospheric blend: " << params.atmospheric_blend << "\n";
        }

        // Exit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void render_frame() {
        // In a real application, this would:
        // 1. Begin command buffer recording
        // 2. Update uniform buffers with environment data
        // 3. Render skybox using current HDRI cubemap
        // 4. Render scene geometry with IBL lighting
        // 5. Apply atmospheric effects if in hybrid mode
        // 6. Submit command buffer

        // Get current environment's UBO data
        auto& hdri = hdris[current_hdri];
        auto env_ubo = hdri.get_environment_ubo(current_mode);

        // Example: Upload UBO to GPU (simplified)
        // vkCmdUpdateBuffer(cmd, env_uniform_buffer, 0, sizeof(EnvironmentUBO), &env_ubo);

        // Example: Bind IBL descriptor set (simplified)
        // const auto& textures = hdri.textures();
        // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
        //                         3, 1, &ibl_descriptor_set, 0, nullptr);

        // Example: Draw skybox (simplified)
        // if (current_mode != EnvironmentMode::Atmospheric) {
        //     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline);
        //     vkCmdDraw(cmd, 36, 1, 0, 0); // Cube mesh
        // }

        // For this simplified example, just wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    void cleanup() {
        // Destroy HDRIs
        for (auto& hdri : hdris) {
            hdri.destroy(context);
        }

        // Cleanup window
        if (window) {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }

private:
    GLFWwindow* window = nullptr;
    VulkanContext context{};

    std::vector<HDRIEnvironment> hdris;
    size_t current_hdri = 0;
    EnvironmentMode current_mode = EnvironmentMode::Hybrid;

    // Key press tracking (debounce)
    std::map<int, bool> key_pressed;
};

int main() {
    try {
        HDRIDemo demo;
        demo.run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}

/*
 * Integration Notes:
 *
 * To integrate this into your existing renderer:
 *
 * 1. **Descriptor Set Layout** (set = 3 for IBL)
 *    ```cpp
 *    VkDescriptorSetLayoutBinding bindings[] = {
 *        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // irradiance
 *        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // prefiltered
 *        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // brdf LUT
 *        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}          // environment UBO
 *    };
 *    ```
 *
 * 2. **Update deferred_lighting.frag shader**
 *    Add IBL sampling code after line 1007 (see docs/systems/image_based_lighting.md)
 *
 * 3. **Skybox Rendering Pass**
 *    Add a skybox pass before or after deferred lighting
 *    Use environment cubemap as texture
 *    Render inverted cube with camera at center
 *
 * 4. **Hybrid Mode Blending**
 *    In shader:
 *    ```glsl
 *    vec3 final_color;
 *    if (environment.mode == 2) { // Hybrid
 *        vec3 ibl_color = /* IBL calculation */;
 *        vec3 atm_color = /* Atmospheric calculation */;
 *        final_color = mix(ibl_color, atm_color, environment.atmospheric_blend);
 *    }
 *    ```
 *
 * 5. **Performance Optimization**
 *    - Generate IBL maps once at startup or async during loading screen
 *    - Cache BRDF LUT (same for all HDRIs)
 *    - Use BC6H compression for cubemaps to save VRAM
 *    - Stream in HDRIs as needed (don't keep all loaded)
 */