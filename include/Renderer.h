#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>

struct PushConstantsBindless {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress transformBuffer;
    uint32_t transformOffset;
};

struct DrawData {
    bool hasIndices;
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t transformOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
};

struct GlobalUniformData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 projView;
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

    GlobalUniformData m_globalUniformData;

    void createBuffers();

    void createDrawDatas();

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);
};
