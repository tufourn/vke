#pragma once

#include <memory>
#include <filesystem>
#include <optional>

#include "VulkanTypes.h"

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;

};

struct LoadedGLTF : public IRenderable {
    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(std::filesystem::path filePath);