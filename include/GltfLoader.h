#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

struct Texture {
    std::string name;
    VkImageView imageview;
    VkSampler sampler;
};

struct Material {
    std::string name;
    glm::vec4 baseColorFactor = {1.f, 1.f, 1.f, 1.f};
    float metallicFactor = 1.f;
    float roughnessFactor = 1.f;
    uint32_t baseTextureOffset = UINT32_MAX;
};

struct MeshPrimitive {
    uint32_t indexStart;
    uint32_t vertexStart;
    uint32_t indexCount;
    uint32_t vertexCount;
    Material* material = nullptr;
    bool hasIndices;
};

struct Mesh {
    std::string name;
    std::vector<MeshPrimitive> meshPrimitives;
};

struct RenderObject {
    uint32_t indexStart;
    uint32_t vertexStart;
    uint32_t indexCount;
    uint32_t vertexCount;
    bool hasIndices;

    glm::mat4 transform;
};

struct Node {
    std::string name;
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node> > children;

    std::shared_ptr<Mesh> mesh;

    glm::mat4 localTransform = glm::mat4(1.f);
    glm::mat4 worldTransform = glm::mat4(1.f);
};

struct Scene {
public:
    Scene(VulkanContext *vkCtx);

    std::filesystem::path path;

    VulkanContext *m_vkCtx;
    bool loaded = false;

    std::vector<std::optional<VulkanImage> > images;
    std::vector<VkSampler> samplers;
    std::vector<std::shared_ptr<Texture> > textures;
    std::vector<std::shared_ptr<Material> > materials;
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node> > topLevelNodes;

    GPUMeshBuffers buffers = {};

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    void clear();

    void load(std::filesystem::path filePath);

private:
    void parseImages(const cgltf_data *data);

    void parseMesh(const cgltf_data *data);

    void parseNodes(const cgltf_data *data);

    void parseTextures(const cgltf_data *data);

    void parseMaterials(const cgltf_data *data);
};

static VkFilter extractGltfMagFilter(int gltfMagFilter);

static VkFilter extractGltfMinFilter(int gltfMinFilter);

static VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);
