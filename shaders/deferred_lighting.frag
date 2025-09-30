#version 450

/**
 * DEFERRED LIGHTING PASS - FRAGMENT SHADER
 * ==========================================
 *
 * PURPOSE:
 *   This is the heart of the deferred renderer. It calculates final pixel colors
 *   by reading the G-Buffer and applying physically-based rendering (PBR) lighting.
 *
 * WHAT IS PBR?
 *   PBR (Physically-Based Rendering) simulates how light interacts with surfaces
 *   in the real world. It uses physics equations to calculate:
 *   - How much light bounces off (reflection)
 *   - How rough or smooth the surface is
 *   - Whether it's metallic or dielectric (non-metal)
 *
 * HOW IT WORKS:
 *   1. Sample G-Buffer to get surface properties at current pixel
 *   2. For each light in the scene:
 *      a. Calculate light direction and attenuation
 *      b. Use BRDF (Bidirectional Reflectance Distribution Function) to compute reflection
 *      c. Accumulate light contribution
 *   3. Add ambient lighting and emissive
 *   4. Output final HDR color
 *
 * BRDF COMPONENTS:
 *   - Diffuse: Soft, scattered light (like cloth, paper)
 *   - Specular: Sharp reflections (like metal, water)
 *   - Fresnel: Objects reflect more at grazing angles
 *
 * LIGHT TYPES:
 *   0 = Directional (sun, moon) - parallel rays, infinite distance
 *   1 = Point (light bulb) - radiates in all directions
 *   2 = Spot (flashlight) - cone of light
 */

#define PI 3.14159265359
#define MAX_LIGHTS 256

// ═══════════════════════════════════════════════════════════════
// INPUTS
// ═══════════════════════════════════════════════════════════════

// Texture coordinate from vertex shader
layout(location = 0) in vec2 fragTexCoord;

// G-Buffer textures (from geometry pass)
layout(set = 0, binding = 0) uniform sampler2D gAlbedoMetallic;    // RGB: albedo, A: metallic
layout(set = 0, binding = 1) uniform sampler2D gNormalRoughness;   // RGB: normal, A: roughness
layout(set = 0, binding = 2) uniform sampler2D gPositionAO;        // RGB: position, A: AO
layout(set = 0, binding = 3) uniform sampler2D gEmissive;          // RGB: emissive color

// Camera uniforms
layout(set = 1, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 position;  // Camera world position
} camera;

// Light data structure (must match C++ struct exactly)
struct Light {
    vec3 position;          // World position for point/spot
    float range;            // Attenuation distance
    vec3 direction;         // Direction for directional/spot
    float intensity;        // Light intensity multiplier
    vec3 color;             // RGB color
    float innerConeAngle;   // Inner cone angle (spot lights, radians)
    float outerConeAngle;   // Outer cone angle (spot lights, radians)
    uint lightType;         // 0=directional, 1=point, 2=spot
    uint castsShadows;      // 0 or 1
    uint padding;
};

// Lights buffer
layout(set = 2, binding = 0) readonly buffer LightsBuffer {
    uint lightCount;
    uint padding[3];        // Alignment padding
    Light lights[];
} lightsData;

// Shadow map samplers (cascaded shadow maps)
layout(set = 2, binding = 3) uniform sampler2DShadow shadowMaps[4];

// Shadow mapping uniforms
layout(set = 2, binding = 4) uniform ShadowUBO {
    mat4 cascadeViewProj[4];     // Light view-projection matrix for each cascade
    vec4 cascadeSplits;          // Split distances for cascade selection (x, y, z, w for 4 cascades)
    vec4 shadowParams;           // x: depth bias, y: normal bias, z: pcf radius, w: shadow softness
    vec4 shadowSettings;         // x: pcf kernel size (0=3x3, 1=5x5, 2=7x7), y: use poisson (0/1), z: max shadow distance, w: fade range
    vec3 lightDirection;         // Main directional light direction (for shadow mapping)
    float padding;
} shadow;

// ═══════════════════════════════════════════════════════════════
// ATMOSPHERIC SCATTERING UNIFORMS
// ═══════════════════════════════════════════════════════════════

// Atmospheric scattering uniforms
layout(set = 2, binding = 5) uniform sampler2D atmosphericTransmittance; // 256×64
layout(set = 2, binding = 6) uniform sampler3D atmosphericScattering;    // 200×128×32

layout(set = 2, binding = 7) uniform AtmosphericUBO {
    vec3 planet_center;           // Planet center (world space)
    float planet_radius;          // Planet radius (meters)
    
    float atmosphere_height;      // Atmosphere thickness
    float rayleigh_scale_height;  // Rayleigh scattering scale
    float mie_scale_height;       // Mie scattering scale
    vec3 rayleigh_scattering;     // Rayleigh coefficients
    
    vec3 mie_scattering;          // Mie coefficients
    float mie_anisotropy;         // Mie phase function g
    vec3 sun_direction;           // Primary sun direction
    float sun_intensity;          // Sun intensity
} atmospheric;

// ═══════════════════════════════════════════════════════════════
// OUTPUTS
// ═══════════════════════════════════════════════════════════════

layout(location = 0) out vec4 outColor;  // Final HDR color

