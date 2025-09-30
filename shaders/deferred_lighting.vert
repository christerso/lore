#version 450

/**
 * DEFERRED LIGHTING PASS - VERTEX SHADER
 * ========================================
 *
 * PURPOSE:
 *   This shader generates a full-screen quad that covers the entire viewport.
 *   It's used in the lighting pass to process every pixel on the screen.
 *
 * WHY A FULL-SCREEN QUAD?
 *   In deferred rendering, the lighting pass doesn't render 3D geometry.
 *   Instead, it processes the G-Buffer (which already contains all geometry data).
 *   We need to touch every pixel on the screen exactly once, so we render
 *   a simple quad that fills the screen.
 *
 * HOW IT WORKS:
 *   1. Uses gl_VertexIndex to generate 6 vertices (2 triangles = 1 quad)
 *   2. Positions are in NDC (Normalized Device Coordinates): [-1, 1]
 *   3. Texture coordinates [0, 1] are calculated to sample the G-Buffer
 *   4. No transformation matrices needed - already in clip space
 *
 * VERTEX LAYOUT (NO INPUT BUFFERS NEEDED):
 *   gl_VertexIndex 0: bottom-left
 *   gl_VertexIndex 1: bottom-right
 *   gl_VertexIndex 2: top-left
 *   gl_VertexIndex 3: top-left      (second triangle)
 *   gl_VertexIndex 4: bottom-right  (second triangle)
 *   gl_VertexIndex 5: top-right     (second triangle)
 *
 * USAGE:
 *   vkCmdDraw(commandBuffer, 6, 1, 0, 0);  // 6 vertices, 1 instance
 */

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Generate full-screen quad using vertex index
    // This technique avoids needing a vertex buffer

    /**
     * Vertex index to position mapping:
     * 0 → (-1, -1)  bottom-left
     * 1 → ( 3, -1)  far right (off-screen, but creates full coverage)
     * 2 → (-1,  3)  far top (off-screen, but creates full coverage)
     *
     * This creates a triangle that covers the entire screen plus extra.
     * The rasterizer will clip it to the viewport bounds.
     * This is more efficient than rendering 2 triangles with 6 vertices.
     */

    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),  // Bottom-left
        vec2( 3.0, -1.0),  // Far right
        vec2(-1.0,  3.0)   // Far top
    );

    vec2 texCoords[3] = vec2[](
        vec2(0.0, 0.0),    // Bottom-left UV
        vec2(2.0, 0.0),    // Far right UV
        vec2(0.0, 2.0)     // Far top UV
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = texCoords[gl_VertexIndex];
}

/**
 * ALTERNATIVE APPROACH (6 vertices, 2 triangles):
 * If you prefer the traditional approach, you can use:
 *
 * vec2 positions[6] = vec2[](
 *     vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),  // Triangle 1
 *     vec2(-1.0,  1.0), vec2(1.0, -1.0), vec2(1.0,  1.0)   // Triangle 2
 * );
 *
 * Then call: vkCmdDraw(commandBuffer, 6, 1, 0, 0);
 */