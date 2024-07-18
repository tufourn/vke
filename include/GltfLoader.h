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
    std::vector<VulkanImage> images;
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node> > topLevelNodes;

    GPUMeshBuffers buffers;

    VulkanContext *vulkanContext;

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;

    void clear();
};

std::optional<Scene> loadGLTF(VulkanContext *vk, std::filesystem::path filePath);

void parseImages(const cgltf_data *data, Scene &scene, std::filesystem::path gltfPath);

void parseMesh(const cgltf_data *data, Scene &scene);

void parseNodes(const cgltf_data *data, Scene &scene);