// ═══════════════════════════════════════════════════════════════
// PBR HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Normal Distribution Function (NDF) - GGX/Trowbridge-Reitz
 * -----------------------------------------------------------
 * Describes the statistical distribution of microfacets on the surface.
 * Rough surfaces have more randomly oriented microfacets.
 *
 * Parameters:
 *   N = Surface normal
 *   H = Half-vector between view and light
 *   roughness = Surface roughness [0=smooth, 1=rough]
 *
 * Returns: Probability that microfacet is aligned with H
 */
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;

    return numerator / max(denominator, 0.0000001);  // Avoid division by zero
}

/**
 * Geometry Function - Smith's Schlick-GGX
 * ----------------------------------------
 * Describes self-shadowing of microfacets. Rough surfaces have more
 * microfacets blocking light from reaching other microfacets.
 *
 * Parameters:
 *   NdotV = Dot product of normal and view direction
 *   roughness = Surface roughness
 *
 * Returns: Percentage of microfacets not in shadow
 */
float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // Direct lighting

    float numerator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;

    return numerator / max(denominator, 0.0000001);
}

/**
 * Geometry Function - Smith's method (combines view and light)
 * -------------------------------------------------------------
 * Accounts for geometry obstruction from both view and light directions.
 *
 * Returns: Percentage of visible microfacets
 */
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = G_SchlickGGX(NdotL, roughness);  // Light direction
    float ggx2 = G_SchlickGGX(NdotV, roughness);  // View direction

    return ggx1 * ggx2;
}

/**
 * Fresnel Function - Schlick's approximation
 * -------------------------------------------
 * Describes how much light is reflected vs refracted at the surface.
 * Objects reflect more light at grazing angles (like looking at water).
 *
 * Parameters:
 *   F0 = Reflectance at normal incidence (head-on view)
 *   V = View direction
 *   H = Half vector
 *
 * Returns: Fresnel reflectance (how much light is reflected)
 */
vec3 F_Schlick(vec3 F0, vec3 V, vec3 H) {
    float cosTheta = max(dot(H, V), 0.0);
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/**
 * Fresnel with roughness - for rough surfaces
 * --------------------------------------------
 * Modifies Fresnel for rough surfaces (they reflect less)
 */
vec3 F_SchlickRoughness(vec3 F0, vec3 V, vec3 H, float roughness) {
    float cosTheta = max(dot(H, V), 0.0);
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ═══════════════════════════════════════════════════════════════
// SHADOW MAPPING FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Pre-computed Poisson Disk Samples (64 samples)
 * -----------------------------------------------
 * Poisson disk provides better distribution than regular grid sampling.
 * These samples are distributed with minimum distance between points,
 * reducing clustering artifacts in PCF soft shadows.
 *
 * The pattern is rotated per-pixel using noise to break up repetition.
 */
const vec2 POISSON_DISK_64[64] = vec2[](
    vec2(-0.613392, 0.617481),   vec2(0.170019, -0.040254),
    vec2(-0.299417, 0.791925),   vec2(0.645680, 0.493210),
    vec2(-0.651784, 0.717887),   vec2(0.421003, 0.027070),
    vec2(-0.817194, -0.271096),  vec2(-0.705374, -0.668203),
    vec2(0.977050, -0.108615),   vec2(0.063326, 0.142369),
    vec2(0.203528, 0.214331),    vec2(-0.667531, 0.326090),
    vec2(-0.098422, -0.295755),  vec2(-0.885922, 0.215369),
    vec2(0.566637, 0.605213),    vec2(0.039766, -0.396100),
    vec2(0.751946, 0.453352),    vec2(0.078707, -0.715323),
    vec2(-0.075838, -0.529344),  vec2(0.724479, -0.580798),
    vec2(0.222999, -0.215125),   vec2(-0.467574, -0.405438),
    vec2(-0.248268, -0.814753),  vec2(0.354411, -0.887570),
    vec2(0.175817, 0.382366),    vec2(0.487472, -0.063082),
    vec2(-0.084078, 0.898312),   vec2(0.488876, -0.783441),
    vec2(0.470016, 0.217933),    vec2(-0.696890, -0.549791),
    vec2(-0.149693, 0.605762),   vec2(0.034211, 0.979980),
    vec2(0.503098, -0.308878),   vec2(-0.016205, -0.872921),
    vec2(0.385784, -0.393902),   vec2(-0.146886, -0.859249),
    vec2(0.643361, 0.164098),    vec2(0.634388, -0.049471),
    vec2(-0.688894, 0.007843),   vec2(0.464034, -0.188818),
    vec2(-0.440840, 0.137486),   vec2(0.364483, 0.511704),
    vec2(0.034028, 0.325968),    vec2(0.099094, -0.308023),
    vec2(0.693960, -0.366253),   vec2(0.678884, -0.204688),
    vec2(0.001801, 0.780328),    vec2(0.145177, -0.898984),
    vec2(0.062655, -0.611866),   vec2(0.315226, -0.604297),
    vec2(-0.780145, 0.486251),   vec2(-0.371868, 0.882138),
    vec2(0.200476, 0.494430),    vec2(-0.494552, -0.711051),
    vec2(0.612476, 0.705252),    vec2(-0.578845, -0.768792),
    vec2(-0.772454, -0.090976),  vec2(0.504440, 0.372295),
    vec2(0.155736, 0.065157),    vec2(0.391522, 0.849605),
    vec2(-0.620106, -0.328104),  vec2(0.789239, -0.419965),
    vec2(-0.545396, 0.538133),   vec2(-0.178564, -0.596057)
);

/**
 * Interleaved Gradient Noise
 * ---------------------------
 * Fast, high-quality noise function for per-pixel randomization.
 * Used to rotate Poisson disk samples and break up shadow banding.
 *
 * Based on: "Next Generation Post Processing in Call of Duty: Advanced Warfare"
 * Reference: https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
 */
float interleavedGradientNoise(vec2 position) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position, magic.xy)));
}

