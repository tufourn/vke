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

layout (set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 model;
} ubo;

layout(push_constant) uniform skyboxConstants {
    VertexBuffer vertexBuffer;
} pc;

void main() {
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];

    outUVW = v.position;
    outUVW.y *= -1.0;
    gl_Position = (ubo.projection * ubo.model * vec4(v.position, 1.0)).xyww;
}