#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

struct DrawContext;
struct Scene;

class IRenderable {
    virtual void draw(const glm::mat4 &topMatrix, DrawContext &ctx) = 0;
};

struct Texture {
    std::string name;
    VulkanImage image;
    VkSampler sampler;
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline *pipeline;
    VkDescriptorSet materialSet;
};

struct GLTFMetallicRoughness {
    struct materialData {
        glm::vec4 baseColorFactor = {1.f, 1.f, 1.f, 1.f};
        float metallicFactor = 1.f;
        float roughnessFactor = 1.f;
    };
};

struct MeshPrimitive {
    uint32_t indexStart;
    uint32_t vertexStart;
    uint32_t indexCount;
    uint32_t vertexCount;
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

struct DrawContext {
    std::vector<RenderObject> renderObjects;

    VkBuffer indexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct Node : public IRenderable {
    std::string name;
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node> > children;

    std::shared_ptr<Mesh> mesh;

    glm::mat4 localTransform = glm::mat4(1.f);
    glm::mat4 worldTransform = glm::mat4(1.f);

    void updateWorldTransform(const glm::mat4 &parentMatrix) {
        worldTransform = parentMatrix * localTransform;
        for (auto &child: children) {
            child->updateWorldTransform(worldTransform);
        }
    }

    void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
};

struct Scene : public IRenderable {
public:
    Scene(VulkanContext *vkCtx);

    std::filesystem::path path;

    VulkanContext *m_vkCtx;
    bool loaded = false;

    std::vector<std::optional<VulkanImage> > images;
    std::vector<std::shared_ptr<Texture> > textures;
    std::vector<std::shared_ptr<MaterialInstance> > materials;
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node> > topLevelNodes;

    GPUMeshBuffers buffers = {};

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;

    void clear();

    void load(std::filesystem::path filePath);

private:
    void parseImages(const cgltf_data *data);
    void parseMesh(const cgltf_data *data);
    void parseNodes(const cgltf_data* data);
};

void parseTextures(const cgltf_data *data, Scene &scene);

void parseMaterials(const cgltf_data *data, Scene &scene);

static VkFilter extractGltfMagFilter(int gltfMagFilter);

static VkFilter extractGltfMinFilter(int gltfMinFilter);

static VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);
