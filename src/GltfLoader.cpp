#include "GltfLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

void Scene::draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
}

std::optional<Scene> loadGLTF(VulkanContext *vk, std::filesystem::path filePath) {
    std::filesystem::path gltfPath = std::filesystem::current_path() / filePath;
    cgltf_options options = {};
    cgltf_data *data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, gltfPath.string().c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load GLTF file: " << gltfPath <<
                " Error: " << result << std::endl;
        return {};
    }

    if (cgltf_load_buffers(&options, data, gltfPath.string().c_str()) != cgltf_result_success) {
        std::cerr << "Failed to load buffers from file: " << gltfPath <<
                " Error: " << result << std::endl;
        return {};
    }

    Scene scene;

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    scene.meshes = parseMesh(data, indexBuffer, vertexBuffer);
    scene.buffers = vk->uploadMesh(indexBuffer, vertexBuffer);

    cgltf_free(data);
    return scene;
}

std::vector<std::shared_ptr<Mesh> > parseMesh(cgltf_data *data, std::vector<uint32_t> &indexBuffer,
                                              std::vector<Vertex> &vertexBuffer) {
    std::vector<std::shared_ptr<Mesh> > meshes;

    for (size_t mesh_i = 0; mesh_i < data->meshes_count; mesh_i++) {
        Mesh newMesh = {};

        const cgltf_mesh *gltfMesh = &data->meshes[mesh_i];

        if (gltfMesh->name != nullptr) {
            newMesh.name = gltfMesh->name;
        }

        for (size_t primitive_i = 0; primitive_i < gltfMesh->primitives_count; primitive_i++) {
            const cgltf_primitive *gltfPrimitive = &gltfMesh->primitives[primitive_i];

            uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            bool hasIndices = gltfPrimitive->indices != nullptr;

            for (size_t attr_i = 0; attr_i < gltfPrimitive->attributes_count; attr_i++) {
                const cgltf_attribute *gltfAttribute = &gltfPrimitive->attributes[attr_i];
                const cgltf_accessor *gltfAccessor = gltfAttribute->data;

                if (strcmp(gltfAttribute->name, "POSITION") == 0) {
                    vertexCount = gltfAccessor->count;
                    const cgltf_buffer_view *posBufferView = gltfAccessor->buffer_view;
                    const std::byte *posBuffer = static_cast<const std::byte *>(posBufferView->buffer->data);
                    for (size_t pos_i = 0; pos_i < gltfAccessor->count; pos_i++) {
                        Vertex vertex = {};
                        size_t bufIndex = gltfAccessor->offset + posBufferView->offset +
                                          (vertexStart + pos_i) * gltfAccessor->stride;

                        vertex.position = glm::make_vec3(
                            reinterpret_cast<const float *>(&posBuffer[bufIndex])
                        );

                        vertexBuffer.push_back(vertex);
                    }
                }
            }

            if (hasIndices) {
                const cgltf_accessor *gltfIndexAccessor = gltfPrimitive->indices;
                indexCount = gltfIndexAccessor->count;
                const cgltf_buffer_view *indexBufferView = gltfIndexAccessor->buffer_view;
                const std::byte *indBuffer = static_cast<const std::byte *>(indexBufferView->buffer->data);

                size_t bufOffset = gltfIndexAccessor->offset + indexBufferView->offset +
                                   indexStart * gltfIndexAccessor->stride;

                switch (cgltf_component_size(gltfIndexAccessor->component_type)) {
                    case 4: {
                        const uint32_t *buffer = reinterpret_cast<const uint32_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    case 2: {
                        const uint16_t *buffer = reinterpret_cast<const uint16_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    case 1: {
                        const uint8_t *buffer = reinterpret_cast<const uint8_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    default:
                        std::cerr << newMesh.name << ": invalid primitive index component type" << std::endl;
                        return {};
                }
            }

            newMesh.meshPrimitives.emplace_back(indexStart, vertexStart, indexCount, vertexCount, hasIndices);
        }
        meshes.emplace_back(std::make_shared<Mesh>(std::move(newMesh)));
    }

    return meshes;
}
