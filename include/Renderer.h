#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>

struct PushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

class Renderer {
public:
    Renderer();

    ~Renderer();

    void run();

    uint32_t currentFrame = 0;

    void loadGltf(std::filesystem::path filePath);

    std::vector<Scene> scenes;

    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

    VkSampler defaultSamplerLinear;

    VkDescriptorSetLayout singleImageDescriptorLayout;

private:
    VulkanContext m_vk;
    Camera m_camera;

    std::vector<uint32_t> m_indexBuffer;
    std::vector<Vertex> m_vertexBuffer;

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);
};
