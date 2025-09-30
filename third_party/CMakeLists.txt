# Third-Party Dependencies
# This file manages all external dependencies for the Lore engine
# These libraries are built once and cached, only rebuilding when changed

# GLFW for windowing
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_USE_HYBRID_HPG ON CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# VkBootstrap for easy Vulkan setup
add_subdirectory(vk-bootstrap)

# GLM for mathematics (from third_party submodule)
set(GLM_ENABLE_CXX_23 ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLM_DISABLE_AUTO_DETECTION ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)

# miniaudio for audio (from third_party submodule - header only)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)

# miniz for compression (needed by tinyexr) - header only from third_party submodule
add_library(miniz INTERFACE)
target_include_directories(miniz INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniz)
target_compile_definitions(miniz INTERFACE
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES
    MINIZ_EXPORT=
    MINIZ_NO_EXPORT=)

# VulkanMemoryAllocator - header only from third_party submodule
add_library(VulkanMemoryAllocator INTERFACE)
target_include_directories(VulkanMemoryAllocator INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)

# SPIRV-Reflect from third_party submodule - enable static library
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Reflect)

# Create an interface library alias for easier usage
add_library(SPIRV-Reflect INTERFACE)
target_link_libraries(SPIRV-Reflect INTERFACE spirv-reflect-static)
target_include_directories(SPIRV-Reflect INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect)

# nlohmann/json from third_party submodule
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
add_subdirectory(json)

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
