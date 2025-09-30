#version 450

/**
 * SHADOW DEPTH PASS - FRAGMENT SHADER
 * =====================================
 *
 * PURPOSE:
 *   Minimal fragment shader for depth-only shadow map rendering.
 *   In most cases, this shader does nothing - depth is written automatically.
 *   However, we need it for alpha testing (vegetation, foliage, chains, etc.)
 *
 * WHAT IS ALPHA TESTING?
 *   Some objects like leaves, grass, or chain-link fences have transparent
 *   areas defined by their texture's alpha channel. We need to discard these
 *   fragments so they don't cast shadows.
 *
 *   Example: A leaf texture is green where the leaf is, and transparent
 *   everywhere else. Without alpha testing, the entire quad would cast a
 *   rectangular shadow. With alpha testing, only the leaf shape casts shadow.
 *
 * HOW IT WORKS:
 *   1. Sample the albedo texture's alpha channel
 *   2. If alpha < threshold (usually 0.5), discard the fragment
 *   3. Otherwise, let the depth value write to shadow map
 *
 * OPTIMIZATION NOTES:
 *   - Most objects skip alpha testing entirely (controlled by material flag)
 *   - Early-Z optimization still works with discard (on modern GPUs)
 *   - Keep alpha threshold consistent with main rendering pass
 *   - Consider using alpha-to-coverage for smoother results
 */

// ═══════════════════════════════════════════════════════════════
// INPUTS
// ═══════════════════════════════════════════════════════════════

// Texture coordinate from vertex shader (only used if alpha testing enabled)
layout(location = 0) in vec2 fragTexCoord;

// ═══════════════════════════════════════════════════════════════
// TEXTURES
// ═══════════════════════════════════════════════════════════════

// Albedo texture with alpha channel
// Only bound when rendering alpha-tested materials (vegetation, etc.)
layout(set = 0, binding = 0) uniform sampler2D albedoTexture;

// ═══════════════════════════════════════════════════════════════
// PUSH CONSTANTS
// ═══════════════════════════════════════════════════════════════

// Material flags to control alpha testing
layout(push_constant) uniform MaterialFlags {
    layout(offset = 128) uint flags;  // Offset past the matrices in push constants
} material;

// Flag bits
#define ALPHA_TESTED (1u << 0)  // Bit 0: Enable alpha testing
#define DOUBLE_SIDED (1u << 1)  // Bit 1: Disable backface culling

// Alpha test threshold
#define ALPHA_THRESHOLD 0.5

// ═══════════════════════════════════════════════════════════════
// OUTPUTS
// ═══════════════════════════════════════════════════════════════

// No explicit outputs - depth is written automatically to gl_FragDepth

// ═══════════════════════════════════════════════════════════════
// MAIN SHADER
// ═══════════════════════════════════════════════════════════════

void main() {
    // Check if alpha testing is enabled for this material
    bool alphaTest = (material.flags & ALPHA_TESTED) != 0;

    if (alphaTest) {
        // Sample the albedo texture's alpha channel
        float alpha = texture(albedoTexture, fragTexCoord).a;

        // Discard fragments with low alpha (transparent areas)
        // These fragments won't cast shadows
        if (alpha < ALPHA_THRESHOLD) {
            discard;
        }

        // Note: Some engines use alpha-to-coverage (MSAA) instead of hard cutoff
        // This provides smoother shadow edges for vegetation:
        // - alpha-to-coverage converts alpha to MSAA coverage mask
        // - Requires MSAA shadow maps (more memory but better quality)
        // - Enable with VkPipelineMultisampleStateCreateInfo::alphaToCoverageEnable
    }

    // If we reach here, the fragment passes and its depth will be written
    // Depth is automatically written by the GPU to the shadow map
    // No need to explicitly set gl_FragDepth unless we want to modify it

    // Optional: Manual depth bias (usually done in rasterization state instead)
    // gl_FragDepth = gl_FragCoord.z + depthBias;

    // No color output - we're rendering to a depth-only framebuffer
}

/**
 * TECHNICAL DETAILS:
 * -------------------
 *
 * Depth Output:
 * - gl_FragDepth is automatically set to gl_FragCoord.z
 * - We don't need to explicitly write it unless applying manual bias
 * - Vulkan uses reverse-Z: 1.0 = near plane, 0.0 = far plane
 *
 * Alpha Testing vs Alpha Blending:
 * - Alpha testing (discard): Binary decision, fragment exists or doesn't
 * - Alpha blending: Smooth transparency, requires sorting and blending
 * - Shadow maps use alpha testing because blending doesn't make sense for depth
 *
 * Performance Considerations:
 * - discard can hurt performance by disabling early-Z
 * - Modern GPUs handle it well for shadow maps
 * - Consider separate pipelines: one for opaque (no alpha test), one for masked
 *
 * Depth Bias Methods:
 * 1. Rasterization state bias (preferred):
 *    - VkPipelineRasterizationStateCreateInfo::depthBiasEnable
 *    - Hardware-accelerated, no shader overhead
 *    - Applies slope-scaled bias automatically
 *
 * 2. Shader bias (manual):
 *    - gl_FragDepth = gl_FragCoord.z + bias;
 *    - More control but disables early-Z optimization
 *    - Useful for special cases (slope bias, normal bias)
 *
 * 3. Normal offset bias (in vertex shader):
 *    - Offset vertices along normal direction
 *    - Prevents "peter-panning" (shadows detaching from objects)
 *    - Applied in main lighting shader during shadow sampling
 *
 * Shadow Acne Prevention:
 * - Combination of depth bias and normal offset works best
 * - Depth bias: Pushes shadow map depth away from surface
 * - Normal offset: Moves shadow sample point toward light
 * - Both are tunable parameters in shadow uniform buffer
 *
 * Cascade-Specific Considerations:
 * - Closer cascades need less bias (higher resolution)
 * - Farther cascades need more bias (lower resolution)
 * - Bias should scale with cascade texel size
 * - Dynamic bias calculation based on depth slope is ideal
 */