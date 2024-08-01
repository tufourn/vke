#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>
#include "Timer.h"

struct Stats {
    float frameTime;
};

//todo: separate into ranges
struct PushConstantsBindless {
    VkDeviceAddress vertexBuffer;
    uint32_t transformOffset;
    uint32_t materialOffset;
    float pad0;
};

struct DrawData {
    bool hasIndices;
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t transformOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t materialOffset;
};

struct SceneData {
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t textureOffset;
    uint32_t materialOffset;
    std::vector<DrawData> drawDatas;
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

    VulkanContext vulkanContext;

    VulkanImage errorTextureImage = {};
    VulkanImage opaqueWhiteTextureImage = {};
    VkSampler defaultSampler = {};

private:
    uint32_t currentFrame = 0;

    std::vector<std::pair<std::unique_ptr<Scene>, SceneData>> m_scenes;

    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

    DescriptorAllocator globalDescriptors = {};
    VkDescriptorSetLayout globalDescriptorLayout = {};
    std::array<VkDescriptorSet, MAX_CONCURRENT_FRAMES> bindlessDescriptorSets;

    Camera m_camera;

    std::vector<uint32_t> m_indices;
    std::vector<Vertex> m_vertices;
    std::vector<std::shared_ptr<Texture>> m_textures;
    std::vector<Material> m_materials;

    VulkanBuffer m_boundedVertexBuffer;
    VulkanBuffer m_boundedIndexBuffer;
    VulkanBuffer m_boundedMaterialBuffer;

    void destroyStaticBuffers();
    void createStaticBuffers();

    GlobalUniformData m_globalUniformData;
    VulkanBuffer m_uniformBuffer;

    std::vector<glm::mat4> m_transforms;
    VulkanBuffer m_transformBuffer;

    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedUniformBuffers;
    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedTransformBuffers;

    Timer m_timer;

    Stats m_stats;

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);

    void initDefaultData();

    void createDrawDatas(VkCommandBuffer cmd);
};
