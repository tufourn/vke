#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

struct Light {
    vec3 direction;
    float pad;
};

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 proj;
    mat4 projView;
    vec3 cameraPos;
    uint numLights;
    float pad[12];
} globalUniform;

layout(set = 0, binding = 4) readonly buffer lightBuffer {
    Light lights[];
};

layout(set = 0, binding = 5) uniform sampler2D displayTexture[];

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 jointIndices;
    vec4 jointWeights;
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

layout(std430, set = 0, binding = 2) readonly buffer MaterialsBuffer {
    Material materials[];
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
    Material material = materials[pc.materialOffset];

    vec4 baseColor = material.baseColorFactor * texture(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV);
    outFragColor = vec4(0.0);

    for (uint i = 0; i < globalUniform.numLights; i++) {
        Light light = lights[i];

        vec3 norm = normalize(inNormal);
        vec3 lightDir = normalize(light.direction);
        float diff = max(dot(norm, lightDir), 0.0);

        outFragColor += baseColor * diff;
    }
}
