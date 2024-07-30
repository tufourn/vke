#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanContext.h"
#include "VulkanTypes.h"

constexpr uint32_t DEFAULT_MATERIAL = UINT32_MAX;
constexpr uint32_t OPAQUE_WHITE_TEXTURE = UINT32_MAX;

class Renderer;

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
    Scene(Renderer *renderer);

    std::filesystem::path path;

    Renderer *renderer;
    bool loaded = false;

    std::vector<std::optional<VulkanImage> > images;
    std::vector<VkSampler> samplers;
    std::vector<std::shared_ptr<Texture> > textures;
    std::vector<Material> materials;
    std::vector<std::string> materialNames;
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node> > topLevelNodes;

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
