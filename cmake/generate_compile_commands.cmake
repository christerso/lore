# Generate comprehensive compile_commands.json for all C++ source files
# This script creates a compile_commands.json that includes ALL source files,
# not just the ones being compiled by CMake

# Basic compiler settings for C++23
set(CXX_STANDARD "c++23")
set(COMPILER "clang++")

# Define include directories
set(INCLUDE_DIRS
    "${SOURCE_DIR}/include"
    "${SOURCE_DIR}/src"
    "${BINARY_DIR}/include"
    "${BINARY_DIR}/external"
    "${BINARY_DIR}/_deps/glfw-src/include"
    "${BINARY_DIR}/_deps/glm-src"
    "${BINARY_DIR}/_deps/vk-bootstrap-src/src"
    "${BINARY_DIR}/_deps/nlohmann_json-src/include"
    "${BINARY_DIR}/_deps/vulkanmemoryallocator-src/include"
    "${BINARY_DIR}/_deps/spirv-reflect-src/include"
    "${BINARY_DIR}/_deps/stb-src"
    "${BINARY_DIR}/_deps/miniaudio-src"
    "C:/VulkanSDK/1.3.283.1/Include"
)

# Define compiler flags
set(COMPILE_FLAGS
    "-std=${CXX_STANDARD}"
    "-Wall"
    "-Wextra"
    "-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1"
    "-DWIN32"
    "-D_WINDOWS"
    "-DLORE_GRAPHICS_VULKAN=1"
    "-DGLFW_INCLUDE_VULKAN"
    "-DSTB_IMAGE_IMPLEMENTATION"
    "-DMINIAUDIO_IMPLEMENTATION"
    "-DVMA_IMPLEMENTATION"
    "-DVK_USE_PLATFORM_WIN32_KHR"
    "-DNOMINMAX"
    "-DWIN32_LEAN_AND_MEAN"
    "-D_CRT_SECURE_NO_WARNINGS"
)

# Build include string
set(INCLUDE_STRING "")
foreach(dir ${INCLUDE_DIRS})
    if(EXISTS "${dir}")
        set(INCLUDE_STRING "${INCLUDE_STRING} -I${dir}")
    endif()
endforeach()

# Build flags string
set(FLAGS_STRING "")
foreach(flag ${COMPILE_FLAGS})
    set(FLAGS_STRING "${FLAGS_STRING} ${flag}")
endforeach()

# Find all C++ source files
file(GLOB_RECURSE ALL_CPP_FILES
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/src/*.cc"
    "${SOURCE_DIR}/src/*.cxx"
)

# Start building the JSON content
set(JSON_CONTENT "[\n")
set(FIRST_ENTRY TRUE)

foreach(cpp_file ${ALL_CPP_FILES})
    # Skip files in build directory
    string(FIND "${cpp_file}" "${BINARY_DIR}" BUILD_DIR_POS)
    if(BUILD_DIR_POS EQUAL -1)
        if(NOT FIRST_ENTRY)
            set(JSON_CONTENT "${JSON_CONTENT},\n")
        endif()
        set(FIRST_ENTRY FALSE)

        # Normalize path separators for JSON
        string(REPLACE "\\" "/" cpp_file_normalized "${cpp_file}")
        string(REPLACE "\\" "/" source_dir_normalized "${SOURCE_DIR}")

        # Create JSON entry
        set(JSON_CONTENT "${JSON_CONTENT}  {\n")
        set(JSON_CONTENT "${JSON_CONTENT}    \"directory\": \"${source_dir_normalized}\",\n")
        set(JSON_CONTENT "${JSON_CONTENT}    \"command\": \"${COMPILER}${FLAGS_STRING}${INCLUDE_STRING} -c ${cpp_file_normalized}\",\n")
        set(JSON_CONTENT "${JSON_CONTENT}    \"file\": \"${cpp_file_normalized}\"\n")
        set(JSON_CONTENT "${JSON_CONTENT}  }")
    endif()
endforeach()

set(JSON_CONTENT "${JSON_CONTENT}\n]\n")

# Write the JSON file
set(OUTPUT_FILE "${SOURCE_DIR}/compile_commands.json")
file(WRITE "${OUTPUT_FILE}" "${JSON_CONTENT}")

message(STATUS "Generated comprehensive compile_commands.json with ${list_length} entries at ${OUTPUT_FILE}")