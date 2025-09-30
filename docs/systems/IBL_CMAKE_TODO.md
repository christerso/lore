# CMakeLists.txt IBL Integration TODO

## Required Changes

Add the following to `CMakeLists.txt` after line 231 (after FRAGMENT_SHADER_SPV definition):

```cmake
# IBL compute shaders
set(IBL_EQUIRECT_TO_CUBEMAP_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/ibl/equirect_to_cubemap.spv)
set(IBL_IRRADIANCE_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/ibl/irradiance_convolution.spv)
set(IBL_PREFILTER_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/ibl/prefilter_environment.spv)
set(IBL_BRDF_LUT_SPV ${CMAKE_BINARY_DIR}/shaders/compiled/ibl/brdf_integration.spv)
```

Add after line 234 (after triangle.frag compilation):

```cmake
compile_shader(${CMAKE_SOURCE_DIR}/shaders/ibl/equirect_to_cubemap.comp ${IBL_EQUIRECT_TO_CUBEMAP_SPV})
compile_shader(${CMAKE_SOURCE_DIR}/shaders/ibl/irradiance_convolution.comp ${IBL_IRRADIANCE_SPV})
compile_shader(${CMAKE_SOURCE_DIR}/shaders/ibl/prefilter_environment.comp ${IBL_PREFILTER_SPV})
compile_shader(${CMAKE_SOURCE_DIR}/shaders/ibl/brdf_integration.comp ${IBL_BRDF_LUT_SPV})
```

Update line 238 to include IBL shaders:

```cmake
add_custom_target(compile_shaders ALL
    DEPENDS
        ${VERTEX_SHADER_SPV}
        ${FRAGMENT_SHADER_SPV}
        ${IBL_EQUIRECT_TO_CUBEMAP_SPV}
        ${IBL_IRRADIANCE_SPV}
        ${IBL_PREFILTER_SPV}
        ${IBL_BRDF_LUT_SPV}
    COMMENT "Building all shaders"
)
```

Add to POST_BUILD commands (after line 246):

```cmake
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:lore>/shaders/compiled/ibl
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IBL_EQUIRECT_TO_CUBEMAP_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/ibl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IBL_IRRADIANCE_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/ibl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IBL_PREFILTER_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/ibl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IBL_BRDF_LUT_SPV} $<TARGET_FILE_DIR:lore>/shaders/compiled/ibl/
```

## Alternative: Manual Compilation

If CMakeLists.txt integration fails, compile manually:

```bash
# Create output directory
mkdir -p shaders/compiled/ibl

# Find glslangValidator
# On Windows with Vulkan SDK: C:/VulkanSDK/<version>/Bin/glslangValidator.exe

# Compile shaders
glslangValidator -V shaders/ibl/equirect_to_cubemap.comp -o shaders/compiled/ibl/equirect_to_cubemap.spv
glslangValidator -V shaders/ibl/irradiance_convolution.comp -o shaders/compiled/ibl/irradiance_convolution.spv
glslangValidator -V shaders/ibl/prefilter_environment.comp -o shaders/compiled/ibl/prefilter_environment.spv
glslangValidator -V shaders/ibl/brdf_integration.comp -o shaders/compiled/ibl/brdf_integration.spv
```