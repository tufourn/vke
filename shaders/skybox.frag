#version 460

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform samplerCube samplerEnv;

void main() {
    outColor = texture(samplerEnv, inUVW);
//    outColor = vec4(1.f, 1.f, 1.f, 1.f);
}