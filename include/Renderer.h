#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>

struct Stats {
    float frameTime;
};

//todo: separate into ranges
struct PushConstantsBindless {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress transformBuffer;
    uint32_t transformOffset;
    uint32_t textureOffset;
};

struct DrawData {
    bool hasIndices;
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t transformOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t textureOffset;
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

    DescriptorAllocator globalDescriptors = {};
    VkDescriptorSetLayout globalDescriptorLayout = {};
    std::array<VkDescriptorSet, MAX_CONCURRENT_FRAMES> bindlessDescriptorSets;

    VulkanContext m_vk;
    Camera m_camera;

    std::vector<uint32_t> m_indexBuffer;
    std::vector<Vertex> m_vertexBuffer;
    std::vector<glm::mat4> m_transformBuffer;

    VulkanBuffer m_boundedVertexBuffer;
    VulkanBuffer m_boundedIndexBuffer;
    VulkanBuffer m_boundedTransformBuffer;

    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_uniformBuffers;
    GlobalUniformData m_globalUniformData;
    VulkanBuffer m_boundedUniformBuffer;

    std::vector<std::shared_ptr<Texture>> m_textures;

    Stats m_stats;

    void createBuffers();

    void createDrawDatas();

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);
};
