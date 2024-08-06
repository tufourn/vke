#include "Skybox.h"
#include "Renderer.h"

Skybox::Skybox(Renderer *renderer) : m_renderer(renderer) {

}

Skybox::~Skybox() {
    for (auto& texture : m_textures) {
        if (texture.image != VK_NULL_HANDLE) {
            m_renderer->vulkanContext.destroyImage(texture);
        }
    }
}

void Skybox::load(std::filesystem::path filePath) {
    path = std::filesystem::current_path() / filePath;


}
