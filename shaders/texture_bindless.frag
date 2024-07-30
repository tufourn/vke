#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;

//output write
layout (location = 0) out vec4 outFragColor;

//texture to access
layout(set = 0, binding = 1) uniform sampler2D displayTexture[];

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
};

struct Material {
    vec4 baseColorFactor;

    float metallicFactor;
    float roughnessFactor;
    uint baseTextureOffset;
    float pad0;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer TransformBuffer {
    mat4 transforms[];
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer {
    Material materials[];
};

//push constants block
layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
    TransformBuffer transformBuffer;
    MaterialBuffer materialBuffer;
    uint transformOffset;
    uint materialOffset;
    float pad0[3];
} pc;

void main()
{
    Material material = pc.materialBuffer.materials[pc.materialOffset];
    outFragColor = material.baseColorFactor * texture(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV);
}
