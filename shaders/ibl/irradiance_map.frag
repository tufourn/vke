#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform samplerCube envMap;

const float PI = 3.141592653589;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 tangent;
    vec4 bitangent;
    vec4 jointIndices;
    vec4 jointWeights;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform cubemapConstants {
    mat4 matrix;
    VertexBuffer vertexBuffer;
    float sampleDelta;
    uint sampleCount;
    float roughness;
} pc;

void main() {
    vec3 normal = normalize(inUVW);

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = pc.sampleDelta;

    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(envMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));

    outColor = vec4(irradiance, 1.0);
}
