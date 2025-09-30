#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in mat3 fragTBN;

// Material properties
layout(set = 2, binding = 0) uniform MaterialUBO {
    vec3 albedo;
    float metallic;
    vec3 emissive;
    float roughness;
    float ao;
    float alpha;
    uint albedoTexture;
    uint normalTexture;
    uint metallicRoughnessTexture;
    uint emissiveTexture;
    uint aoTexture;
    uint padding;
} material;

// Texture samplers (optional - 0 means no texture)
layout(set = 3, binding = 0) uniform sampler2D textures[16];

// G-Buffer outputs
layout(location = 0) out vec4 outAlbedoMetallic;      // RGB: albedo, A: metallic
layout(location = 1) out vec4 outNormalRoughness;     // RGB: world normal, A: roughness
layout(location = 2) out vec4 outPositionAO;          // RGB: world position, A: ambient occlusion
layout(location = 3) out vec4 outEmissive;            // RGB: emissive color

void main() {
    // Sample albedo (with optional texture)
    vec3 albedo = material.albedo;
    if (material.albedoTexture > 0) {
        albedo *= texture(textures[material.albedoTexture - 1], fragTexCoord).rgb;
    }

    // Sample and apply normal map (if present)
    vec3 normal = normalize(fragNormal);
    if (material.normalTexture > 0) {
        // Sample normal map
        vec3 tangentNormal = texture(textures[material.normalTexture - 1], fragTexCoord).xyz * 2.0 - 1.0;
        // Transform from tangent space to world space
        normal = normalize(fragTBN * tangentNormal);
    }

    // Sample metallic and roughness (from combined texture or material properties)
    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.metallicRoughnessTexture > 0) {
        vec2 mr = texture(textures[material.metallicRoughnessTexture - 1], fragTexCoord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    }

    // Sample emissive (with optional texture)
    vec3 emissive = material.emissive;
    if (material.emissiveTexture > 0) {
        emissive *= texture(textures[material.emissiveTexture - 1], fragTexCoord).rgb;
    }

    // Sample ambient occlusion (with optional texture)
    float ao = material.ao;
    if (material.aoTexture > 0) {
        ao *= texture(textures[material.aoTexture - 1], fragTexCoord).r;
    }

    // Write to G-Buffer
    outAlbedoMetallic = vec4(albedo, metallic);
    outNormalRoughness = vec4(normal * 0.5 + 0.5, roughness);  // Encode normal to [0,1] range
    outPositionAO = vec4(fragWorldPos, ao);
    outEmissive = vec4(emissive, 1.0);

    // Alpha testing (discard fully transparent fragments)
    if (material.alpha < 0.01) {
        discard;
    }
}