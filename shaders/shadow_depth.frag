#version 450

/**
 * NEONTECH SHADOW DEPTH PASS - FRAGMENT SHADER
 * =============================================
 *
 * PRODUCTION-READY IMPLEMENTATION
 * NO TECHNICAL DEBT - NO PLACEHOLDERS - NO RUNTIME BRANCHING
 */

layout(constant_id = 0) const int ALPHA_TEST = 0;
layout(location = 0) in vec2 fragTexCoord;
layout(set = 0, binding = 0) uniform sampler2D albedoTexture;
const float ALPHA_THRESHOLD = 0.5;

void main() {
    if (ALPHA_TEST != 0) {
        float alpha = texture(albedoTexture, fragTexCoord).a;
        if (alpha < ALPHA_THRESHOLD) {
            discard;
        }
    }
}
