# IBL Shader Integration Guide

## Overview

This document shows how to integrate Image-Based Lighting (IBL) into the existing `deferred_lighting.frag` shader to work alongside the atmospheric scattering system.

## Shader Changes Required

### 1. Add IBL Descriptor Set (set = 3)

Add after atmospheric descriptors (around line 118):

```glsl
// IBL textures (set = 3)
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilteredMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

layout(set = 3, binding = 3) uniform EnvironmentUBO {
    uint mode;                      // 0 = atmospheric, 1 = HDRI, 2 = hybrid
    float hdri_intensity;
    float hdri_rotation_y;
    float atmospheric_blend;
    vec3 hdri_tint;
    float hdri_saturation;
    float hdri_contrast;
    uint apply_fog;
    uint apply_aerial_perspective;
} environment;
```

### 2. Add IBL Sampling Function

Add before main() function (around line 710):

```glsl
/**
 * Sample Image-Based Lighting (IBL) for ambient term
 * Uses split-sum approximation for real-time PBR
 *
 * @param N Normal vector (world space)
 * @param V View vector (world space)
 * @param albedo Surface albedo
 * @param metallic Metallic factor
 * @param roughness Roughness factor
 * @return IBL contribution (diffuse + specular)
 */
vec3 sampleIBL(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness) {
    // Fresnel-Schlick approximation at normal incidence
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);
    vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);

    // Diffuse IBL (irradiance)
    vec3 kD = (1.0 - F) * (1.0 - metallic); // Energy conservation
    vec3 irradiance = texture(irradianceMap, N).rgb * environment.hdri_intensity;
    vec3 diffuse = kD * irradiance * albedo;

    // Specular IBL (split-sum approximation)
    vec3 R = reflect(-V, N);

    // Apply environment rotation if needed
    if (environment.hdri_rotation_y != 0.0) {
        float cosRot = cos(environment.hdri_rotation_y);
        float sinRot = sin(environment.hdri_rotation_y);
        float x = R.x;
        float z = R.z;
        R.x = x * cosRot - z * sinRot;
        R.z = x * sinRot + z * cosRot;
    }

    // Sample pre-filtered environment at appropriate roughness level
    // Roughness maps to mip level: roughness * (mipLevels - 1)
    float maxMipLevel = 4.0; // 5 mip levels (0-4)
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * maxMipLevel).rgb * environment.hdri_intensity;

    // BRDF integration lookup
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    // Apply color tint and saturation
    vec3 ibl_color = diffuse + specular;
    ibl_color *= environment.hdri_tint;

    if (environment.hdri_saturation != 1.0) {
        float luminance = dot(ibl_color, vec3(0.2126, 0.7152, 0.0722));
        ibl_color = mix(vec3(luminance), ibl_color, environment.hdri_saturation);
    }

    return ibl_color;
}
```

### 3. Integrate IBL into Main Lighting Loop

Replace the ambient term calculation in main() (around line 960):

```glsl
void main() {
    // ... existing G-Buffer sampling code ...

    vec3 N = normalize(normal);
    vec3 V = normalize(cameraPos - worldPos);

    vec3 finalColor = vec3(0.0);

    // Direct lighting loop (existing code)
    for (int i = 0; i < lightCount; ++i) {
        // ... existing direct lighting code ...
    }

    // **NEW: Add IBL ambient term**
    vec3 ambient;
    if (environment.mode == 0) {
        // Pure Atmospheric mode - use simple ambient
        ambient = vec3(0.03) * albedo;
    } else if (environment.mode == 1) {
        // Pure HDRI mode - full IBL
        ambient = sampleIBL(N, V, albedo, metallic, roughness);
    } else if (environment.mode == 2) {
        // Hybrid mode - blend IBL with atmospheric
        vec3 ibl_ambient = sampleIBL(N, V, albedo, metallic, roughness);

        // Sample atmospheric for ambient (simplified)
        vec3 transmittance, inScattering;
        sampleAtmospheric(worldPos, V, transmittance, inScattering);
        vec3 atm_ambient = inScattering * 0.5; // Scale down for ambient

        // Blend based on atmospheric_blend parameter
        ambient = mix(ibl_ambient, atm_ambient, environment.atmospheric_blend);
    }

    finalColor += ambient;

    // **MODIFIED: Apply atmospheric effects on top (if hybrid mode)**
    if (environment.mode == 2 && environment.apply_fog == 1) {
        vec3 transmittance, inScattering;
        sampleAtmospheric(worldPos, V, transmittance, inScattering);

        // Attenuate direct + IBL lighting
        finalColor = finalColor * transmittance;

        // Add aerial perspective
        if (environment.apply_aerial_perspective == 1) {
            finalColor += inScattering;
        }
    }

    // Apply emissive
    finalColor += emissive;

    // Output
    fragColor = vec4(finalColor, 1.0);
}
```

