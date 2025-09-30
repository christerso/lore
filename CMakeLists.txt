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

# Configure SUPER STRICT compiler flags for MSVC
if(MSVC)
    # Enable C++23 features with proper flag
    add_compile_options(/std:c++latest)
    add_compile_options(/permissive-)
    add_compile_options(/Zc:__cplusplus)

    # STRICT warnings and treat warnings as errors for our code
    add_compile_options(/W4)         # Enable level 4 warnings (maximum for practical use)
    add_compile_options(/WX)         # Treat warnings as errors
    add_compile_options(/we4996)     # Treat deprecation warnings as errors
    add_compile_options(/we4267)     # Treat size_t conversion warnings as errors
    add_compile_options(/we4244)     # Treat type conversion warnings as errors
    add_compile_options(/we4100)     # Treat unused parameter warnings as errors
    add_compile_options(/we4101)     # Treat unused variable warnings as errors
    add_compile_options(/we4189)     # Treat local variable initialized but not referenced as errors
    add_compile_options(/we4018)     # Treat signed/unsigned mismatch as errors
    add_compile_options(/we4389)     # Treat signed/unsigned comparison as errors
    add_compile_options(/we4245)     # Treat signed/unsigned conversion as errors
    add_compile_options(/we4706)     # Treat assignment within conditional as errors
    add_compile_options(/we4715)     # Treat not all control paths return value as errors
    add_compile_options(/we4701)     # Treat potentially uninitialized variable as errors
    add_compile_options(/we4703)     # Treat potentially uninitialized pointer as errors

    # Code quality warnings as errors
    add_compile_options(/we4456)     # Declaration hides previous local declaration
    add_compile_options(/we4457)     # Declaration hides function parameter
    add_compile_options(/we4458)     # Declaration hides class member
    add_compile_options(/we4459)     # Declaration hides global declaration
    add_compile_options(/we4505)     # Unreferenced local function has been removed
    add_compile_options(/we4702)     # Unreachable code
    add_compile_options(/we4390)     # Empty controlled statement found

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
        # Override all warning settings for third-party code
        set_target_properties(${target} PROPERTIES
            COMPILE_OPTIONS "/w"  # Disable ALL warnings completely
        )
        # Additional override to ensure no warnings are treated as errors
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endfunction()

# Function to disable third-party warnings when including external headers
function(target_disable_external_warnings target)
    if(MSVC)
        # Disable specific warnings that come from external libraries
        target_compile_options(${target} PRIVATE /wd4464)  # Relative include path contains '..'
        target_compile_options(${target} PRIVATE /wd4005)  # Macro redefinition
        target_compile_options(${target} PRIVATE /wd4820)  # Padding added to struct
        target_compile_options(${target} PRIVATE /wd4625)  # Copy constructor implicitly deleted
        target_compile_options(${target} PRIVATE /wd4626)  # Assignment operator implicitly deleted
        target_compile_options(${target} PRIVATE /wd4250)  # Diamond inheritance issues
        target_compile_options(${target} PRIVATE /wd4251)  # DLL interface issues
        target_compile_options(${target} PRIVATE /wd4514)  # Unreferenced inline function removed
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
    GIT_SHALLOW TRUE
    OPTIONS
        "GLM_ENABLE_CXX_23 ON"
        "BUILD_SHARED_LIBS OFF"
        "GLM_DISABLE_AUTO_DETECTION ON"
        "BUILD_TESTING OFF"
        "GLM_BUILD_TESTS OFF"
)

# Disable GLM SIMD to avoid conflicts with our custom SIMD
if(TARGET glm)
    target_compile_definitions(glm INTERFACE GLM_FORCE_PURE)
    target_compile_definitions(glm INTERFACE GLM_FORCE_MESSAGES)
endif()

# miniaudio for audio
CPMAddPackage(
    NAME miniaudio
    GITHUB_REPOSITORY mackron/miniaudio
    GIT_TAG 0.11.21
    DOWNLOAD_ONLY YES
)

if(miniaudio_ADDED)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})
endif()

# miniz for compression (needed by tinyexr)
# Using header-only mode via direct download
CPMAddPackage(
    NAME miniz
    GITHUB_REPOSITORY richgel999/miniz
    GIT_TAG 3.0.2
    DOWNLOAD_ONLY YES
)

# Create header-only interface library for miniz
if(miniz_ADDED)
    add_library(miniz INTERFACE)
    target_include_directories(miniz INTERFACE ${miniz_SOURCE_DIR})
    target_compile_definitions(miniz INTERFACE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
endif()

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

# Core engine libraries
add_subdirectory(src/ecs)
add_subdirectory(src/math)
add_subdirectory(src/physics)
add_subdirectory(src/audio)
# add_subdirectory(src/assets)  # TODO: Fix assets system compilation issues
add_subdirectory(src/graphics)

# Main executable
add_executable(lore src/main.cpp)

# Link libraries in proper dependency order
target_link_libraries(lore PRIVATE
    lore_graphics    # Graphics system (depends on other systems)
    # lore_assets      # Asset system (depends on ECS and Math) - TODO: Fix compilation
    lore_audio       # Audio system (depends on ECS and Math)
    lore_physics     # Physics system (depends on ECS and Math)
    lore_math        # Math system (depends on GLM)
    lore_ecs         # ECS system (base dependency)
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    glm::glm
    miniaudio
)

# Ensure static linking for the main executable
set_target_properties(lore PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Disable third-party warnings for our targets that use external libraries
target_disable_external_warnings(lore_math)
target_disable_external_warnings(lore_ecs)
# target_disable_external_warnings(lore_assets)  # TODO: Uncomment when assets system is fixed
target_disable_external_warnings(lore_graphics)
target_disable_external_warnings(lore)

# Function to compile GLSL shaders to SPIR-V
function(compile_shader SHADER_SOURCE SHADER_OUTPUT)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)

    # Use glslangValidator from VulkanSDK
    find_program(GLSLANG_VALIDATOR glslangValidator HINTS "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" PATHS "${VULKAN_SDK}/Bin" ENV VULKAN_SDK)

    if(NOT GLSLANG_VALIDATOR)
        message(FATAL_ERROR "glslangValidator not found. Please install Vulkan SDK.")
    endif()

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders/compiled
        COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
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

# Copy shaders to executable directory after compilation
add_custom_command(TARGET lore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:lore>/shaders/compiled
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VERTEX_SHADER_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FRAGMENT_SHADER_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/
    COMMENT "Copying compiled shaders to executable directory"
)

# Copy assets to executable directory after compilation
add_custom_command(TARGET lore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:lore>/assets
    COMMENT "Copying assets to executable directory"
)

# Make main executable depend on shader compilation
add_dependencies(lore compile_shaders)