/**
 * Rotate 2D vector by angle (in radians)
 * ---------------------------------------
 * Used to rotate Poisson disk samples for temporal/spatial variance.
 */
vec2 rotate2D(vec2 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

/**
 * Calculate view-space depth from world position
 * -----------------------------------------------
 * Used for cascade selection based on camera distance.
 */
float getViewSpaceDepth(vec3 worldPos) {
    vec4 viewPos = camera.view * vec4(worldPos, 1.0);
    return -viewPos.z;  // Negate because view space Z is negative in front of camera
}

/**
 * Select shadow cascade based on view-space depth
 * ------------------------------------------------
 * Returns cascade index (0-3) based on distance from camera.
 * Closer objects use higher-resolution cascades (0),
 * farther objects use lower-resolution cascades (3).
 *
 * Also returns blend factor for smooth cascade transitions.
 */
int selectCascade(float viewDepth, out float blendFactor) {
    // Initialize blend factor to 0 (no blending)
    blendFactor = 0.0;

    // Compare depth against cascade split distances
    // cascadeSplits = (split0, split1, split2, split3)
    if (viewDepth < shadow.cascadeSplits.x) {
        // Cascade 0 (closest, highest resolution)
        // Calculate blend factor for smooth transition to cascade 1
        float fadeStart = shadow.cascadeSplits.x * 0.9;
        blendFactor = smoothstep(fadeStart, shadow.cascadeSplits.x, viewDepth);
        return 0;
    } else if (viewDepth < shadow.cascadeSplits.y) {
        // Cascade 1
        float fadeStart = shadow.cascadeSplits.y * 0.9;
        blendFactor = smoothstep(fadeStart, shadow.cascadeSplits.y, viewDepth);
        return 1;
    } else if (viewDepth < shadow.cascadeSplits.z) {
        // Cascade 2
        float fadeStart = shadow.cascadeSplits.z * 0.9;
        blendFactor = smoothstep(fadeStart, shadow.cascadeSplits.z, viewDepth);
        return 2;
    } else if (viewDepth < shadow.cascadeSplits.w) {
        // Cascade 3 (farthest, lowest resolution)
        float fadeStart = shadow.cascadeSplits.w * 0.9;
        blendFactor = smoothstep(fadeStart, shadow.cascadeSplits.w, viewDepth);
        return 3;
    }

    // Beyond last cascade - no shadow
    return -1;
}

/**
 * Calculate shadow factor using PCF (Percentage Closer Filtering)
 * ----------------------------------------------------------------
 * PCF samples multiple points in the shadow map and averages the results,
 * creating soft shadow edges. We support three kernel sizes:
 * - 3×3: 9 samples (fast, moderate quality)
 * - 5×5: 25 samples (balanced)
 * - 7×7: 49 samples (high quality, expensive)
 *
 * Parameters:
 *   cascadeIndex = Which cascade to sample (0-3)
 *   shadowCoord = Position in light space (NDC)
 *   pcfRadius = Sampling radius in texel units
 *   kernelSize = 0 (3×3), 1 (5×5), 2 (7×7)
 *
 * Returns: Shadow factor [0 = fully shadowed, 1 = fully lit]
 */
float pcfShadow(int cascadeIndex, vec3 shadowCoord, float pcfRadius, int kernelSize) {
    // Get shadow map texel size for this cascade
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[cascadeIndex], 0));

    // Scale radius by texel size and PCF radius parameter
    vec2 sampleRadius = texelSize * pcfRadius;

    float shadow = 0.0;
    int sampleCount = 0;

    // Determine kernel size based on quality setting
    int kernelHalfSize = 1;  // Default 3×3
    if (kernelSize == 1) kernelHalfSize = 2;      // 5×5
    else if (kernelSize == 2) kernelHalfSize = 3; // 7×7

    // Sample shadow map in grid pattern
    for (int y = -kernelHalfSize; y <= kernelHalfSize; ++y) {
        for (int x = -kernelHalfSize; x <= kernelHalfSize; ++x) {
            // Calculate sample offset
            vec2 offset = vec2(float(x), float(y)) * sampleRadius;

            // Sample shadow map using hardware comparison
            // texture() returns 1.0 if shadowCoord.z < shadow map depth (lit)
            //                   0.0 if shadowCoord.z >= shadow map depth (shadowed)
            shadow += texture(shadowMaps[cascadeIndex], shadowCoord + vec3(offset, 0.0));
            sampleCount++;
        }
    }

    // Average all samples to get soft shadow percentage
    return shadow / float(sampleCount);
}

