#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "Utils.h"
#include "VulkanTypes.h"

class VulkanContext;

struct GltfScene {
public:
    MOVABLE_ONLY(GltfScene);

    GltfScene(VulkanContext *vulkanContext);

    ~GltfScene();

    std::filesystem::path path;

    bool loaded = false;

    std::vector<std::optional<VulkanImage> > images;
    std::vector<VkSampler> samplers;
    std::vector<std::shared_ptr<Texture> > textures;
    std::vector<Material> materials;
    std::vector<std::string> materialNames;
    std::vector<std::shared_ptr<Mesh> > meshes;

    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Node> > topLevelNodes;

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    std::vector<Animation> animations;
    std::vector<std::unique_ptr<Skin>> skins;

    std::vector<uint32_t> skinJointCounts;
    std::vector<uint32_t> jointOffsets;

    void load(std::filesystem::path filePath);

    void updateAnimation(float deltaTime);

private:
    VulkanContext* m_vulkanContext;

    void parseImages(const cgltf_data *data);

    void parseMesh(const cgltf_data *data);

    void parseNodes(const cgltf_data *data);

    void parseTextures(const cgltf_data *data);

    void parseMaterials(const cgltf_data *data);

    void parseAnimations(const cgltf_data *data);

    void parseSkins(const cgltf_data *data);

    void clear();

};

static VkFilter extractGltfMagFilter(int gltfMagFilter);

static VkFilter extractGltfMinFilter(int gltfMinFilter);

static VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);

static VkSamplerMipmapMode extractGltfMipmapMode(int gltfMinFilter);
