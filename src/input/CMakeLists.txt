# Input System Library
# Provides thread-safe input handling with GLFW integration

set(INPUT_SOURCES
    input.cpp
    input_ecs.cpp
)

set(INPUT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/lore/input/input.hpp
)

# Create the input system library
add_library(lore_input STATIC ${INPUT_SOURCES} ${INPUT_HEADERS})

# Set target properties
set_target_properties(lore_input PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Include directories
target_include_directories(lore_input PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(lore_input PUBLIC
    lore_ecs     # ECS dependency
    glfw         # GLFW for input handling
    glm::glm     # GLM for vector math
)

# Apply warning suppression for external libraries
target_disable_external_warnings(lore_input)

# Organize files in IDE
source_group("Header Files" FILES ${INPUT_HEADERS})
source_group("Source Files" FILES ${INPUT_SOURCES})