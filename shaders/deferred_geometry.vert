#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

// Uniform buffers
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPosition;
} camera;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    mat4 normalMatrix;
} object;

// Outputs to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out mat3 fragTBN;

void main() {
    // Transform position to world space
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // Transform normal to world space (using normal matrix to handle non-uniform scaling)
    fragNormal = normalize(mat3(object.normalMatrix) * inNormal);

    // Pass through texture coordinates
    fragTexCoord = inTexCoord;

    // Calculate TBN matrix for normal mapping
    vec3 T = normalize(mat3(object.normalMatrix) * inTangent);
    vec3 N = fragNormal;
    // Re-orthogonalize T with respect to N (Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    fragTBN = mat3(T, B, N);

    // Transform to clip space
    gl_Position = camera.viewProjection * worldPos;
}