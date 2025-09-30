#version 450

/**
 * SHADOW DEPTH PASS - VERTEX SHADER
 * ===================================
 *
 * PURPOSE:
 *   Renders scene geometry from the light's perspective to generate shadow maps.
 *   This is a minimal, optimized shader for depth-only rendering.
 *
 * WHAT ARE SHADOW MAPS?
 *   Shadow maps store the depth of the closest surface from the light's viewpoint.
 *   Later, when rendering the scene, we compare each pixel's distance from the light
 *   to the stored depth. If the pixel is farther away, it's in shadow.
 *
 * CASCADED SHADOW MAPS:
 *   We render multiple shadow maps at different distances from the camera:
 *   - Cascade 0: Very close to camera (high detail)
 *   - Cascade 1: Medium distance (medium detail)
 *   - Cascade 2: Far from camera (lower detail)
 *   - Cascade 3: Very far (lowest detail)
 *
 *   This gives us high-quality shadows near the player and acceptable quality far away,
 *   without wasting resolution on distant areas.
 *
 * HOW IT WORKS:
 *   1. Take vertex position in world space
 *   2. Transform to light's clip space using light's view-projection matrix
 *   3. Output depth value (happens automatically via gl_Position)
 *   4. Fragment shader writes depth to shadow map texture
 *
 * OPTIMIZATION NOTES:
 *   - No normal, texcoord, or tangent calculations (depth only!)
 *   - No color calculations (we only care about depth)
 *   - Push constants for fast per-cascade matrix updates
 *   - Early Z-test reduces fragment shader invocations
 */

// ═══════════════════════════════════════════════════════════════
// INPUTS
// ═══════════════════════════════════════════════════════════════

// Vertex attributes - we only need position for shadow mapping
layout(location = 0) in vec3 inPosition;

// Push constants for fast per-cascade updates
// Push constants are faster than uniform buffers for frequently changing data
layout(push_constant) uniform PushConstants {
    mat4 lightViewProj;  // Light's view-projection matrix for current cascade
    mat4 model;          // Model matrix for current object
} pc;

// ═══════════════════════════════════════════════════════════════
// OUTPUTS
// ═══════════════════════════════════════════════════════════════

// Optional: Pass through position for alpha testing in fragment shader
// Only used for vegetation/foliage that needs alpha cutout
layout(location = 0) out vec2 fragTexCoord;

// ═══════════════════════════════════════════════════════════════
// MAIN SHADER
// ═══════════════════════════════════════════════════════════════

void main() {
    // Transform vertex from model space -> world space -> light clip space
    // This single matrix multiplication is all we need for shadow mapping!
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = pc.lightViewProj * worldPos;

    // Note: We don't output texture coordinates here because most objects
    // don't need alpha testing. For objects that do (vegetation), a separate
    // shader variant or dynamic branching can be used.
    // For now, we'll pass through a default value of (0,0) which can be
    // ignored in the fragment shader if alpha testing is disabled.
    fragTexCoord = vec2(0.0);
}

/**
 * TECHNICAL DETAILS:
 * -------------------
 *
 * Depth Buffer Precision:
 * - Vulkan uses reverse-Z by default (1.0 at near, 0.0 at far)
 * - This provides better precision for distant objects
 * - Shadow map format: D32_SFLOAT (32-bit float depth)
 *
 * Cascade Rendering:
 * - This shader is invoked once per cascade per object
 * - Push constants allow us to quickly switch light matrices between cascades
 * - Each cascade renders to a different layer of the shadow map array
 *
 * Culling Optimization:
 * - Front-face culling can be used (render back faces only)
 * - This reduces shadow acne by pushing shadows slightly back
 * - Alternative: depth bias in fragment shader or rasterization state
 *
 * Performance Considerations:
 * - Depth-only rendering is ~5-10x faster than full geometry pass
 * - GPU can use early-Z to skip fragment shader when possible
 * - Minimize overdraw by rendering front-to-back when possible
 */