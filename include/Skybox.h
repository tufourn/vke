#pragma once

#include <array>
#include "VulkanTypes.h"
#include "Utils.h"
#include "VulkanDescriptor.h"

class VulkanContext;

static constexpr uint32_t cubeRes = 1024;

struct SkyboxPushBlock {
    glm::mat4 matrix;
    VkDeviceAddress vertexBuffer;
};

static const std::array<glm::mat4, 6> faceMatrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
};

class Skybox {
public:
    MOVABLE_ONLY(Skybox);

    Skybox(VulkanContext *vulkanContext);

    ~Skybox();

    bool load(std::filesystem::path filePath);

    VulkanImage loadedImage;
    VulkanImage cubemap;
    VulkanImage irradianceMap;
    VkSampler sampler;

    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;

    void init();

private:
    VulkanContext *m_vulkanContext;

    DescriptorAllocator cubemapDescriptors;
    VkDescriptorSetLayout cubemapDescriptorLayout;
    VkDescriptorSet cubemapDescriptorSet;
    VkPipelineLayout cubemapPipelineLayout;
    VkPipeline cubemapPipeline;
    VkPipeline irradianceMapPipeline;

    void createOffscreenDrawImage();
    VulkanImage createCubemapImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);

    void createPipelines();
    void createCubemap();
    void createIrradianceMap();

    VulkanImage m_offscreenImage;

    bool m_loaded;
};