## C++ Integration

### 1. Create IBL Descriptor Set Layout

```cpp
void DeferredRenderer::create_ibl_descriptor_layout() {
    VkDescriptorSetLayoutBinding bindings[4] = {};

    // Binding 0: Irradiance cubemap
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: Pre-filtered environment cubemap
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2: BRDF LUT
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 3: Environment UBO
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
    layout_info.pBindings = bindings;

    vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &ibl_descriptor_layout);
}
```

### 2. Update IBL Descriptor Set

```cpp
void DeferredRenderer::update_ibl_descriptors(const HDRIEnvironment& hdri) {
    const auto& textures = hdri.textures();

    VkDescriptorImageInfo irradiance_info{};
    irradiance_info.sampler = textures.irradiance_sampler;
    irradiance_info.imageView = textures.irradiance_view;
    irradiance_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo prefiltered_info{};
    prefiltered_info.sampler = textures.prefiltered_sampler;
    prefiltered_info.imageView = textures.prefiltered_view;
    prefiltered_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo brdf_info{};
    brdf_info.sampler = textures.brdf_lut_sampler;
    brdf_info.imageView = textures.brdf_lut_view;
    brdf_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo env_ubo_info{};
    env_ubo_info.buffer = environment_ubo_buffer;
    env_ubo_info.offset = 0;
    env_ubo_info.range = sizeof(EnvironmentUBO);

    VkWriteDescriptorSet writes[4] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = ibl_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &irradiance_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = ibl_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &prefiltered_info;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = ibl_descriptor_set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &brdf_info;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = ibl_descriptor_set;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &env_ubo_info;

    vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
}
```

### 3. Render Loop Integration

```cpp
void DeferredRenderer::render_frame(const HDRIEnvironment& hdri, EnvironmentMode mode) {
    // Update environment UBO
    auto env_ubo = hdri.get_environment_ubo(mode);
    void* data;
    vmaMapMemory(allocator, environment_ubo_allocation, &data);
    memcpy(data, &env_ubo, sizeof(EnvironmentUBO));
    vmaUnmapMemory(allocator, environment_ubo_allocation);

    VkCommandBuffer cmd = begin_frame();

    // G-Buffer pass (existing)
    render_gbuffer_pass(cmd);

    // Lighting pass with IBL
    begin_lighting_pass(cmd);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_lighting_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            0, 1, &gbuffer_descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            1, 1, &lighting_descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            2, 1, &atmospheric_descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            3, 1, &ibl_descriptor_set, 0, nullptr); // NEW: IBL descriptors
    vkCmdDraw(cmd, 6, 1, 0, 0); // Fullscreen quad
    end_lighting_pass(cmd);

    // Optional: Skybox pass (if mode != Atmospheric)
    if (mode != EnvironmentMode::Atmospheric) {
        render_skybox(cmd, hdri);
    }

    end_frame(cmd);
}
```

## Testing Checklist

- [ ] Shader compiles without errors
- [ ] IBL descriptor set created and bound correctly
- [ ] Diffuse IBL visible on rough surfaces
- [ ] Specular IBL visible on smooth metallic surfaces
- [ ] Metallic surfaces show clear environment reflections
- [ ] Rough surfaces show blurred reflections
- [ ] Atmospheric mode still works (fallback to simple ambient)
- [ ] Hybrid mode blends IBL and atmospheric correctly
- [ ] HDRI intensity control works
- [ ] Environment rotation works
- [ ] Performance acceptable (~0.2ms per frame for IBL sampling)

## Performance Notes

**Expected Cost:**
- IBL sampling: ~0.1ms @ 1080p (4 texture lookups per pixel)
- Skybox rendering: ~0.05ms (early-Z culled in most cases)
- Total overhead: ~0.15-0.2ms per frame

**Optimization Tips:**
- Use lower resolution irradiance map (32×32 sufficient for diffuse)
- Reduce pre-filter mip count for lower-end GPUs (3-4 mips)
- Cache BRDF LUT (same for all HDRIs, only need one)
- Use hardware trilinear filtering (faster than manual mip selection)

## Troubleshooting

### Dark/Black Ambient Lighting
- Check HDRI intensity (should be 0.8-1.5 typically)
- Verify IBL maps generated correctly (not all black)
- Check descriptor set bindings (correct set = 3?)

### Overly Bright Reflections
- Reduce HDRI intensity
- Check pre-filter mip levels (too few = overly sharp reflections)
- Verify roughness values in G-Buffer (0.0 = mirror, 1.0 = diffuse)

### Incorrect Environment Orientation
- Adjust `hdri_rotation_y` parameter
- Check cubemap face orientation (may need to flip coordinates)

### Performance Issues
- Use lower quality preset (medium instead of high)
- Reduce environment cubemap resolution
- Check for redundant IBL sampling (should only sample once per pixel)