/**
 * Calculate shadow factor using Poisson disk sampling
 * ----------------------------------------------------
 * Poisson disk provides better sample distribution than regular grid,
 * reducing banding artifacts and providing more natural soft shadows.
 * Samples are rotated per-pixel using noise for temporal variance.
 *
 * Parameters:
 *   cascadeIndex = Which cascade to sample (0-3)
 *   shadowCoord = Position in light space (NDC)
 *   pcfRadius = Sampling radius in texel units
 *   fragCoord = Screen-space position for noise
 *
 * Returns: Shadow factor [0 = fully shadowed, 1 = fully lit]
 */
float poissonShadow(int cascadeIndex, vec3 shadowCoord, float pcfRadius, vec2 fragCoord) {
    // Get shadow map texel size for this cascade
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[cascadeIndex], 0));

    // Scale radius by texel size and PCF radius parameter
    vec2 sampleRadius = texelSize * pcfRadius;

    // Generate random rotation angle using interleaved gradient noise
    // This breaks up repetitive patterns and adds temporal variance
    float randomAngle = interleavedGradientNoise(fragCoord) * 2.0 * PI;

    float shadow = 0.0;

    // Sample shadow map using rotated Poisson disk (64 samples)
    for (int i = 0; i < 64; ++i) {
        // Rotate sample point by random angle
        vec2 offset = rotate2D(POISSON_DISK_64[i], randomAngle) * sampleRadius;

        // Sample shadow map using hardware comparison
        shadow += texture(shadowMaps[cascadeIndex], shadowCoord + vec3(offset, 0.0));
    }

    // Average all samples to get soft shadow percentage
    return shadow / 64.0;
}

/**
 * Main shadow calculation function
 * ---------------------------------
 * Calculates shadow factor for a given world position and surface normal.
 * Handles cascade selection, normal offset bias, PCF/Poisson sampling,
 * and shadow distance fading.
 *
 * Parameters:
 *   worldPos = World-space position of fragment
 *   normal = World-space surface normal
 *
 * Returns: Shadow factor [0 = fully shadowed, 1 = fully lit]
 *
 * SHADOW BIAS EXPLAINED:
 * ----------------------
 * Shadow acne occurs when the shadow map resolution is insufficient,
 * causing self-shadowing artifacts. We combat this with two types of bias:
 *
 * 1. DEPTH BIAS: Offset shadow map depth comparison by small amount
 *    - Pushes shadow "back" from surface
 *    - Can cause "peter-panning" (shadows detaching from objects)
 *
 * 2. NORMAL OFFSET BIAS: Move sample point along surface normal toward light
 *    - Prevents peter-panning
 *    - More robust than depth bias alone
 *    - Scale by texel size for consistent results across cascades
 *
 * The combination of both biases provides artifact-free shadows.
 */
