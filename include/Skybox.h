#pragma once

#include "VulkanContext.h"
#include "Utils.h"

class Renderer;

class Skybox {
public:
    MOVABLE_ONLY(Skybox);
    Skybox(Renderer* renderer);
    ~Skybox();

    void load(std::filesystem::path filePath);

    std::filesystem::path path;

    bool loaded = false;
private:
    Renderer* m_renderer;
    std::array<VulkanImage, 6> m_textures;
};