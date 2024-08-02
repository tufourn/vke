#pragma once

#include <cgltf.h>
#include <memory>
#include <filesystem>
#include <optional>

#include "Utils.h"
#include "VulkanContext.h"
#include "VulkanTypes.h"

constexpr uint32_t DEFAULT_MATERIAL = UINT32_MAX;
constexpr uint32_t OPAQUE_WHITE_TEXTURE = UINT32_MAX;
constexpr uint32_t JOINT_IDENTITY_MATRIX = UINT32_MAX;

class Renderer;

struct Scene {
public:
    MOVABLE_ONLY(Scene);

    Scene(Renderer *renderer);

    ~Scene();

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

    std::vector<Animation> animations;
    std::vector<std::unique_ptr<Skin>> skins;

    std::vector<uint32_t> skinJointCounts;
    std::vector<uint32_t> jointOffsets;

    void clear();

    void load(std::filesystem::path filePath);

    void updateAnimation(float deltaTime);

private:
    void parseImages(const cgltf_data *data);

    void parseMesh(const cgltf_data *data);

    void parseNodes(const cgltf_data *data);

    void parseTextures(const cgltf_data *data);

    void parseMaterials(const cgltf_data *data);

    void parseAnimations(const cgltf_data *data);

    void parseSkins(const cgltf_data *data);
};

static VkFilter extractGltfMagFilter(int gltfMagFilter);

static VkFilter extractGltfMinFilter(int gltfMinFilter);

static VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);
