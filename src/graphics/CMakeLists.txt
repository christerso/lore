# Graphics library
add_library(lore_graphics STATIC
    graphics.cpp
)

# Link required libraries
target_link_libraries(lore_graphics PUBLIC
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    glm::glm
)

# Include directories
target_include_directories(lore_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Disable warnings for this library if it's third-party code
target_disable_warnings(lore_graphics)