float calculateShadow(vec3 worldPos, vec3 normal) {
    // ───────────────────────────────────────────────────────────
    // EARLY EXIT: Check if position is beyond max shadow distance
    // ───────────────────────────────────────────────────────────
    float viewDepth = getViewSpaceDepth(worldPos);
    float maxShadowDistance = shadow.shadowSettings.z;

    // Early exit if beyond maximum shadow distance
    if (maxShadowDistance > 0.0 && viewDepth > maxShadowDistance) {
        return 1.0;  // Fully lit (no shadow)
    }

    // ───────────────────────────────────────────────────────────
    // STEP 1: Select appropriate cascade based on distance
    // ───────────────────────────────────────────────────────────
    float cascadeBlendFactor;
    int cascadeIndex = selectCascade(viewDepth, cascadeBlendFactor);

    // Early exit if beyond last cascade
    if (cascadeIndex < 0) {
        // Apply distance-based fade out
        float fadeRange = shadow.shadowSettings.w;
        float fadeStart = maxShadowDistance - fadeRange;
        float fadeFactor = smoothstep(fadeStart, maxShadowDistance, viewDepth);
        return mix(1.0, 1.0, fadeFactor);  // Fade to no shadow
    }

    // ───────────────────────────────────────────────────────────
    // STEP 2: Apply normal offset bias
    // ───────────────────────────────────────────────────────────
    // Move sample point slightly toward the light along the normal
    // This prevents shadow acne while avoiding peter-panning

    // Calculate texel size for this cascade
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[cascadeIndex], 0));
    float texelWorldSize = length(texelSize);  // Approximate texel size in world units

    // Normal bias scaled by cascade texel size and user parameter
    // Larger texels (farther cascades) need more bias
    float normalBias = shadow.shadowParams.y * texelWorldSize;

    // Offset position along normal, scaled by angle to light
    // More offset when surface is perpendicular to light (prevents self-shadowing on steep slopes)
    float NdotL = max(dot(normal, -shadow.lightDirection), 0.0);
    float slopeBias = sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.001);  // tan(angle) = sqrt(1-cos²)/cos
    vec3 offsetPos = worldPos + normal * normalBias * (1.0 + slopeBias);

    // ───────────────────────────────────────────────────────────
    // STEP 3: Transform to light space (NDC coordinates)
    // ───────────────────────────────────────────────────────────
    vec4 shadowCoord4 = shadow.cascadeViewProj[cascadeIndex] * vec4(offsetPos, 1.0);
    vec3 shadowCoord = shadowCoord4.xyz / shadowCoord4.w;

    // Convert from Vulkan NDC [-1, 1] to texture coordinates [0, 1]
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

    // Apply depth bias (constant offset)
    shadowCoord.z -= shadow.shadowParams.x;

    // ───────────────────────────────────────────────────────────
    // STEP 4: Check if position is within shadow map bounds
    // ───────────────────────────────────────────────────────────
    // Coordinates outside [0, 1] are outside the shadow map frustum
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        // Outside shadow map - assume fully lit
        return 1.0;
    }

    // ───────────────────────────────────────────────────────────
    // STEP 5: Perform shadow sampling (PCF or Poisson)
    // ───────────────────────────────────────────────────────────
    float shadowFactor;

    // Check if Poisson disk sampling is enabled
    bool usePoisson = shadow.shadowSettings.y > 0.5;
    int kernelSize = int(shadow.shadowSettings.x);  // 0=3×3, 1=5×5, 2=7×7

    if (usePoisson) {
        // High-quality Poisson disk sampling (64 samples)
        shadowFactor = poissonShadow(cascadeIndex, shadowCoord, shadow.shadowParams.z, gl_FragCoord.xy);
    } else {
        // Standard PCF sampling (9/25/49 samples depending on kernel size)
        shadowFactor = pcfShadow(cascadeIndex, shadowCoord, shadow.shadowParams.z, kernelSize);
    }

    // ───────────────────────────────────────────────────────────
    // STEP 6: Apply soft shadow modifier
    // ───────────────────────────────────────────────────────────
    // The softness parameter allows artistic control over shadow hardness
    // 0.0 = hard shadows (binary), 1.0 = very soft shadows
    float softness = shadow.shadowParams.w;
    shadowFactor = mix(step(0.5, shadowFactor), shadowFactor, softness);

    // ───────────────────────────────────────────────────────────
    // STEP 7: Apply distance-based fade
    // ───────────────────────────────────────────────────────────
    // Smoothly fade out shadows near maximum distance
    if (maxShadowDistance > 0.0) {
        float fadeRange = shadow.shadowSettings.w;
        float fadeStart = maxShadowDistance - fadeRange;
        float fadeFactor = smoothstep(fadeStart, maxShadowDistance, viewDepth);
        shadowFactor = mix(shadowFactor, 1.0, fadeFactor);
    }

    // ───────────────────────────────────────────────────────────
    // STEP 8: Optional cascade blending
    // ───────────────────────────────────────────────────────────
    // Blend with next cascade at transition boundaries for seamless LOD
    // This prevents visible "seams" between cascades
    if (cascadeBlendFactor > 0.01 && cascadeIndex < 3) {
        // Sample next cascade
        int nextCascade = cascadeIndex + 1;

        // Transform to next cascade's light space
        vec4 nextShadowCoord4 = shadow.cascadeViewProj[nextCascade] * vec4(offsetPos, 1.0);
        vec3 nextShadowCoord = nextShadowCoord4.xyz / nextShadowCoord4.w;
        nextShadowCoord.xy = nextShadowCoord.xy * 0.5 + 0.5;
        nextShadowCoord.z -= shadow.shadowParams.x;

        // Sample next cascade if within bounds
        float nextShadowFactor = 1.0;
        if (nextShadowCoord.x >= 0.0 && nextShadowCoord.x <= 1.0 &&
            nextShadowCoord.y >= 0.0 && nextShadowCoord.y <= 1.0 &&
            nextShadowCoord.z >= 0.0 && nextShadowCoord.z <= 1.0) {

            if (usePoisson) {
                nextShadowFactor = poissonShadow(nextCascade, nextShadowCoord, shadow.shadowParams.z, gl_FragCoord.xy);
            } else {
                nextShadowFactor = pcfShadow(nextCascade, nextShadowCoord, shadow.shadowParams.z, kernelSize);
            }
        }

        // Blend between cascades
        shadowFactor = mix(shadowFactor, nextShadowFactor, cascadeBlendFactor);
    }

    return shadowFactor;
}

// ═══════════════════════════════════════════════════════════════
// ATMOSPHERIC SCATTERING FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Sample atmospheric scattering for a world position
 * ----------------------------------------------------
 * Samples precomputed LUTs to get atmospheric transmittance and in-scattering.
 * 
 * TRANSMITTANCE: How much direct light survives atmosphere
 *   - Controls how much the sun/lights are attenuated by distance
 *   - Makes sun red at sunset (blue light scattered away)
 *   - T = exp(-optical_depth), wavelength-dependent
 * 
 * IN-SCATTERING: Light scattered into view ray
 *   - Creates aerial perspective (fog effect)
 *   - Distant objects fade to sky color
 *   - Adds atmospheric glow around sun
 * 
 * LUT SAMPLING:
 * --------------
 * Transmittance LUT (2D): [altitude, view_zenith_angle]
 *   - altitude: 0 = surface, 1 = atmosphere top
 *   - view_zenith: angle from zenith (0 = straight up, 0.5 = horizon, 1 = down)
 * 
 * Scattering LUT (3D): [altitude, view_zenith_angle, sun_zenith_angle]
 *   - Includes sun position for accurate scattering color
 * 
 * Parameters:
 *   worldPos = Fragment world position
 *   viewDir = Direction from camera to fragment (normalized)
 *   transmittance = Output: RGB transmittance factor (0-1)
 *   inScattering = Output: RGB in-scattered radiance (HDR)
 */
