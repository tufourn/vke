#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

struct Scene : public IRenderable {
    std::vector<std::shared_ptr<Mesh> > meshes;
    std::vector<std::shared_ptr<Node> > nodes;

    GPUMeshBuffers buffers;

    void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
};

std::optional<Scene> loadGLTF(VulkanContext *vk, std::filesystem::path filePath);

std::vector<std::shared_ptr<Mesh> > parseMesh(cgltf_data *data, std::vector<uint32_t> &indexBuffer,
                                              std::vector<Vertex> &vertexBuffer);
