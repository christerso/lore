# GPU Audio Compute Shaders CMake Configuration

# Find required tools
find_program(GLSLC glslc HINTS ${Vulkan_GLSLC_EXECUTABLE})

if(NOT GLSLC)
    message(FATAL_ERROR "Could not find glslc shader compiler")
endif()

# Define shader source directory
set(AUDIO_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(AUDIO_SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# List of audio compute shaders to compile
set(AUDIO_COMPUTE_SHADERS
    gpu_audio_hrtf.comp
    gpu_audio_mixing.comp
    gpu_arena_allocator.comp
    gpu_arena_deallocator.comp
    gpu_ecs_transform.comp
)

# Create output directory for compiled shaders
file(MAKE_DIRECTORY ${AUDIO_SHADER_BINARY_DIR})

# Function to compile a single shader
function(compile_audio_shader SHADER_NAME)
    set(SHADER_SOURCE ${AUDIO_SHADER_SOURCE_DIR}/${SHADER_NAME})
    set(SHADER_BINARY ${AUDIO_SHADER_BINARY_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SHADER_BINARY}
        COMMAND ${GLSLC}
            -O                              # Optimize for performance
            --target-env=vulkan1.2         # Target Vulkan 1.2
            -g                              # Include debug information
            -DGPU_AUDIO_WORKGROUP_SIZE=64   # Define workgroup size
            ${SHADER_SOURCE}
            -o ${SHADER_BINARY}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling audio compute shader: ${SHADER_NAME}"
        VERBATIM
    )

    # Add to list of compiled shaders
    set_property(GLOBAL APPEND PROPERTY AUDIO_COMPILED_SHADERS ${SHADER_BINARY})
endfunction()

# Compile all audio compute shaders
foreach(SHADER ${AUDIO_COMPUTE_SHADERS})
    compile_audio_shader(${SHADER})
endforeach()

# Get list of compiled shaders
get_property(COMPILED_AUDIO_SHADERS GLOBAL PROPERTY AUDIO_COMPILED_SHADERS)

# Create target for audio shader compilation
add_custom_target(audio_shaders ALL
    DEPENDS ${COMPILED_AUDIO_SHADERS}
    COMMENT "Building audio compute shaders"
)

# Install compiled shaders to assets directory
install(FILES ${COMPILED_AUDIO_SHADERS}
    DESTINATION assets/shaders/audio
    COMPONENT shaders
)

# Optional: Copy shaders to output directory for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_command(TARGET audio_shaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${AUDIO_SHADER_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/assets/shaders/audio
        COMMENT "Copying audio shaders to output directory"
    )
endif()

# Add dependency to main target
if(TARGET lore)
    add_dependencies(lore audio_shaders)
endif()