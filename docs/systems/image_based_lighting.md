# Image-Based Lighting (IBL) System

## Overview

The IBL system provides photorealistic ambient lighting and reflections using High Dynamic Range (HDR) environment maps. It works **in combination** with the procedural atmospheric scattering system to create stunning hybrid environments.

## Rendering Modes

### 1. Pure Atmospheric (Procedural)
- Mathematical sky simulation (Rayleigh + Mie scattering)
- Dynamic time-of-day transitions
- Procedural fog and aerial perspective
- Lower memory usage
- **Use for:** Dynamic weather, sci-fi worlds, stylized art

### 2. Pure HDRI (Image-Based)
- Photorealistic captured lighting
- High-quality reflections on metallic surfaces
- Pre-baked ambient occlusion
- Static lighting conditions
- **Use for:** Photorealistic scenes, product visualization, archviz

### 3. Hybrid (HDRI + Atmospheric) ⭐
- HDRI provides base ambient lighting and skybox
- Atmospheric adds dynamic fog, depth, and aerial perspective
- Best of both worlds: photorealism + atmospheric depth
- **Use for:** Game environments, cinematic scenes, outdoor levels

## Technical Architecture

### IBL Pre-computation Pipeline

```
HDRI (.exr) → Equirectangular Texture (4K HDR)
    ↓
Equirect → Cubemap Conversion (compute shader)
    ↓
    ├─→ Irradiance Map (32×32 cubemap, diffuse IBL)
    ├─→ Pre-filtered Environment Map (512×512 cubemap, 5 mip levels, specular IBL)
    └─→ BRDF Integration LUT (512×512 2D texture, split-sum approximation)
```

### Memory Footprint (per HDRI)
- **Equirectangular**: 4K × 2K × RGBA16F = 64 MB
- **Environment Cubemap**: 512×512×6 × RGBA16F = 12 MB
- **Irradiance Cubemap**: 32×32×6 × RGBA16F = 48 KB
- **Pre-filtered Mips**: ~16 MB (5 levels)
- **BRDF LUT**: 512×512 × RG16F = 2 MB
- **Total**: ~94 MB per HDRI

### Shader Integration

**Deferred Lighting Shader Changes:**
```glsl
// IBL textures (set = 3)
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilteredMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

layout(set = 3, binding = 3) uniform EnvironmentUBO {
    int mode; // 0 = atmospheric, 1 = HDRI, 2 = hybrid
    float hdri_intensity;
    float atmospheric_blend;
} environment;

// Diffuse IBL
vec3 irradiance = texture(irradianceMap, N).rgb;
vec3 diffuseIBL = irradiance * albedo;

// Specular IBL (split-sum approximation)
vec3 R = reflect(-V, N);
vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * 4.0).rgb;
vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

// Hybrid mode: blend with atmospheric
if (environment.mode == 2) {
    vec3 atmospheric_color = /* atmospheric sampling */;
    finalColor = mix(iblColor, atmospheric_color, environment.atmospheric_blend);
}
```

## Implementation

### C++ API

```cpp
#include <lore/graphics/hdri_environment.hpp>

// Load HDRI
auto hdri = HDRIEnvironment::load_from_file(
    device,
    "C:/Users/chris/Desktop/hdri/belfast_sunset_puresky_4k.exr"
);

// Pre-compute IBL maps (one-time cost)
hdri.generate_ibl_maps(command_buffer);

// Set environment mode
renderer.set_environment_mode(EnvironmentMode::Hybrid);
renderer.set_hdri_environment(hdri);
renderer.set_atmospheric_blend(0.3f); // 30% atmospheric fog overlay
```

### Compute Shader Pipeline

**1. Equirectangular to Cubemap (`equirect_to_cubemap.comp`)**
- Input: 4K×2K equirectangular HDR texture
- Output: 512×512 cubemap (6 faces)
- Samples equirect using spherical coordinates

**2. Irradiance Convolution (`irradiance_convolution.comp`)**
- Input: Environment cubemap
- Output: 32×32 irradiance cubemap
- Convolves hemisphere for diffuse lighting

**3. Pre-filter Environment (`prefilter_environment.comp`)**
- Input: Environment cubemap
- Output: 512×512 cubemap with 5 mip levels
- Importance-samples GGX for varying roughness

**4. BRDF Integration (`brdf_integration.comp`)**
- Input: None (analytical)
- Output: 512×512 BRDF LUT (RG16F)
- Pre-computes Fresnel × Geometry term

## Performance Characteristics

### Pre-computation Cost (One-Time)
- **Equirect → Cubemap**: ~2ms (512×512)
- **Irradiance Convolution**: ~50ms (32×32, 1024 samples/pixel)
- **Pre-filter Environment**: ~200ms (5 mip levels, importance sampling)
- **BRDF Integration**: ~10ms (512×512, 512 samples/pixel)
- **Total**: ~262ms per HDRI

### Runtime Cost (Per Frame)
- **IBL Sampling**: ~0.1ms @ 1080p (4 texture lookups in fragment shader)
- **Skybox Rendering**: ~0.05ms (6-face cubemap, early-Z culled)
- **Hybrid Blending**: +0.05ms (additional atmospheric sampling)
- **Total**: ~0.2ms per frame

## File Formats Supported