void sampleAtmospheric(vec3 worldPos, vec3 viewDir, out vec3 transmittance, out vec3 inScattering) {
    // ───────────────────────────────────────────────────────────
    // STEP 1: Calculate altitude above planet surface
    // ───────────────────────────────────────────────────────────
    // Calculate vector from planet center to fragment
    vec3 toPlanet = worldPos - atmospheric.planet_center;
    float distanceFromCenter = length(toPlanet);
    
    // Calculate altitude: distance from center minus planet radius
    // altitude = 0 at surface, increases with height
    float altitude = distanceFromCenter - atmospheric.planet_radius;
    
    // Normalize altitude to [0, 1] range for LUT sampling
    // 0 = surface, 1 = top of atmosphere
    float h = clamp(altitude / atmospheric.atmosphere_height, 0.0, 1.0);
    
    // ───────────────────────────────────────────────────────────
    // STEP 2: Calculate view zenith angle
    // ───────────────────────────────────────────────────────────
    // Zenith direction: vector from planet center toward sky
    vec3 zenith = normalize(toPlanet);
    
    // Angle between view direction and zenith
    // cos(0°) = 1 (looking up), cos(90°) = 0 (horizon), cos(180°) = -1 (looking down)
    float cosViewZenith = dot(zenith, viewDir);
    
    // Convert to texture coordinate [0, 1]
    // 0 = looking down (-zenith), 0.5 = horizon, 1 = looking up (+zenith)
    float viewAngle = cosViewZenith * 0.5 + 0.5;
    
    // ───────────────────────────────────────────────────────────
    // STEP 3: Sample transmittance LUT (2D)
    // ───────────────────────────────────────────────────────────
    // Transmittance depends on altitude and view angle
    vec2 transUV = vec2(h, viewAngle);
    transmittance = texture(atmosphericTransmittance, transUV).rgb;
    
    // Ensure transmittance is in valid range [0, 1]
    // (LUT should already provide this, but clamp for safety)
    transmittance = clamp(transmittance, 0.0, 1.0);
    
    // ───────────────────────────────────────────────────────────
    // STEP 4: Calculate sun zenith angle
    // ───────────────────────────────────────────────────────────
    // Angle between sun direction and zenith
    // Used to determine scattering color (sunset vs noon)
    float cosSunZenith = dot(zenith, atmospheric.sun_direction);
    
    // Convert to texture coordinate [0, 1]
    float sunAngle = cosSunZenith * 0.5 + 0.5;
    
    // ───────────────────────────────────────────────────────────
    // STEP 5: Sample scattering LUT (3D)
    // ───────────────────────────────────────────────────────────
    // Scattering depends on altitude, view angle, and sun angle
    vec3 scatterUV = vec3(h, viewAngle, sunAngle);
    vec4 scatterSample = texture(atmosphericScattering, scatterUV);
    
    // Extract in-scattering radiance (RGB, HDR)
    // Alpha channel could store additional data (density, fog, etc.)
    inScattering = scatterSample.rgb;
    
    // Scale by sun intensity (convert from normalized to physical units)
    // Sun intensity is in W/m², scattering LUT is normalized
    inScattering *= atmospheric.sun_intensity;
    
    // Clamp to reasonable HDR range to prevent fireflies
    // Atmosphere can produce very bright values near sun
    inScattering = min(inScattering, vec3(10.0));
}

// ═══════════════════════════════════════════════════════════════
// LIGHT CALCULATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Calculate attenuation for point and spot lights
 * ------------------------------------------------
 * Light intensity decreases with distance (inverse-square law in real world).
 * We use a modified version that approaches zero smoothly at the light's range.
 *
 * Parameters:
 *   distance = Distance from light to surface
 *   range = Maximum range of light
 *
 * Returns: Attenuation factor [0, 1]
 */
float calculateAttenuation(float distance, float range) {
    // Prevent division by zero
    if (range <= 0.0001) return 0.0;

    // Physical attenuation (inverse square law)
    float attenuation = 1.0 / (distance * distance + 1.0);

    // Smoothly fade to zero at range limit
    float falloff = max(1.0 - (distance / range), 0.0);
    falloff = falloff * falloff;  // Square for smoother falloff

    return attenuation * falloff;
}

/**
 * Calculate spot light cone attenuation
 * --------------------------------------
 * Spot lights have a cone of light with smooth falloff at edges.
 *
 * Parameters:
 *   L = Light direction
 *   spotDir = Spot light direction
 *   innerAngle = Inner cone angle (full brightness)
 *   outerAngle = Outer cone angle (falloff to zero)
 *
 * Returns: Cone attenuation factor [0, 1]
 */
