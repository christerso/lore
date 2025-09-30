# Deferred Rendering System

## Table of Contents
1. [What is Deferred Rendering?](#what-is-deferred-rendering)
2. [Why Use Deferred Rendering?](#why-use-deferred-rendering)
3. [How Deferred Rendering Works](#how-deferred-rendering-works)
4. [G-Buffer Design](#g-buffer-design)
5. [Implementation Strategy](#implementation-strategy)
6. [PBR Integration](#pbr-integration)
7. [Performance Considerations](#performance-considerations)
8. [Pseudocode](#pseudocode)
9. [Vulkan-Specific Details](#vulkan-specific-details)

---

## What is Deferred Rendering?

Deferred rendering is a rendering technique that **separates geometry processing from lighting calculations**. Instead of calculating lighting for each object as it's drawn (forward rendering), deferred rendering:

1. **First Pass (Geometry Pass)**: Renders all geometry and stores surface properties (position, normal, albedo, etc.) into multiple textures called a **G-Buffer** (Geometry Buffer)

2. **Second Pass (Lighting Pass)**: Processes all lights using the G-Buffer data, calculating final pixel colors

This decoupling allows for **hundreds or thousands of lights** with minimal performance impact, as lighting is calculated once per screen pixel rather than once per object per light.

### Key Concept

```
Forward Rendering:
  For each object:
    For each light:
      Calculate lighting

Deferred Rendering:
  For each object:
    Store geometry data to G-Buffer
  For each screen pixel:
    For each light:
      Calculate lighting using G-Buffer data
```

The critical insight: In deferred rendering, lighting cost is **O(screen pixels × lights)** instead of **O(geometry × lights)**.

---

## Why Use Deferred Rendering?

### Advantages

1. **Many Lights**: Support hundreds/thousands of dynamic lights efficiently
2. **Consistent Performance**: Lighting cost depends on screen resolution, not scene complexity
3. **Post-Processing**: G-Buffer provides rich data for effects (SSAO, SSR, etc.)
4. **Decoupled Passes**: Geometry and lighting are independent, easier to optimize separately
5. **PBR-Friendly**: Perfect fit for physically-based rendering workflows

### Disadvantages

1. **Memory Bandwidth**: Multiple large textures (G-Buffer) consume significant bandwidth
2. **No Hardware MSAA**: Anti-aliasing requires alternative techniques (FXAA, TAA)
3. **Transparency**: Cannot handle transparent objects in deferred pass (requires forward pass)
4. **Memory Usage**: G-Buffer can be 100+MB at 4K resolution

### When to Use

- **Many dynamic lights** (10+)
- **PBR workflow** with complex materials
- **Post-processing heavy** games
- **Consistent frame times** more important than peak performance
- **Desktop/Console** targets with sufficient VRAM

---

## How Deferred Rendering Works

### High-Level Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│                     DEFERRED RENDERING                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. GEOMETRY PASS                                           │
│     ┌──────────┐                                            │
│     │  Scene   │                                            │
│     │ Geometry │ ──────────────┐                            │
│     └──────────┘                │                           │
│                                 ▼                           │
│                         ┌───────────────┐                   │
│                         │   G-BUFFER    │                   │
│                         ├───────────────┤                   │
│                         │ Albedo+Metal  │                   │
│                         │ Normal+Rough  │                   │
│                         │ Position+AO   │                   │
│                         │   Emissive    │                   │
│                         │     Depth     │                   │
│                         └───────────────┘                   │
│                                 │                           │
│  2. LIGHTING PASS               │                           │
│     ┌──────────┐                │                           │
│     │  Lights  │ ───────┐       │                           │
│     └──────────┘        │       │                           │
│                         ▼       ▼                           │
│                    ┌─────────────────┐                      │
│                    │  PBR Lighting   │                      │
│                    │  + BRDF Calc    │                      │
│                    └─────────────────┘                      │
│                            │                                │
│                            ▼                                │
│  3. POST-PROCESSING   ┌──────────┐                          │
│                       │   HDR    │                          │
│                       │ Tone Map │                          │
│                       │   Bloom  │                          │
│                       └──────────┘                          │
│                            │                                │
│                            ▼                                │
│                      ┌──────────┐                           │
│                      │  Final   │                           │
│                      │  Image   │                           │
│                      └──────────┘                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step-by-Step Execution

#### 1. Geometry Pass
```
For each visible object:
  1. Bind material properties (albedo, roughness, metallic, etc.)
  2. Bind textures (albedo map, normal map, etc.)
  3. Draw geometry
  4. Fragment shader writes to G-Buffer:
     - Albedo + Metallic → Attachment 0
     - Normal + Roughness → Attachment 1
     - Position + AO → Attachment 2
     - Emissive → Attachment 3
     - Depth → Depth attachment
```

**Result**: Screen-space buffers containing all geometry information

#### 2. Lighting Pass
```
For each screen pixel:
  1. Sample G-Buffer at pixel location
  2. Reconstruct surface properties:
     - World position
     - Surface normal
     - Material properties (albedo, metallic, roughness)

  3. For each light affecting this pixel:
     - Calculate light direction and attenuation
     - Compute BRDF (Bidirectional Reflectance Distribution Function)
     - Add contribution to final color

  4. Add emissive contribution
  5. Write final lit color to screen
```

**Result**: Fully lit image ready for post-processing

#### 3. Post-Processing (Optional)
```
Apply effects in order:
  1. SSAO (Screen-Space Ambient Occlusion)
  2. SSR (Screen-Space Reflections)
  3. Bloom
  4. Tone mapping (HDR → LDR)
  5. Color grading
  6. Anti-aliasing (FXAA/TAA)
```

---

## G-Buffer Design

### Texture Layout

Our G-Buffer uses **5 attachments** for optimal data storage:

```c++
Attachment 0: RGBA8        - Albedo (RGB) + Metallic (A)
Attachment 1: RGBA16F      - Normal (RGB) + Roughness (A)
Attachment 2: RGBA16F      - Position (RGB) + AO (A)
Attachment 3: RGBA16F      - Emissive (RGB)
Attachment 4: D32_S8_UINT  - Depth + Stencil
```

### Data Encoding

#### Albedo + Metallic (RGBA8)
- **RGB**: Base color (sRGB color space)
- **A**: Metallic factor [0-1]
- **Size**: 32 bits/pixel
- **Rationale**: Albedo doesn't need high precision, 8 bits per channel is sufficient

#### Normal + Roughness (RGBA16F)
- **RGB**: World-space normal encoded as `(normal * 0.5 + 0.5)` to fit [0,1] range
- **A**: Surface roughness [0-1]
- **Size**: 64 bits/pixel
- **Rationale**: Normals need precision to avoid banding in lighting calculations

#### Position + AO (RGBA16F)
- **RGB**: World-space position (XYZ)
- **A**: Ambient occlusion factor [0-1]
- **Size**: 64 bits/pixel
- **Rationale**: Position needs high precision for accurate lighting calculations

#### Emissive (RGBA16F)
- **RGB**: Emissive color (HDR values supported)
- **A**: Unused (could be used for bloom mask)
- **Size**: 64 bits/pixel
- **Rationale**: Emissive can exceed [0,1] range for bright lights

#### Depth (D32_S8)
- **D32**: 32-bit depth for precise depth testing
- **S8**: 8-bit stencil for masking operations
- **Size**: 40 bits/pixel

### Memory Calculation

For 1920×1080 resolution:
```
Albedo:   1920 × 1080 × 4 bytes  = 8.29 MB
Normal:   1920 × 1080 × 8 bytes  = 16.59 MB
Position: 1920 × 1080 × 8 bytes  = 16.59 MB
Emissive: 1920 × 1080 × 8 bytes  = 16.59 MB
Depth:    1920 × 1080 × 5 bytes  = 10.37 MB
─────────────────────────────────────────────
Total:                            = 68.43 MB
```

For 3840×2160 (4K):
```
Total G-Buffer size: ~274 MB
```

---

## Implementation Strategy

### Phase 1: Core G-Buffer Setup

1. **Create G-Buffer textures** with proper formats
2. **Create render pass** with multiple color attachments
3. **Create framebuffer** binding all attachments
4. **Set up descriptor sets** for sampling G-Buffer in lighting pass

### Phase 2: Geometry Pass

1. **Write geometry pass shaders**:
   - Vertex: Transform vertices, pass data to fragment
   - Fragment: Sample textures, write to G-Buffer

2. **Create geometry pipeline**:
   - Depth test: ENABLED
   - Depth write: ENABLED
   - Blending: DISABLED (opaque geometry only)
   - Color attachments: 4 (albedo, normal, position, emissive)

3. **Render all opaque geometry**:
   - Bind per-object descriptors (model matrix, material)
   - Issue draw calls

### Phase 3: Lighting Pass

1. **Write lighting pass shaders**:
   - Vertex: Full-screen quad
   - Fragment: Sample G-Buffer, calculate PBR lighting

2. **Create lighting pipeline**:
   - Depth test: DISABLED (full-screen pass)
   - Blending: DISABLED
   - Input: G-Buffer textures
   - Output: HDR color buffer

3. **For each light**:
   - Calculate light contribution using BRDF
   - Accumulate to final color

### Phase 4: Integration

1. **Modify main render loop**:
   ```
   begin_frame()
   ├─ begin_geometry_pass()
   ├─ render_opaque_geometry()
   ├─ end_geometry_pass()
   ├─ begin_lighting_pass()
   ├─ render_fullscreen_quad()
   ├─ end_lighting_pass()
   └─ present()
   ```

2. **Add transparent objects** (forward rendering):
   - Render after lighting pass
   - Use lighting data from G-Buffer if needed

---

## PBR Integration

### Cook-Torrance BRDF

The **Bidirectional Reflectance Distribution Function** (BRDF) determines how light reflects off a surface.

```
BRDF = kD * Diffuse + kS * Specular

Where:
  kD = (1 - metallic) * (1 - F)  // Diffuse coefficient
  kS = F                         // Specular coefficient
  F  = Fresnel term
```

#### Diffuse Term (Lambertian)
```glsl
vec3 diffuse = albedo / PI;
```

#### Specular Term (Cook-Torrance)
```glsl
float D = D_GGX(N, H, roughness);          // Normal Distribution
float G = G_SmithGGX(N, V, L, roughness);  // Geometry term
vec3  F = F_Schlick(F0, V, H);             // Fresnel term

vec3 specular = (D * G * F) / (4.0 * max(dot(N,V), 0.0) * max(dot(N,L), 0.0));
```

#### Complete Lighting Equation
```glsl
vec3 calculatePBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);  // Half vector

    // Fresnel at normal incidence (F0)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Calculate BRDF terms
    float NDF = D_GGX(N, H, roughness);
    float G = G_SmithGGX(N, V, L, roughness);
    vec3 F = F_Schlick(F0, V, H);

    // Specular component
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // Diffuse component
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Final radiance
    float NdotL = max(dot(N, L), 0.0);
    return (diffuse + specular) * lightColor * lightIntensity * NdotL;
}
```

### PBR Functions

#### Normal Distribution Function (GGX/Trowbridge-Reitz)
```glsl
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
```

#### Geometry Function (Smith's method with Schlick-GGX)
```glsl
float G_SchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float G_SmithGGX(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = G_SchlickGGX(NdotV, roughness);
    float ggx1 = G_SchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
```

#### Fresnel Function (Schlick approximation)
```glsl
vec3 F_Schlick(vec3 F0, vec3 V, vec3 H) {
    float cosTheta = max(dot(H, V), 0.0);
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

---

## Performance Considerations

### Optimization Strategies

1. **Light Culling**
   - Use compute shaders to cull lights per tile (Tiled Deferred)
   - Or per cluster (Clustered Deferred) for better precision
   ```
   Naive:     O(pixels × lights)
   Tiled:     O(tiles × lights + pixels)
   Clustered: O(clusters × lights + pixels)
   ```

2. **G-Buffer Compression**
   - Pack normals: Use octahedral encoding (2×16-bit instead of 3×16-bit)
   - Pack material: Combine multiple properties per channel
   - Trade: Memory bandwidth ↔ ALU cost

3. **Bandwidth Optimization**
   - Render opaque front-to-back (early Z-rejection)
   - Use smaller G-Buffer formats when possible
   - Enable compression (texture compression won't help much)

4. **Light Volume Rendering**
   - Instead of full-screen quad, render light volumes (spheres for point lights)
   - Only process pixels inside light's influence
   - Requires depth testing against G-Buffer

5. **Async Compute**
   - Use Vulkan async compute for lighting pass
   - Overlap lighting with next frame's geometry pass
   - Requires careful synchronization

### Bandwidth Calculation

```
Geometry Pass Write:
  4 attachments × 8 bytes/pixel = 32 bytes/pixel

Lighting Pass Read:
  Sample 4 attachments = 32 bytes/pixel

Total per frame (1080p):
  Write: 1920 × 1080 × 32 = 66.4 MB
  Read:  1920 × 1080 × 32 = 66.4 MB
  Total: 132.8 MB/frame

  At 60 FPS: 7.97 GB/s bandwidth
```

---

## Pseudocode

### Complete Rendering Loop

```cpp
// Initialization
void initialize() {
    create_gbuffer();
    create_geometry_pass();
    create_lighting_pass();
    load_pbr_shaders();
}

// Main render function
void render(Scene scene, Camera camera) {
    // ═══════════════════════════════════════
    // PHASE 1: GEOMETRY PASS
    // ═══════════════════════════════════════
    begin_geometry_pass();
    {
        bind_geometry_pipeline();
        bind_camera_ubo(camera);

        for (Object obj : scene.opaque_objects) {
            bind_model_matrix(obj.transform);
            bind_material(obj.material);
            bind_textures(obj.textures);
            draw_mesh(obj.mesh);
        }
    }
    end_geometry_pass();

    // ═══════════════════════════════════════
    // PHASE 2: LIGHTING PASS
    // ═══════════════════════════════════════
    begin_lighting_pass();
    {
        bind_lighting_pipeline();
        bind_gbuffer_textures();
        bind_camera_position(camera);
        bind_lights(scene.lights);

        // Draw full-screen quad
        draw_fullscreen_quad();
    }
    end_lighting_pass();

    // ═══════════════════════════════════════
    // PHASE 3: FORWARD PASS (Transparent)
    // ═══════════════════════════════════════
    begin_forward_pass();
    {
        bind_forward_pipeline();

        // Sort transparent objects back-to-front
        sort(scene.transparent_objects, back_to_front);

        for (Object obj : scene.transparent_objects) {
            bind_model_matrix(obj.transform);
            bind_material(obj.material);
            draw_mesh(obj.mesh);
        }
    }
    end_forward_pass();

    // ═══════════════════════════════════════
    // PHASE 4: POST-PROCESSING
    // ═══════════════════════════════════════
    apply_ssao();
    apply_bloom();
    apply_tone_mapping();
    apply_fxaa();
}
```

### Geometry Pass Shader (Fragment)

```glsl
// Inputs
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragTexCoord;
in mat3 fragTBN;

// Material
uniform Material {
    vec3 albedo;
    float metallic;
    float roughness;
    vec3 emissive;
    float ao;
} material;

// Outputs to G-Buffer
layout(location = 0) out vec4 gAlbedoMetallic;
layout(location = 1) out vec4 gNormalRoughness;
layout(location = 2) out vec4 gPositionAO;
layout(location = 3) out vec4 gEmissive;

void main() {
    // Sample textures
    vec3 albedo = sample_albedo(material, fragTexCoord);
    vec3 normal = sample_normal(material, fragTexCoord, fragTBN);
    float metallic = sample_metallic(material, fragTexCoord);
    float roughness = sample_roughness(material, fragTexCoord);
    vec3 emissive = sample_emissive(material, fragTexCoord);
    float ao = sample_ao(material, fragTexCoord);

    // Write to G-Buffer
    gAlbedoMetallic = vec4(albedo, metallic);
    gNormalRoughness = vec4(encode_normal(normal), roughness);
    gPositionAO = vec4(fragWorldPos, ao);
    gEmissive = vec4(emissive, 1.0);
}
```

### Lighting Pass Shader (Fragment)

```glsl
// G-Buffer inputs
uniform sampler2D gAlbedoMetallic;
uniform sampler2D gNormalRoughness;
uniform sampler2D gPositionAO;
uniform sampler2D gEmissive;

// Camera
uniform vec3 cameraPos;

// Lights
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float range;
    int type; // 0=directional, 1=point, 2=spot
};
uniform Light lights[MAX_LIGHTS];
uniform int numLights;

// Output
out vec4 fragColor;

void main() {
    // Sample G-Buffer at current pixel
    vec2 uv = gl_FragCoord.xy / screenSize;
    vec4 albedoMetallic = texture(gAlbedoMetallic, uv);
    vec4 normalRoughness = texture(gNormalRoughness, uv);
    vec4 positionAO = texture(gPositionAO, uv);
    vec4 emissive = texture(gEmissive, uv);

    // Unpack G-Buffer data
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N = decode_normal(normalRoughness.rgb);
    float roughness = normalRoughness.a;
    vec3 worldPos = positionAO.rgb;
    float ao = positionAO.a;

    // Calculate view direction
    vec3 V = normalize(cameraPos - worldPos);

    // Accumulate lighting
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < numLights; i++) {
        vec3 L = calculate_light_direction(lights[i], worldPos);
        float attenuation = calculate_attenuation(lights[i], worldPos);

        vec3 radiance = calculate_pbr(N, V, L, albedo, metallic, roughness);
        Lo += radiance * lights[i].color * lights[i].intensity * attenuation;
    }

    // Add ambient term
    vec3 ambient = vec3(0.03) * albedo * ao;

    // Add emissive
    vec3 color = ambient + Lo + emissive.rgb;

    // Output HDR color
    fragColor = vec4(color, 1.0);
}
```

---

## Vulkan-Specific Details

### Render Pass Creation

```cpp
// Create render pass for geometry pass
VkAttachmentDescription attachments[5];

// Attachment 0: Albedo + Metallic
attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

// Attachment 1: Normal + Roughness
attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
// ... similar setup

// Attachment 2: Position + AO
// Attachment 3: Emissive
// Attachment 4: Depth

VkAttachmentReference colorRefs[4] = {
    {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
};

VkAttachmentReference depthRef = {
    4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
};

VkSubpassDescription subpass = {};
subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
subpass.colorAttachmentCount = 4;
subpass.pColorAttachments = colorRefs;
subpass.pDepthStencilAttachment = &depthRef;

VkRenderPassCreateInfo renderPassInfo = {};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
renderPassInfo.attachmentCount = 5;
renderPassInfo.pAttachments = attachments;
renderPassInfo.subpassCount = 1;
renderPassInfo.pSubpasses = &subpass;

vkCreateRenderPass(device, &renderPassInfo, nullptr, &geometryRenderPass);
```

### Synchronization

```cpp
// Transition G-Buffer from write to read
VkImageMemoryBarrier barriers[4];
for (int i = 0; i < 4; i++) {
    barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[i].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[i].image = gbufferImages[i];
    // ... setup subresource range
}

vkCmdPipelineBarrier(
    cmd,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,
    0, nullptr,
    0, nullptr,
    4, barriers
);
```

---

## Summary

Deferred rendering is a powerful technique that:
- ✅ Enables hundreds of dynamic lights
- ✅ Simplifies complex lighting calculations
- ✅ Provides rich data for post-processing
- ✅ Perfectly suited for PBR workflows

By separating geometry and lighting into distinct passes, we gain flexibility and performance for modern, visually rich 3D games.

**Implementation Checklist**:
1. ✅ Create G-Buffer with appropriate formats
2. ✅ Implement geometry pass shaders
3. ✅ Implement lighting pass with PBR BRDF
4. ⬜ Add shadow mapping
5. ⬜ Implement SSAO
6. ⬜ Add HDR and tone mapping
7. ⬜ Optimize with light culling

---

**References**:
- [LearnOpenGL - Deferred Shading](https://learnopengl.com/Advanced-Lighting/Deferred-Shading)
- [Real Shading in Unreal Engine 4](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
- [Physically Based Rendering in Filament](https://google.github.io/filament/Filament.html)