### Input HDRIs
- **EXR** (OpenEXR) - Recommended, lossless HDR
- **HDR** (Radiance RGBE) - Widely supported
- **PNG** (16-bit) - Limited dynamic range

### Recommended Sources
- **Poly Haven** (polyhaven.com) - Free 4K+ HDRIs
- **HDRI Haven** (hdrihaven.com) - Free environments
- **Custom captures** - 360° HDR photography

## Quality Presets

### Ultra (Photorealistic)
```cpp
HDRIConfig ultra = {
    .environment_resolution = 2048,
    .irradiance_resolution = 64,
    .prefiltered_mip_levels = 6,
    .brdf_lut_resolution = 512,
    .sample_count = 2048
};
```

### High (Balanced)
```cpp
HDRIConfig high = {
    .environment_resolution = 1024,
    .irradiance_resolution = 32,
    .prefiltered_mip_levels = 5,
    .brdf_lut_resolution = 512,
    .sample_count = 1024
};
```

### Medium (Performance)
```cpp
HDRIConfig medium = {
    .environment_resolution = 512,
    .irradiance_resolution = 32,
    .prefiltered_mip_levels = 5,
    .brdf_lut_resolution = 256,
    .sample_count = 512
};
```

## Integration with Existing Systems

### Deferred Renderer
- IBL descriptor set added as `set = 3`
- Skybox rendered after G-Buffer, before lighting
- IBL sampled in lighting pass for ambient term

### Atmospheric System
- Atmospheric scattering applied **after** IBL in hybrid mode
- Transmittance modulates HDRI skybox
- In-scattering adds depth fog over IBL lighting

### Post-Processing
- Tone mapping applied **after** IBL + atmospheric
- Exposure can be auto-adjusted based on HDRI luminance
- Bloom works naturally with HDR skybox highlights

## Usage Examples

### Example 1: Static HDRI Scene
```cpp
// Load sunset HDRI
auto sunset = HDRIEnvironment::load_from_file(
    device,
    "belfast_sunset_puresky_4k.exr"
);
sunset.generate_ibl_maps(command_buffer);

// Pure HDRI mode
renderer.set_environment_mode(EnvironmentMode::HDRI);
renderer.set_hdri_environment(sunset);
renderer.set_hdri_intensity(1.2f);
```

### Example 2: Hybrid Outdoor Scene
```cpp
// Load orchard HDRI for base lighting
auto orchard = HDRIEnvironment::load_from_file(
    device,
    "citrus_orchard_road_puresky_4k.exr"
);
orchard.generate_ibl_maps(command_buffer);

// Hybrid mode with atmospheric depth
renderer.set_environment_mode(EnvironmentMode::Hybrid);
renderer.set_hdri_environment(orchard);
renderer.set_atmospheric_blend(0.4f); // 40% atmospheric fog

// Configure atmospheric parameters
renderer.set_atmospheric_params({
    .mie_scattering = glm::vec3(0.004f), // Light fog
    .rayleigh_scale_height = 8500.0f,
    .sun_direction = glm::normalize(glm::vec3(0.3f, 0.8f, 0.2f))
});
```

### Example 3: Time-of-Day Transition
```cpp
// Load multiple HDRIs
auto morning = HDRIEnvironment::load_from_file(device, "morning.exr");
auto noon = HDRIEnvironment::load_from_file(device, "noon.exr");
auto sunset = HDRIEnvironment::load_from_file(device, "sunset.exr");
auto night = HDRIEnvironment::load_from_file(device, "night.exr");

// Transition based on time
float time = get_time_of_day(); // 0.0 - 1.0
if (time < 0.25f) {
    renderer.set_hdri_environment(morning);
} else if (time < 0.5f) {
    renderer.set_hdri_environment(noon);
} else if (time < 0.75f) {
    renderer.set_hdri_environment(sunset);
} else {
    renderer.set_hdri_environment(night);
}

// Atmospheric effects enhance transitions
renderer.set_atmospheric_blend(0.3f + time * 0.3f); // More fog at dusk
```

## Limitations and Considerations

### Memory Budget
- Each HDRI uses ~94 MB VRAM
- Recommend limiting to 2-4 active HDRIs
- Use texture compression (BC6H) for larger counts

### Static Lighting
- HDRIs provide static lighting conditions
- Cannot animate sun position within single HDRI
- Use multiple HDRIs or atmospheric mode for dynamic lighting

### Seams and Artifacts
- Ensure HDRIs are properly stitched (no visible seams)
- Use "puresky" variants for clean horizons
- Consider color correction for consistency

## Future Enhancements

### Real-Time GI Integration
- Use HDRI as first bounce for RTXGI/DDGI
- Infinite distance probe for outdoor scenes

### Volumetric Integration
- Fog uses HDRI for in-scattering color
- Volumetric clouds lit by HDRI sun direction

### Dynamic Weather
- Blend between clear/cloudy/stormy HDRIs
- Procedural cloud overlay on HDRI skybox

---

**Next Steps:**
1. Implement HDRIEnvironment class
2. Add EXR texture loading (via stb_image or tinyexr)
3. Write compute shaders for IBL pre-computation
4. Update deferred_lighting.frag with IBL sampling
5. Add skybox rendering pass
6. Test with belfast_sunset_puresky_4k.exr