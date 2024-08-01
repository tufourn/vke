#pragma once

#define VK_NO_PROTOTYPES
#define GLM_ENABLE_EXPERIMENTAL

#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <memory>

struct VulkanFeatures {
    bool dynamicRendering = true;
    bool synchronization2 = true;
    bool bufferDeviceAddress = true;
    bool descriptorIndexing = true;
    bool runtimeDescriptorArray = true;
    bool shaderSampledImageArrayNonUniformIndexing = true;
    bool shaderStorageBufferArrayNonUniformIndexing = true;
    bool descriptorBindingSampledImageUpdateAfterBind = true;
    bool descriptorBindingPartiallyBound = true;
    bool descriptorBindingVariableDescriptorCount = true;
};

struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo info = {};
};

struct VulkanImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
};

struct Texture {
    std::string name;
    VkImageView imageview;
    VkSampler sampler;
};

struct Material {
    glm::vec4 baseColorFactor = {1.f, 1.f, 1.f, 1.f};
    float metallicFactor = 1.f;
    float roughnessFactor = 1.f;
    uint32_t baseTextureOffset = 0;
    float pad0;
};

struct MeshPrimitive {
    uint32_t indexStart;
    uint32_t vertexStart;
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t materialOffset;
    bool hasIndices;
};

struct Mesh {
    std::string name;
    std::vector<MeshPrimitive> meshPrimitives;
};

struct Node {
    std::string name;
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node> > children;

    std::shared_ptr<Mesh> mesh;

    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;

    glm::mat4 worldTransform = glm::mat4(1.f);
    glm::mat4 getLocalTransform() {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), translation);
        glm::mat4 rotationMatrix = glm::toMat4(rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), scale);

        return translationMatrix * rotationMatrix * scaleMatrix;
    };
};

struct AnimationSampler {
    enum Interpolation {
        eLinear,
        eStep,
        eCubicSpline,
    };

    Interpolation interpolation;
    std::vector<float> inputs;
    std::vector<glm::vec4> outputs;
};

struct AnimationChannel {
    enum Path {
        eTranslation,
        eRotation,
        eScale,
        eWeights,
    };

    Path path;
    uint32_t nodeIndex;
    uint32_t samplerIndex;
};

struct Animation {
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float start = std::numeric_limits<float>::max();
    float end = std::numeric_limits<float>::min();
    float currentTime = 0.f;
};
