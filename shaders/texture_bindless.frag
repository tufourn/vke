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

layout(set = 0, binding = 5) readonly buffer lightBuffer {
    Light lights[];
};

layout(set = 0, binding = 6) uniform samplerCube irradianceMap;

layout(set = 0, binding = 7) uniform samplerCube prefilteredCube;

layout(set = 0, binding = 8) uniform sampler2D brdfLUT;

layout(set = 0, binding = 9) uniform sampler2D displayTexture[];

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

    vec3 emissiveFactor;
    float pad1;
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

// Converts a color from linear light gamma to sRGB gamma
vec4 fromLinear(vec4 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);

    return vec4(mix(higher, lower, cutoff), linearRGB.a);
}

// Converts a color from sRGB gamma to linear light gamma
vec4 toLinear(vec4 sRGB)
{
    bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
    vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));
    vec3 lower = sRGB.rgb/vec3(12.92);

    return vec4(mix(higher, lower, cutoff), sRGB.a);
}

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

vec3 fresnelSchlickRoughness(float NoV, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
}

float lodFromRoughness(float roughness) {
    const float MAX_LOD = 10.0; // cube res is 1024
    return roughness * MAX_LOD;
}

void main()
{
    Material material = materials[pc.materialOffset];

    vec4 baseColor = toLinear(material.baseColorFactor * texture(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV));
    vec4 metallicRoughness = texture(displayTexture[nonuniformEXT(material.metallicRoughnessTextureOffset)], inUV);
    vec3 emissive = toLinear(vec4(material.emissiveFactor, 1.0) * texture(displayTexture[nonuniformEXT(material.emissiveTextureOffset)], inUV)).rgb;
    vec3 n = texture(displayTexture[nonuniformEXT(material.normalTextureOffset)], inUV).rgb;

    // green channel for roughness, clamped to 0.089 to avoid division by 0
    float perceivedRoughness = clamp(material.roughnessFactor * metallicRoughness.g, 0.089, 1.0);
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

    float NoV = abs(dot(n, v)) + 1e-5;
    vec3 kS = fresnelSchlickRoughness(NoV, f0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(irradianceMap, n).rgb;
    vec3 diffuse = irradiance * diffuseColor;

    vec3 r = reflect(v, n);
    vec3 prefilteredColor = textureLod(prefilteredCube, r, lodFromRoughness(roughness)).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NoV, roughness)).xy;
    vec3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD * diffuse + specular);

    outColor += ambient;

//    for (uint i = 0; i < globalUniform.numLights; i++) {
//        Light light = lights[i];
//
//        // light vector
//        vec3 l = normalize(light.position - inFragPos);
//
//        // halfway vector
//        vec3 h = normalize(l + v);
//
//        float NoV = abs(dot(n, v)) + 1e-5;
//        float NoL = clamp(dot(n, l), 0.0, 1.0);
//        float NoH = clamp(dot(n, h), 0.0, 1.0);
//        float LoH = clamp(dot(l, h), 0.0, 1.0);
//
//        float D = D_GGX(NoH, roughness);
//        vec3 F = F_Schlick(LoH, f0);
//        float V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);
//
//        vec3 Fr = (D * V) * F / (4 * NoV);
//        vec3 Fd = diffuseColor;// * Fd_Lambert()
//
//        outColor += (Fd * (1 - F) + Fr) * NoL / globalUniform.numLights;
//    }

    outColor += emissive;
    outFragColor = fromLinear(vec4(outColor, baseColor.a));

//    mipmapping tint test
//    float lod = textureQueryLod(displayTexture[nonuniformEXT(material.baseTextureOffset)], inUV).x;
//    outFragColor += vec4(0.2, 0.0, 0.0, 0.0) * lod;
}
