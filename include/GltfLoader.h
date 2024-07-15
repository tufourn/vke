#pragma once

#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

struct MeshPrimitive {
    uint32_t startIndex;
    uint32_t indexCount;
    uint32_t vertexCount;
    bool hasIndices;
};

struct Mesh {
    std::string name;
    std::vector<MeshPrimitive> meshPrimitives;
    GPUMeshBuffers meshBuffers;
};

struct Scene : public IRenderable {
    std::vector<Mesh> meshes;

    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

std::vector<std::shared_ptr<Mesh>> loadGLTF(VulkanContext *vk, std::filesystem::path filePath);