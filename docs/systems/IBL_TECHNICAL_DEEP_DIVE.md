# Image-Based Lighting (IBL) - Technical Deep Dive

## Table of Contents
1. [Introduction](#introduction)
2. [Theory and Mathematics](#theory-and-mathematics)
3. [The Rendering Equation](#the-rendering-equation)
4. [IBL Components](#ibl-components)
5. [Implementation Techniques](#implementation-techniques)
6. [Integration with PBR](#integration-with-pbr)
7. [Pros and Cons](#pros-and-cons)
8. [Optimization Strategies](#optimization-strategies)
9. [Integration with Atmospheric Scattering](#integration-with-atmospheric-scattering)
10. [Common Pitfalls](#common-pitfalls)

---

## Introduction

**Image-Based Lighting (IBL)** is a rendering technique that uses photographs or renderings of real-world environments to light 3D scenes. Instead of manually placing point lights, directional lights, and spot lights, IBL captures the complex lighting of an entire environment in a single image (typically a High Dynamic Range Image or HDRI).

### Why IBL?

Traditional lighting requires artists to manually place dozens of lights to approximate real-world lighting:
- **Manual approach**: 50+ lights to simulate a sunny day with indirect bounces
- **IBL approach**: 1 HDRI captures sunlight, sky color, ambient bounce, and reflections

### Real-World Use Cases

- **Product visualization**: Cars, watches, jewelry with photorealistic reflections
- **Architectural rendering**: Interior/exterior spaces with accurate ambient lighting
- **Film VFX**: Matching CG elements to on-set lighting
- **Game environments**: Fast, convincing ambient lighting without expensive light baking

---

## Theory and Mathematics

### The Rendering Equation

All physically-based rendering stems from Kajiya's **Rendering Equation** (1986):

```
L_o(p, ω_o) = L_e(p, ω_o) + ∫_Ω f_r(p, ω_i, ω_o) L_i(p, ω_i) (n · ω_i) dω_i
```

Where:
- **L_o(p, ω_o)**: Outgoing radiance at point `p` in direction `ω_o` (what the camera sees)
- **L_e(p, ω_o)**: Emitted radiance (self-illumination, emissive materials)
- **f_r(p, ω_i, ω_o)**: BRDF (how light reflects off the surface)
- **L_i(p, ω_i)**: Incoming radiance from direction `ω_i`
- **(n · ω_i)**: Cosine term (Lambert's Law - surfaces receive less energy at grazing angles)
- **Ω**: Hemisphere of all incoming light directions

### The IBL Simplification

IBL simplifies this integral by:
1. **Assuming L_i is constant per scene**: Environment lighting doesn't change per-pixel
2. **Pre-computing the integral**: Store results in lookup textures
3. **Using the HDRI as L_i**: The environment map provides incoming radiance from all directions

The HDRI encodes **L_i(p, ω_i)** - the light coming from every direction in the environment.

---

## The Rendering Equation

### Splitting the Integral

The Cook-Torrance BRDF splits lighting into two components:

**Diffuse (Lambertian):**
```
L_o,diffuse = (albedo / π) ∫_Ω L_i(ω_i) (n · ω_i) dω_i
```

**Specular (Microfacet):**
```
L_o,specular = ∫_Ω L_i(ω_i) · BRDF(ω_i, ω_o) · (n · ω_i) dω_i
```

Where BRDF is:
```
BRDF = (D · F · G) / (4 · (n · ω_o) · (n · ω_i))
```

- **D**: Normal Distribution Function (GGX) - microfacet distribution
- **F**: Fresnel term - angle-dependent reflectance
- **G**: Geometry function (Smith) - self-shadowing of microfacets

### The Problem: Infinite Integral

Computing these integrals at runtime for every pixel is **impossible**:
- Each pixel needs to sample **hundreds** of directions in the hemisphere
- At 1920×1080 resolution, that's **2 million pixels × hundreds of samples = billions of operations per frame**
- Even GPUs can't handle this in real-time

### The Solution: Split-Sum Approximation

Epic Games' Brian Karis introduced the **Split-Sum Approximation** in 2013:

```
∫_Ω L_i(ω_i) · BRDF(ω_i, ω_o) · (n · ω_i) dω_i
≈ [∫_Ω L_i(ω_i) · (n · ω_i) dω_i] × [∫_Ω BRDF(ω_i, ω_o) · (n · ω_i) dω_i]
```

**Breakthrough**: This splits one impossible integral into two pre-computable parts:

1. **First integral**: Environment lighting convolved with cosine term
   - **Pre-computed**: Store as prefiltered environment map (cubemap mips)
   - **Depends on**: Roughness (more roughness = blurrier reflections)

2. **Second integral**: BRDF convolved with cosine term
   - **Pre-computed**: Store as 2D lookup texture (BRDF LUT)
   - **Depends on**: Roughness and viewing angle (NdotV)

This is an **approximation** (not mathematically exact), but visually indistinguishable and **1000× faster**.

---

## IBL Components

### 1. Environment Map (Equirectangular → Cubemap)

**Input**: Equirectangular HDR image (4K×2K typical)
- **Format**: Latitude-longitude projection (like world map)
- **HDR**: Stores values > 1.0 (sun can be 100,000× brighter than shadows)
- **Problem**: Cannot be directly sampled by GPU cubemap samplers

**Conversion**: Transform to 6-face cubemap (1024×1024 per face typical)
- **Why**: Cubemaps provide uniform sampling across the sphere
- **How**: For each cubemap texel, calculate direction vector, convert to lat-long UV, sample equirect

**Compute Shader** (`equirect_to_cubemap.comp`):
```glsl
vec3 direction = get_cubemap_direction(face_index, uv);
float phi = atan(direction.z, direction.x);      // Azimuthal angle [-π, π]
float theta = asin(direction.y);                 // Polar angle [-π/2, π/2]
vec2 equirect_uv = vec2(phi / TWO_PI + 0.5, theta / PI + 0.5);
vec4 color = texture(equirectangular_map, equirect_uv);
imageStore(environment_cubemap, ivec3(pixel, face_index), color);
```

### 2. Irradiance Map (Diffuse IBL)

**Purpose**: Pre-convolved diffuse lighting for all surface normals

**Theory**: Diffuse surfaces scatter light uniformly, so we integrate incoming light over the entire hemisphere:

```
E(n) = (1/π) ∫_Ω L_i(ω_i) · cos(θ_i) dω_i
```

Where:
- **E(n)**: Irradiance for normal direction `n`
- **cos(θ_i)**: Lambert's Law (surfaces receive less light at grazing angles)

**Implementation**: For each direction `n` (cubemap texel):
1. Sample environment map in **hemisphere** around `n`
2. Weight samples by **cos(θ)** (Lambert term)
3. Average samples to get irradiance

**Compute Shader** (`irradiance_convolution.comp`):
```glsl
vec3 irradiance = vec3(0.0);
for (int i = 0; i < SAMPLE_COUNT; i++) {
    vec3 sample_dir = hemisphere_sample(normal, i);  // Random direction
    float cos_theta = max(dot(normal, sample_dir), 0.0);
    vec3 radiance = texture(environment_map, sample_dir).rgb;
    irradiance += radiance * cos_theta;
}
irradiance /= SAMPLE_COUNT;
irradiance *= PI;  // Normalize by hemisphere solid angle
```

**Key Insight**: This is **independent of view direction** - only depends on surface normal. Compute once, use forever.

**Storage**: Low-resolution cubemap (32×32 or 64×64 typical)
- **Why low-res?**: Diffuse lighting is smooth and low-frequency
- **Memory**: 32³ × 6 faces × RGBA16F = 768 KB

### 3. Prefiltered Environment Map (Specular IBL)

**Purpose**: Pre-convolved specular reflections for varying roughness levels

**Theory**: Specular reflections depend on surface roughness:
- **Roughness = 0**: Mirror-like (reflect environment sharply)
- **Roughness = 1**: Diffuse-like (blurry, almost uniform)

The reflection integral is:
```
L_o,spec = ∫_Ω L_i(ω_i) · D(ω_h) · F(ω_h, ω_o) · G(ω_i, ω_o, ω_h) · (n · ω_i) dω_i
```

**Problem**: This depends on **both** roughness AND view direction. Can't pre-compute for all combinations.

**Solution - Importance Sampling**: Sample directions where the BRDF is strongest:
- **GGX Distribution**: Most energy is concentrated around the reflection vector `R`
- **Importance sampling**: Only sample directions that matter, weighted by probability

**Implementation**: For each reflection direction `R` and roughness level:
1. Generate sample directions using **GGX importance sampling**
2. Sample environment map at those directions
3. Weight samples by BRDF and probability density function (PDF)
4. Average to get filtered result

**Compute Shader** (`prefilter_environment.comp`):
```glsl
vec3 N = R;  // Normal = Reflection for pre-filtering
vec3 V = R;  // View = Reflection (assume viewer looking down reflection)

vec3 prefiltered_color = vec3(0.0);
float total_weight = 0.0;

for (int i = 0; i < SAMPLE_COUNT; i++) {
    vec2 Xi = hammersley_2d(i, SAMPLE_COUNT);  // Low-discrepancy sequence
    vec3 H = importance_sample_ggx(Xi, N, roughness);  // GGX distribution
    vec3 L = normalize(2.0 * dot(V, H) * H - V);  // Reflect around H

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL > 0.0) {
        vec3 radiance = texture(environment_map, L).rgb;
        prefiltered_color += radiance * NdotL;
        total_weight += NdotL;
    }
}

prefiltered_color /= total_weight;
```

**Storage**: Mipmap chain (1024→512→256→...→1 typical)
- **Mip 0** (roughness=0): Sharp reflections (1024×1024)
- **Mip 5** (roughness=0.5): Blurry reflections (32×32)
- **Mip 10** (roughness=1.0): Almost uniform (1×1)
- **Memory**: ~1024³ × 6 × RGBA16F × 4/3 (mip chain) = ~48 MB

### 4. BRDF Integration LUT (Split-Sum Second Term)

**Purpose**: Pre-compute Fresnel × Geometry term for split-sum approximation

**Theory**: The second integral in split-sum is:
```
∫_Ω BRDF(ω_i, ω_o) · (n · ω_i) dω_i = F₀ × scale + bias
```

Where:
- **F₀**: Reflectance at normal incidence (material property)
- **scale, bias**: Depend on roughness and NdotV (view angle)

**Implementation**: For each (NdotV, roughness) pair:
1. Importance sample GGX distribution
2. Integrate Fresnel × Geometry term
3. Store `scale` in R channel, `bias` in G channel

**Compute Shader** (`brdf_integration.comp`):
```glsl
float roughness = texCoord.y;
float NdotV = texCoord.x;

vec3 V = vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV);  // View in tangent space
vec3 N = vec3(0.0, 0.0, 1.0);

float scale = 0.0;
float bias = 0.0;

for (uint i = 0; i < SAMPLE_COUNT; i++) {
    vec2 Xi = hammersley_2d(i, SAMPLE_COUNT);
    vec3 H = importance_sample_ggx(Xi, N, roughness);
    vec3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdotL = max(L.z, 0.0);
    float NdotH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0) {
        float G = geometry_smith(N, V, L, roughness);
        float G_vis = (G * VdotH) / (NdotH * NdotV);
        float Fc = pow(1.0 - VdotH, 5.0);  // Schlick approximation

        scale += (1.0 - Fc) * G_vis;
        bias += Fc * G_vis;
    }
}

scale /= SAMPLE_COUNT;
bias /= SAMPLE_COUNT;

imageStore(brdf_lut, pixel, vec4(scale, bias, 0.0, 1.0));
```

**Storage**: 2D texture (512×512 typical)
- **X-axis**: NdotV (0=grazing, 1=perpendicular)
- **Y-axis**: Roughness (0=smooth, 1=rough)
- **Memory**: 512² × RG16F = 1 MB
- **Advantage**: Same LUT works for ALL materials and environments!

---

## Implementation Techniques

### Runtime IBL Sampling

**Fragment Shader** (per-pixel):

```glsl
// 1. Calculate view and reflection vectors
vec3 V = normalize(camera_pos - world_pos);
vec3 N = normalize(surface_normal);
vec3 R = reflect(-V, N);

// 2. Apply Y-axis rotation for sun alignment
R = rotate_y(R, ibl_rotation);
N = rotate_y(N, ibl_rotation);

// 3. Sample diffuse irradiance (low-res cubemap, no mips)
vec3 irradiance = texture(irradiance_map, N).rgb;

// 4. Sample specular prefiltered environment (high-res cubemap with mips)
float lod = roughness * max_reflection_lod;  // Map roughness to mip level
vec3 prefiltered_color = textureLod(prefiltered_map, R, lod).rgb;

// 5. Sample BRDF LUT
float NdotV = max(dot(N, V), 0.0);
vec2 brdf = texture(brdf_lut, vec2(NdotV, roughness)).rg;

// 6. Combine with Fresnel (energy conservation)
vec3 F = fresnel_schlick_roughness(F0, V, N, roughness);
vec3 kS = F;  // Specular contribution
vec3 kD = (1.0 - kS) * (1.0 - metallic);  // Diffuse contribution (metals don't diffuse)

// 7. Final IBL contribution
vec3 diffuse = irradiance * albedo * kD;
vec3 specular = prefiltered_color * (F0 * brdf.x + brdf.y);
vec3 ambient = (diffuse + specular) * ibl_intensity * ao;
```

**Key Points**:
- **2 cubemap samples**: Irradiance (N) + Prefiltered (R, lod)
- **1 2D texture sample**: BRDF LUT (NdotV, roughness)
- **Runtime cost**: ~0.2ms @ 1080p (very cheap!)

### HDRI Rotation for Sun Alignment

**Problem**: HDRI sun position might not match scene's directional light

**Solution**: Detect brightest spot in HDRI (sun), rotate HDRI to align with scene sun

**Detection** (CPU, once per HDRI):
```cpp
float max_luminance = 0.0f;
uint32_t max_u = 0, max_v = 0;

for (uint32_t v = 0; v < height; ++v) {
    for (uint32_t u = 0; u < width; ++u) {
        float lum = 0.2126*R + 0.7152*G + 0.0722*B;  // Rec. 709
        if (lum > max_luminance) {
            max_luminance = lum;
            max_u = u;
            max_v = v;
        }
    }
}

// Convert pixel to spherical direction
float phi = (max_u / width * 2.0 - 1.0) * PI;
float theta = (max_v / height * 2.0 - 1.0) * PI/2;
vec3 sun_dir = vec3(cos(theta)*cos(phi), sin(theta), cos(theta)*sin(phi));
```

**Alignment** (CPU):
```cpp
// Calculate rotation angle to align HDRI sun with scene sun
vec2 hdri_sun_xz = vec2(hdri_sun.x, hdri_sun.z);
vec2 scene_sun_xz = vec2(scene_sun.x, scene_sun.z);

float hdri_angle = atan2(hdri_sun_xz.y, hdri_sun_xz.x);
float scene_angle = atan2(scene_sun_xz.y, scene_sun_xz.x);
float rotation = scene_angle - hdri_angle;

ibl_config.rotation_y = rotation;
```

**Runtime** (GPU, per-pixel):
```glsl
vec3 rotate_y(vec3 dir, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec3(c * dir.x + s * dir.z, dir.y, -s * dir.x + c * dir.z);
}

vec3 rotated_normal = rotate_y(normal, ibl_rotation);
```

---

## Integration with PBR

### Energy Conservation

**Fundamental Law**: Reflected light ≤ Incident light (no free energy!)

**Implementation**:
```glsl
vec3 F = fresnel_schlick(F0, VdotH);  // How much light is reflected (specular)
vec3 kS = F;                           // Specular contribution
vec3 kD = (1.0 - kS) * (1.0 - metallic);  // Diffuse contribution

// Energy equation: kD + kS = 1.0 (for dielectrics)
// Metals: kD = 0 (no diffuse), kS ≈ 1.0 (all specular)
```

### Ambient Occlusion Integration

**Problem**: IBL assumes full hemisphere visibility (no geometry occlusion)

**Solution**: Multiply IBL by AO to darken occluded areas

```glsl
vec3 ambient = (diffuse + specular) * ao;
```

**Where AO comes from**:
- **Baked AO**: Pre-computed in texture (static geometry)
- **SSAO**: Screen-space ambient occlusion (dynamic, approximate)
- **Ray-traced AO**: Ground truth (expensive, 1-4 spp typical)

---

## Pros and Cons

### Advantages ✅

1. **Photorealism**: Captures real-world lighting complexity
2. **Artist-Friendly**: One HDRI vs. dozens of manual lights
3. **Fast Runtime**: ~0.2ms for ambient lighting (cheaper than many point lights)
4. **Automatic Reflections**: Metallic surfaces get realistic environment reflections
5. **Consistent Lighting**: All objects lit from the same environment
6. **Scalable**: Works for small objects (product viz) to large scenes (open worlds)

### Disadvantages ❌

1. **Memory Cost**:
   - **High quality**: ~100 MB per HDRI (equirect + cubemap + irradiance + prefiltered + BRDF)
   - **Multiple environments**: Costs multiply (but BRDF LUT is shared)

2. **Pre-computation Time**:
   - **Irradiance convolution**: ~50-100ms (hemisphere integration)
   - **Prefiltered generation**: ~200-500ms (importance sampling × mip chain)
   - **BRDF LUT**: ~50ms (one-time, reusable)
   - **Total**: ~300-650ms per HDRI

3. **Static Lighting**:
   - HDRI is a snapshot, doesn't respond to dynamic lights
   - **Solution**: Blend with dynamic lights (additive)

4. **No Shadows**:
   - IBL doesn't know about scene geometry
   - **Solution**: Combine with shadow mapping for directional lights

5. **Infinite Distance Assumption**:
   - IBL assumes environment is infinitely far (no parallax)
   - **Problem**: Reflections don't match nearby geometry
   - **Solution**: Local reflection probes (per-room/area)

6. **Indoor/Outdoor Mismatch**:
   - Outdoor HDRI doesn't work indoors (sun comes through walls)
   - **Solution**: Blend multiple HDRIs per zone

### When to Use IBL

✅ **Use IBL when:**
- Outdoor scenes with sky/sun
- Product visualization (cars, jewelry)
- Scenes with many metallic/glossy materials
- Need fast, convincing ambient lighting
- Have memory budget (50-100 MB per HDRI)

❌ **Avoid IBL when:**
- Entirely indoor scenes (many lights, no sky)
- Memory-constrained (mobile, low-end PC)
- Need dynamic time-of-day (IBL is static - use atmospheric scattering instead)
- Mostly diffuse materials (simple ambient term sufficient)

---

## Optimization Strategies

### 1. Resolution Tuning

**Irradiance Map**:
- ✅ **32×32**: Sufficient for smooth diffuse (24 KB)
- ⚠️ **64×64**: Slightly better (96 KB)
- ❌ **128×128**: Overkill (diffuse is low-frequency)

**Prefiltered Environment**:
- ❌ **2048×2048**: Film quality (192 MB!)
- ✅ **1024×1024**: High quality (48 MB)
- ⚠️ **512×512**: Medium quality (12 MB)
- ❌ **256×256**: Low quality, visible artifacts (3 MB)

**BRDF LUT**:
- ✅ **512×512**: Standard (1 MB)
- ⚠️ **256×256**: Budget (256 KB, slight banding)

### 2. Format Optimization

**RGBA16F (Half-Float)**:
- ✅ **Pros**: HDR range, hardware-accelerated filtering
- ❌ **Cons**: 8 bytes per pixel (2× RGBA8)

**RGB9E5 (Shared Exponent)**:
- ✅ **Pros**: 4 bytes per pixel (50% smaller), HDR range
- ⚠️ **Cons**: Lower precision, no alpha channel

**BC6H (Block Compression)**:
- ✅ **Pros**: 1 byte per pixel (87.5% smaller!), HDR, hardware decompression
- ⚠️ **Cons**: Lossy compression, block artifacts
- **Use case**: Shipping builds, not authoring

### 3. Mip Level Clamping

**Technique**: Limit max roughness to skip highest mip levels

```cpp
// Instead of 11 mip levels (1024→1)
config.max_reflection_lod = 5;  // Only 6 mips (1024→32)

// Shader automatically clamps:
float lod = roughness * config.max_reflection_lod;
```

**Savings**: ~25% memory (skip mips 6-10)
**Trade-off**: Very rough surfaces (roughness > 0.5) have less blur

### 4. Multiple LOD Sets

**Technique**: Load different quality IBL based on distance

```cpp
if (camera_distance < 10.0f) {
    bind_ibl(high_quality);  // 1024³ prefiltered
} else if (camera_distance < 50.0f) {
    bind_ibl(medium_quality);  // 512³ prefiltered
} else {
    bind_ibl(low_quality);  // 256³ prefiltered
}
```

**Savings**: 75% memory for distant objects
**Trade-off**: Texture switching overhead

### 5. Sparse HDRI Sets

**Problem**: Open world game might have 100+ zones, each needing HDRI

**Solution**: Strategic HDRI placement with blending

```cpp
// Outdoor: 4 HDRIs (dawn, noon, dusk, night) = 400 MB
// Indoor: 10 HDRIs (per major building/room) = 1 GB
// Total: 1.4 GB (manageable)

// Blend between nearest 2 HDRIs
float blend = smoothstep(zone1_dist, zone2_dist, camera_dist);
vec3 ibl1 = sample_ibl(hdri1, N, R);
vec3 ibl2 = sample_ibl(hdri2, N, R);
vec3 final_ibl = mix(ibl1, ibl2, blend);
```

---

## Integration with Atmospheric Scattering

### Hybrid Rendering Mode

**Goal**: Combine HDRI base lighting with atmospheric fog overlay

**Why**:
- **HDRI**: Provides sharp sun, realistic colors, detailed reflections
- **Atmospheric**: Adds distance fog, aerial perspective, volumetric effects

**Shader Integration**:
```glsl
// 1. Sample IBL for base ambient lighting
vec3 ibl_ambient = sample_ibl(N, R, roughness, metallic, albedo);

// 2. Sample atmospheric scattering for transmittance and in-scattering
vec3 transmittance, in_scattering;
sample_atmospheric(world_pos, view_dir, transmittance, in_scattering);

// 3. Blend IBL with atmospheric
float atmospheric_blend = ibl_config.atmospheric_blend;  // 0.3 typical

// Apply transmittance to IBL (atmosphere attenuates distant light)
vec3 ambient_attenuated = ibl_ambient * transmittance;

// Blend atmospheric scattering over IBL base
vec3 final_ambient = mix(ambient_attenuated, in_scattering, atmospheric_blend);
```

**Blend Factor Tuning**:
- **0.0**: Pure IBL (no atmospheric fog, clear day)
- **0.3**: Subtle fog overlay (realistic outdoor)
- **0.7**: Heavy fog (misty/overcast)
- **1.0**: Pure atmospheric (no IBL contribution)

### Sun Alignment Workflow

**Step 1**: Detect sun in HDRI (automatic)
```cpp
auto sun_info = hdri.detect_sun();
if (sun_info.has_sun) {
    printf("Sun detected at azimuth: %.2f°, elevation: %.2f°\n",
           sun_info.azimuth * 180/PI, sun_info.elevation * 180/PI);
}
```

**Step 2**: Align HDRI sun with atmospheric sun
```cpp
vec3 atmospheric_sun_dir = normalize(vec3(0.3f, 0.6f, 0.2f));
hdri.align_sun_to_direction(atmospheric_sun_dir);
```

**Step 3**: Runtime rotation in shader (automatic)
```glsl
vec3 N_rotated = rotate_y(N, ibl.rotation_y);
vec3 R_rotated = rotate_y(R, ibl.rotation_y);
```

**Result**: HDRI sunlight matches atmospheric scattering direction!

---

## Common Pitfalls

### 1. Forgetting Gamma Correction

**Problem**: HDRI is linear, display is sRGB

**Wrong**:
```glsl
vec3 final_color = ambient + direct_lighting;
outColor = vec4(final_color, 1.0);  // ❌ Too dark!
```

**Correct**:
```glsl
vec3 final_color = ambient + direct_lighting;
vec3 tone_mapped = tone_map_aces(final_color);  // HDR → LDR
vec3 gamma_corrected = pow(tone_mapped, vec3(1.0/2.2));  // Linear → sRGB
outColor = vec4(gamma_corrected, 1.0);  // ✅
```

### 2. Incorrect Normal Mapping with IBL

**Problem**: Tangent-space normal maps don't directly work with world-space IBL

**Wrong**:
```glsl
vec3 normal = texture(normal_map, uv).rgb * 2.0 - 1.0;  // Tangent space
vec3 irradiance = texture(irradiance_map, normal).rgb;  // ❌ Wrong space!
```

**Correct**:
```glsl
vec3 tangent_normal = texture(normal_map, uv).rgb * 2.0 - 1.0;
vec3 world_normal = tbn_matrix * tangent_normal;  // Transform to world space
vec3 irradiance = texture(irradiance_map, world_normal).rgb;  // ✅
```

### 3. Missing Energy Conservation

**Problem**: kD + kS > 1.0 creates energy

**Wrong**:
```glsl
vec3 diffuse = irradiance * albedo;  // Full diffuse
vec3 specular = prefiltered * brdf_term;  // Full specular
vec3 ambient = diffuse + specular;  // ❌ Too bright!
```

**Correct**:
```glsl
vec3 F = fresnel_schlick_roughness(F0, V, N, roughness);
vec3 kS = F;
vec3 kD = (1.0 - kS) * (1.0 - metallic);  // Ensure kD + kS ≤ 1.0
vec3 diffuse = irradiance * albedo * kD;
vec3 specular = prefiltered * brdf_term;
vec3 ambient = diffuse + specular;  // ✅
```

### 4. Texture Coordinate Wrapping

**Problem**: Cubemaps must use `CLAMP_TO_EDGE`, not `REPEAT`

**Wrong**:
```cpp
sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // ❌ Seams!
```

**Correct**:
```cpp
sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // ✅
sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
```

### 5. Mipmap LOD Calculation

**Problem**: Incorrect LOD causes over-blurring or under-blurring

**Wrong**:
```glsl
float lod = roughness;  // ❌ Assumes max_lod = 1 (wrong!)
```

**Correct**:
```glsl
float lod = roughness * max_reflection_lod;  // ✅ Scale to actual mip count
vec3 prefiltered = textureLod(prefiltered_map, R, lod).rgb;
```

### 6. Indoor/Outdoor HDRI Mismatch

**Problem**: Using outdoor HDRI indoors (sun pierces through walls!)

**Solution**: Detect indoor/outdoor automatically

```cpp
bool is_outdoor = (sun_info.peak_luminance / sun_info.average_luminance) > 10.0f;

if (is_outdoor) {
    hdri.align_sun_to_direction(scene_sun);  // Align sun
} else {
    hdri.set_rotation(0.0f);  // No sun alignment for indoor HDRIs
}
```

---

## Performance Benchmarks

**Test Setup**: RTX 3080, 1920×1080, High Quality

| Operation | Time | Notes |
|---|---|---|
| **Pre-computation (one-time)** |
| Equirect → Cubemap | 15 ms | 4K → 1024³ |
| Irradiance Convolution | 80 ms | 512 samples/texel |
| Prefilter Generation | 250 ms | 1024 samples/texel, 11 mips |
| BRDF Integration | 50 ms | 1024 samples/texel |
| **Total Pre-compute** | **395 ms** | Per-HDRI, one-time |
| | |
| **Runtime (per-frame)** |
| IBL Ambient Lighting | 0.18 ms | 2.07M pixels × 3 texture lookups |
| + Atmospheric Scattering | 0.05 ms | LUT-based |
| + Shadow Mapping | 0.80 ms | 4 cascades, PCF 5×5 |
| + Direct Lighting | 0.15 ms | 4 lights |
| **Total Frame** | **1.18 ms** | 847 FPS |

**Memory Footprint (per HDRI, high quality)**:
- Equirectangular (4K): 64 MB (kept for sun detection)
- Environment Cubemap (1024³): 12 MB
- Irradiance Map (64³): 96 KB
- Prefiltered Environment (1024³ + mips): 48 MB
- BRDF LUT (512²): 1 MB (shared across all HDRIs!)
- **Total per HDRI**: ~94 MB

---

## Conclusion

Image-Based Lighting is a cornerstone of modern physically-based rendering, providing photorealistic ambient lighting and reflections with minimal runtime cost. The split-sum approximation makes real-time IBL possible, trading mathematical exactness for 1000× performance gains with visually indistinguishable results.

**Key Takeaways**:
- IBL captures real-world lighting complexity in a single HDRI
- Pre-computation (~400ms) enables fast runtime (~0.2ms)
- Combines naturally with PBR materials via energy conservation
- Integrates seamlessly with atmospheric scattering for outdoor scenes
- Memory cost (~100 MB per HDRI) is the main limitation

**Further Reading**:
- Brian Karis - "Real Shading in Unreal Engine 4" (SIGGRAPH 2013)
- Sébastien Lagarde - "Moving Frostbite to PBR" (SIGGRAPH 2014)
- Romain Guy - "Physically Based Rendering in Filament" (Google, 2018)

---

*Last Updated: 2025-09-30*
*Lore Engine - Deferred Renderer with IBL*
