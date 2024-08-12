#version 460
#extension GL_EXT_buffer_reference : require

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

layout (location = 0) out vec3 outUVW;

layout(push_constant) uniform cubemapConstants {
    mat4 matrix;
    VertexBuffer vertexBuffer;
    float sampleDelta;
    uint sampleCount;
    float roughness;
} pc;

void main() {
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];

    outUVW = v.position;
    gl_Position = pc.matrix * vec4(v.position, 1.0);
}
