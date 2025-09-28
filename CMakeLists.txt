cmake_minimum_required(VERSION 3.28)

# Set C++23 standard - use latest for proper C++23 support
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force Visual Studio 2026 toolset
set(CMAKE_GENERATOR_TOOLSET "v143")
set(CMAKE_VS_PLATFORM_TOOLSET "v143")

project(Lore VERSION 1.0.0 LANGUAGES CXX)

# Force static linking for all libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# CPM (CMake Package Manager) setup
include(cmake/CPM.cmake)

# Configure strict compiler flags for MSVC
if(MSVC)
    # Enable C++23 features with proper flag
    add_compile_options(/std:c++latest)
    add_compile_options(/permissive-)
    add_compile_options(/Zc:__cplusplus)

    # Maximum warnings and treat as errors for our code
    add_compile_options(/W4)
    add_compile_options(/WX)  # Treat warnings as errors

    # Unicode support
    add_compile_options(/utf-8)
    add_compile_options(/DUNICODE)
    add_compile_options(/D_UNICODE)

    # Static linking flags
    add_compile_options(/MT$<$<CONFIG:Debug>:d>)

    # Performance optimizations for Release
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)  # Whole program optimization
    add_compile_options($<$<CONFIG:Release>:/DNDEBUG>)

    # Debug optimizations
    add_compile_options($<$<CONFIG:Debug>:/Od>)
    add_compile_options($<$<CONFIG:Debug>:/Zi>)
    add_compile_options($<$<CONFIG:Debug>:/RTC1>)
    add_compile_options($<$<CONFIG:Debug>:/D_DEBUG>)

    # Link time code generation for Release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
endif()

# Function to disable warnings for third-party libraries
function(target_disable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /w)  # Disable all warnings
    endif()
endfunction()

# Find Vulkan (required)
find_package(Vulkan REQUIRED)

# Add dependencies via CPM
CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "GLFW_INSTALL OFF"
        "GLFW_USE_HYBRID_HPG ON"
)

# VkBootstrap for easy Vulkan setup
CPMAddPackage(
    NAME vk-bootstrap
    GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
    GIT_TAG v1.3.283
)

# GLM for mathematics
CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
    OPTIONS
        "GLM_ENABLE_CXX_23 ON"
        "BUILD_SHARED_LIBS OFF"
)

# Disable warnings for all third-party libraries
if(TARGET glfw)
    target_disable_warnings(glfw)
endif()
if(TARGET vk-bootstrap)
    target_disable_warnings(vk-bootstrap)
endif()
if(TARGET glm)
    target_disable_warnings(glm)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Graphics library
add_subdirectory(src/graphics)

# Main executable
add_executable(lore src/main.cpp)

# Link libraries
target_link_libraries(lore PRIVATE
    lore_graphics
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    glm::glm
)

# Ensure static linking for the main executable
set_target_properties(lore PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Function to compile GLSL shaders to SPIR-V
function(compile_shader SHADER_SOURCE SHADER_OUTPUT)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders/compiled
        COMMAND glslangValidator -V ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
        VERBATIM
    )
endfunction()

# Create shader directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders/compiled)

# Compile individual shaders
set(VERTEX_SHADER_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/triangle_vert.spv)
set(FRAGMENT_SHADER_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/triangle_frag.spv)

compile_shader(${CMAKE_SOURCE_DIR}/shaders/triangle.vert ${VERTEX_SHADER_SPV})
compile_shader(${CMAKE_SOURCE_DIR}/shaders/triangle.frag ${FRAGMENT_SHADER_SPV})

# Create target for all compiled shaders
add_custom_target(compile_shaders ALL
    DEPENDS ${VERTEX_SHADER_SPV} ${FRAGMENT_SHADER_SPV}
    COMMENT "Building all shaders"
)

# Make main executable depend on shader compilation
add_dependencies(lore compile_shaders)