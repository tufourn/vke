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

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
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
    float pad0;
} pc;

void main()
{
    //load vertex data from device adress
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
    mat4 transform = transforms[pc.transformOffset];

    //output data
    gl_Position = globalUniform.projView * transform * vec4(v.position, 1.0f);

    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