float calculateSpotAttenuation(vec3 L, vec3 spotDir, float innerAngle, float outerAngle) {
    float cosOuter = cos(outerAngle);
    float cosInner = cos(innerAngle);
    float cosAngle = dot(normalize(-L), normalize(spotDir));

    // Smooth interpolation between inner and outer cone
    float spotEffect = smoothstep(cosOuter, cosInner, cosAngle);
    return spotEffect;
}

/**
 * Calculate PBR lighting for a single light
 * ------------------------------------------
 * This is the main lighting equation that combines diffuse and specular
 * using the Cook-Torrance BRDF model.
 *
 * Parameters:
 *   N = Surface normal
 *   V = View direction (surface to camera)
 *   L = Light direction (surface to light)
 *   albedo = Base color
 *   metallic = Metallic factor [0=dielectric, 1=metal]
 *   roughness = Roughness [0=smooth, 1=rough]
 *   F0 = Fresnel reflectance at normal incidence
 *
 * Returns: Reflected radiance (light contribution)
 */
vec3 calculatePBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);  // Half vector between view and light

    // Calculate BRDF terms
    float NDF = D_GGX(N, H, roughness);           // Normal Distribution
    float G = G_Smith(N, V, L, roughness);        // Geometry term
    vec3 F = F_Schlick(F0, V, H);                 // Fresnel term

    // Cook-Torrance specular BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation: diffuse + specular = 1.0
    // kS is the specular contribution (how much light is reflected)
    // kD is the diffuse contribution (how much light is absorbed and scattered)
    vec3 kS = F;  // Fresnel tells us how much is reflected
    vec3 kD = vec3(1.0) - kS;  // Rest is diffused

    // Metals don't have diffuse reflection
    kD *= (1.0 - metallic);

    // Lambertian diffuse BRDF
    vec3 diffuse = kD * albedo / PI;

    // Final reflected light (diffuse + specular)
    float NdotL = max(dot(N, L), 0.0);
    return (diffuse + specular) * NdotL;
}

// ═══════════════════════════════════════════════════════════════
// MAIN SHADER
// ═══════════════════════════════════════════════════════════════

void main() {
    // ───────────────────────────────────────────────────────────
    // STEP 1: Sample G-Buffer
    // ───────────────────────────────────────────────────────────
    vec4 albedoMetallic = texture(gAlbedoMetallic, fragTexCoord);
    vec4 normalRoughness = texture(gNormalRoughness, fragTexCoord);
    vec4 positionAO = texture(gPositionAO, fragTexCoord);
    vec4 emissive = texture(gEmissive, fragTexCoord);

    // Unpack G-Buffer data
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N = normalize(normalRoughness.rgb * 2.0 - 1.0);  // Decode from [0,1] to [-1,1]
    float roughness = normalRoughness.a;
    vec3 worldPos = positionAO.rgb;
    float ao = positionAO.a;

    // ───────────────────────────────────────────────────────────
    // STEP 2: Calculate view direction
    // ───────────────────────────────────────────────────────────
    vec3 V = normalize(camera.position - worldPos);

    // ───────────────────────────────────────────────────────────
    // STEP 3: Calculate F0 (reflectance at normal incidence)
    // ───────────────────────────────────────────────────────────
    // F0 for dielectrics (non-metals) is typically 0.04 (4% reflection)
    // For metals, F0 is the albedo color (they reflect their color)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // ───────────────────────────────────────────────────────────
    // STEP 4: Accumulate lighting from all lights
    // ───────────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);  // Outgoing radiance (accumulated light)

    uint numLights = min(lightsData.lightCount, MAX_LIGHTS);
    for (uint i = 0; i < numLights; i++) {
        Light light = lightsData.lights[i];

        vec3 L;              // Light direction
        float attenuation;   // Light attenuation

        // Calculate L and attenuation based on light type
        if (light.lightType == 0) {
            // ═══ DIRECTIONAL LIGHT ═══
            L = normalize(-light.direction);
            attenuation = 1.0;  // No attenuation (infinite distance)

        } else if (light.lightType == 1) {
            // ═══ POINT LIGHT ═══
            vec3 lightVector = light.position - worldPos;
            float distance = length(lightVector);
            L = normalize(lightVector);
            attenuation = calculateAttenuation(distance, light.range);

        } else if (light.lightType == 2) {
            // ═══ SPOT LIGHT ═══
            vec3 lightVector = light.position - worldPos;
            float distance = length(lightVector);
            L = normalize(lightVector);

            // Distance attenuation
            attenuation = calculateAttenuation(distance, light.range);

            // Cone attenuation
            float spotEffect = calculateSpotAttenuation(L, light.direction,
                                                       light.innerConeAngle,
                                                       light.outerConeAngle);
            attenuation *= spotEffect;

        } else {
            continue;  // Unknown light type
        }

        // Skip if light has no effect
        if (attenuation < 0.001) continue;

        // Calculate radiance (light contribution)
        vec3 radiance = calculatePBR(N, V, L, albedo, metallic, roughness, F0);

        // ───────────────────────────────────────────────────────────
        // Apply shadows if light casts shadows
        // ───────────────────────────────────────────────────────────
        float shadowFactor = 1.0;  // Default: fully lit (no shadow)

        if (light.castsShadows == 1) {
            // Only directional lights cast shadows for now
            // (Point and spot light shadows require cubemap/perspective shadow maps)
            if (light.lightType == 0) {
                // Calculate shadow factor [0 = fully shadowed, 1 = fully lit]
                shadowFactor = calculateShadow(worldPos, N);
            }
        }

        // Accumulate with light color, intensity, attenuation, and shadow
        // Shadow factor multiplicatively darkens the light contribution
        // shadowFactor = 0.0 -> completely in shadow (no light contribution)
        // shadowFactor = 1.0 -> fully lit (full light contribution)
        // shadowFactor = 0.5 -> partially shadowed (50% light contribution)
        Lo += radiance * light.color * light.intensity * attenuation * shadowFactor;
    }

    // ───────────────────────────────────────────────────────────
    // STEP 5: Add ambient lighting
    // ───────────────────────────────────────────────────────────
    // Simple ambient term (in a full engine, this would be IBL)
    vec3 ambient = vec3(0.03) * albedo * ao;

    // ───────────────────────────────────────────────────────────
    // STEP 6: Add emissive contribution
    // ───────────────────────────────────────────────────────────
    vec3 color = ambient + Lo + emissive.rgb;

    // ───────────────────────────────────────────────────────────
    // STEP 6.5: Apply atmospheric scattering
    // ───────────────────────────────────────────────────────────
    // Apply realistic atmospheric effects based on precomputed LUTs.
    // This creates distance fog, aerial perspective, and atmospheric
    // light attenuation (red sunset, blue daytime sky).
    //
    // TRANSMITTANCE: Attenuates direct light based on atmospheric optical depth
    //   - Makes sun/lights redder at sunset (blue scattered away)
    //   - Exponential falloff: T = exp(-optical_depth)
    //
    // IN-SCATTERING: Adds atmospheric glow and haze
    //   - Distant objects fade to sky color (aerial perspective)
    //   - Creates volumetric fog effect
    //   - Adds atmospheric glow around sun
    //
    // Physical equation: L_final = L_direct * transmittance + L_inscatter
    vec3 transmittance, inScattering;
    sampleAtmospheric(worldPos, V, transmittance, inScattering);
    
    // Apply transmittance to attenuate direct/ambient light
    // Direct light (from lights) dims with distance through atmosphere
    // This makes distant lights appear weaker and redder
    color = color * transmittance;
    
    // Add in-scattered radiance (aerial perspective)
    // This is light scattered into the view ray from atmospheric particles
    // Creates distance fog that fades objects to sky color
    // Near objects: minimal scattering, retain original color
    // Far objects: heavy scattering, fade to atmospheric color
    color += inScattering;

    // ───────────────────────────────────────────────────────────
    // STEP 7: Output final HDR color
    // ───────────────────────────────────────────────────────────
    // Note: This is HDR (can be > 1.0). Tone mapping happens later.
    outColor = vec4(color, 1.0);
}

