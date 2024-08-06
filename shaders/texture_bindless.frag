#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

struct Light {
    vec3 position;
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
    vec4 tangent;
    vec4 bitangent;
    vec4 jointIndices;
    vec4 jointWeights;
};

struct Material {
    vec4 baseColorFactor;

    float metallicFactor;
    float roughnessFactor;
    uint baseTextureOffset;
    uint metallicRoughnessTextureOffset;

    uint normalTextureOffset;
    uint occlusionTextureOffset;
    uint emissiveTextureOffset;
    float pad;
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

float PI = 3.141592653589;

float D_GGX(float NoH, float roughness) {
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

vec3 F_Schlick(float u, vec3 f0) {
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

void main()
{
    Material material = materials[pc.materialOffset];

    vec4 baseColor = material.baseColorFactor * texture(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV);
    vec4 metallicRoughness = texture(displayTexture[nonuniformEXT(material.metallicRoughnessTextureOffset)], inUV);
    vec3 n = texture(displayTexture[nonuniformEXT(material.normalTextureOffset)], inUV).rgb;

    // green channel for roughness
    float perceivedRoughness = material.roughnessFactor * metallicRoughness.g;
    float roughness = perceivedRoughness * perceivedRoughness;

    // blue channel for metallic
    float metallic = material.metallicFactor * metallicRoughness.b;

    // default reflectance value per filament doc
    float reflectance = 0.04;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + baseColor.rgb * metallic;

    // base color remapping
    vec3 diffuseColor = (1.0 - metallic) * baseColor.rgb;

    // view vector
    vec3 v = normalize(globalUniform.cameraPos - inFragPos);

    // normal vector in world space
    n = normalize(inTBN * (n * 2.0 - 1.0));

    vec3 outColor = vec3(0.0);

    for (uint i = 0; i < globalUniform.numLights; i++) {
        Light light = lights[i];

        // light vector
        vec3 l = normalize(light.position - inFragPos);

        // halfway vector
        vec3 h = normalize(l + v);

        float NoV = abs(dot(n, v)) + 1e-5;
        float NoL = clamp(dot(n, l), 0.0, 1.0);
        float NoH = clamp(dot(n, h), 0.0, 1.0);
        float LoH = clamp(dot(l, h), 0.0, 1.0);

        float D = D_GGX(NoH, roughness);
        vec3 F = F_Schlick(LoH, f0);
        float V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);

        vec3 Fr = (D * V) * F;
        vec3 Fd = diffuseColor;// * Fd_Lambert()

        outColor += (Fd + Fr) * NoL;
    }

    outFragColor = vec4(outColor, baseColor.w);

//    mipmapping tint test
//    float lod = textureQueryLod(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV).x;
//    outFragColor += vec4(0.2, 0.0, 0.0, 0.0) * lod;
}
