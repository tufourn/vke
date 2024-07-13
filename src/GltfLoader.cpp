#include "GltfLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <iostream>

std::optional<std::shared_ptr<LoadedGLTF> > loadGLTF(std::filesystem::path filePath) {
    std::filesystem::path gltfPath = std::filesystem::current_path() / filePath;
    cgltf_options options = {};
    cgltf_data *data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, gltfPath.string().c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load GLTF file: " << gltfPath <<
                " Errer: " << result << std::endl;
        return nullptr;
    }

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // todo: parse buffers


    cgltf_free(data);
    return nullptr;
}