/**
 * IMPLEMENTED FEATURES:
 * ---------------------
 * ✓ Physically-Based Rendering (Cook-Torrance BRDF)
 * ✓ Cascaded Shadow Maps (4 cascades)
 * ✓ PCF Soft Shadows (3×3, 5×5, 7×7 kernels)
 * ✓ Poisson Disk Sampling (64 samples)
 * ✓ Normal Offset Bias (prevents shadow acne and peter-panning)
 * ✓ Cascade Blending (seamless LOD transitions)
 * ✓ Distance-based Shadow Fade
 * ✓ Slope-scaled Bias (handles steep surfaces)
 * ✓ Atmospheric Scattering (Rayleigh + Mie physics)
 * ✓ Transmittance (wavelength-dependent light attenuation)
 * ✓ In-Scattering (aerial perspective, distance fog)
 * ✓ LUT-based Atmospheric Sampling (256×64 transmittance, 200×128×32 scattering)
 *
 * ATMOSPHERIC EFFECTS:
 * --------------------
 * - Realistic sunset/sunrise colors (red sun, blue sky)
 * - Distance fog with physically-correct falloff
 * - Aerial perspective (distant objects fade to sky color)
 * - Altitude-based atmospheric density
 * - Sun angle affects scattering color (noon vs sunset)
 * - Supports multiple planet atmospheres (Earth, Mars, etc.)
 *
 * FUTURE ENHANCEMENTS:
 * --------------------
 * 1. Point Light Shadows: Cubemap shadow maps for omnidirectional shadows
 * 2. Spot Light Shadows: Perspective shadow maps with cone attenuation
 * 3. Contact-Hardening Shadows: Variable penumbra width based on blocker distance
 * 4. PCSS (Percentage-Closer Soft Shadows): More realistic soft shadows
 * 5. VSM/ESM: Variance/Exponential shadow maps for better filtering
 * 6. IBL (Image-Based Lighting): Use environment maps for ambient
 * 7. SSAO: Screen-space ambient occlusion for better ambient
 * 8. Subsurface Scattering: For skin, wax, translucent materials
 * 9. Clearcoat: For car paint, lacquer, multi-layer materials
 * 10. Anisotropic Highlights: For brushed metal, hair, fabric
 * 11. Volumetric Atmospheric Effects: God rays, light shafts
 * 12. Cloud Shadows: Dynamic cloud coverage affecting light transmission
 */
