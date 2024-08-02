#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 proj;
    mat4 projView;
} globalUniform;

layout(std430, set = 0, binding = 1) readonly buffer TransformBuffer {
    mat4 transforms[];
};

layout(std430, set = 0, binding = 3) readonly buffer JointBuffer {
    mat4 joints[];
};

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 jointIndices;
    vec4 jointWeights;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

//push constants block
layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
    uint transformOffset;
    uint materialOffset;
    uint jointOffset;
} pc;

void main()
{
    //load vertex data from device adress
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
    mat4 transform = transforms[pc.transformOffset];

    mat4 skinMatrix =
        v.jointWeights.x * joints[pc.jointOffset + int(v.jointIndices.x)] +
        v.jointWeights.y * joints[pc.jointOffset + int(v.jointIndices.y)] +
        v.jointWeights.z * joints[pc.jointOffset + int(v.jointIndices.z)] +
        v.jointWeights.w * joints[pc.jointOffset + int(v.jointIndices.w)];

    //output data
    gl_Position = globalUniform.projView * transform * skinMatrix * vec4(v.position, 1.0f);

    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
