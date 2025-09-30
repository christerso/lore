# Graphics library
add_library(lore_graphics STATIC
    graphics.cpp
    hdri_environment.cpp
    vma_implementation.cpp
    deferred_renderer.cpp
    gpu_compute.cpp
    post_process_pipeline.cpp
)

# Link required libraries
target_link_libraries(lore_graphics PUBLIC
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    VulkanMemoryAllocator
    SPIRV-Reflect
    miniz
    glm::glm
)

# Include directories
target_include_directories(lore_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tinyexr
)

# Disable warnings for this library if it's third-party code
target_disable_warnings(lore_graphics)
