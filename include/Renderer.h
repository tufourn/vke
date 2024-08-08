#pragma once

#include <Camera.h>
#include <VulkanContext.h>
#include <GltfLoader.h>
#include "Timer.h"
#include "MeshGenerator.h"

constexpr uint32_t LOAD_FAILED = UINT32_MAX;

// todo
struct Stats {
    float frameTime;
};

//todo: separate into ranges
struct PushConstantsBindless {
    VkDeviceAddress vertexBuffer;
    uint32_t transformOffset;
    uint32_t materialOffset;
    uint32_t jointOffset;

    uint32_t modelTransformOffset;
    float pad[3];
};

// holds draw command parameters: offsets for the bounded buffers and instance count
struct DrawData {
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t transformOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t materialOffset;
    uint32_t jointOffset;
    uint32_t modelTransformOffset;
    uint32_t instanceCount;
    bool hasIndices;
};

// holds model buffer offset information and number of DrawData objects
struct ModelData {
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t textureOffset;
    uint32_t materialOffset;
    uint32_t jointOffset;
    uint32_t drawDataOffset;
    uint32_t drawDataCount;
};

// holds information for render objects, currently only the model matrix
struct RenderObjectInfo {
    glm::mat4 modelMatrix;
    uint32_t modelId;
};

struct GlobalUniformData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 projView;
    glm::vec3 cameraPos;
    uint32_t numLights;
    float pad[12];
};

class Renderer {
public:
    Renderer();

    ~Renderer();

    void run();

    uint32_t loadGltf(std::filesystem::path filePath);

    uint32_t loadGeneratedMesh(MeshBuffers *meshBuffer);

    uint32_t addRenderObject(RenderObjectInfo info);

    void addLight(Light light);

private:
    VulkanContext m_vulkanContext;

    uint32_t currentFrame = 0;

    uint32_t getLoadedModelId();
    uint32_t getRenderObjectId();

    uint32_t m_loadedModelCount = 0;
    uint32_t m_renderObjectCount = 0;

    std::vector<std::pair<uint32_t, RenderObjectInfo>> m_renderObjects;

    std::vector<std::pair<std::unique_ptr<GltfScene>, uint32_t>> m_sceneDatas;
    std::vector<std::pair<MeshBuffers *, uint32_t>> m_generatedMeshDatas;

    std::vector<ModelData> m_modelDatas;
    std::vector<DrawData> m_drawDatas;

    std::vector<Light> m_lights;
    VulkanBuffer m_lightBuffer;

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

    std::vector<glm::mat4> m_joints;
    VulkanBuffer m_jointBuffer;

    std::vector<glm::mat4> m_modelTransforms;
    VulkanBuffer m_modelTransformBuffer;

    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedUniformBuffers;
    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedTransformBuffers;
    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedJointBuffers;
    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedLightBuffers;
    std::array<VulkanBuffer, MAX_CONCURRENT_FRAMES> m_boundedModelTransformBuffer;

    Timer m_timer;

    Stats m_stats;

    void setupVulkan();

    void terminateVulkan();

    void drawGeometry(VkCommandBuffer cmd);

    void initDefaultData();

    void createDrawDatas(VkCommandBuffer cmd);

    void updateLightBuffer(VkCommandBuffer cmd);

    void updateLightPos(uint32_t lightIndex);

    VulkanImage opaqueWhiteTextureImage = {};
    VulkanImage opaqueCyanTextureImage = {};
    VulkanImage defaultNormalTextureImage = {};
    VkSampler defaultSampler = {};
};
