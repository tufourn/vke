#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

struct DrawContext;

class IRenderable {
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
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

struct Node : public IRenderable {
    std::string name;
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    std::shared_ptr<Mesh> mesh;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void updateWorldTransform(const glm::mat4& parentMatrix) {
        worldTransform = parentMatrix * localTransform;
        for (auto& child : children) {
            child->updateWorldTransform(worldTransform);
        }
    }

    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override {
        //todo
    }
};

struct DrawContext {
    std::vector<MeshPrimitive> meshPrimitives;
};

struct Scene : public IRenderable {
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node>> topLevelNodes;


    GPUMeshBuffers buffers;

    void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
};

std::optional<Scene> loadGLTF(VulkanContext *vk, std::filesystem::path filePath);

void parseMesh(cgltf_data *data, std::vector<std::shared_ptr<Mesh>> &meshes,
               std::vector<uint32_t> &indexBuffer, std::vector<Vertex> &vertexBuffer);

void parseNodes(cgltf_data *data, Scene& scene);
