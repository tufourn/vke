#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>

struct PushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct DrawData {
    bool hasIndices;
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t transformOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
};

class Renderer {
public:
    Renderer();

    ~Renderer();

    void run();

    void loadGltf(std::filesystem::path filePath);

private:
    uint32_t currentFrame = 0;

    std::vector<Scene> scenes;
    std::vector<DrawData> drawDatas;

    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

    VkSampler defaultSamplerLinear;

    VkDescriptorSetLayout singleImageDescriptorLayout;
    VulkanContext m_vk;
    Camera m_camera;

    std::vector<uint32_t> m_indexBuffer;
    std::vector<Vertex> m_vertexBuffer;
    std::vector<glm::mat4> m_transformBuffer;

    VulkanBuffer m_vulkanVertexBuffer;
    VulkanBuffer m_vulkanIndexBuffer;
    VulkanBuffer m_vulkanTransformBuffer;

    void createBuffers();

    void createDrawDatas();

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);
};
