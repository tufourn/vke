#include "GltfLoader.h"

#define CGLTF_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION
#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <iostream>
#include <Utils.h>

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
    result = cgltf_load_buffers(&options, data, gltfPath.string().c_str());
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load buffers from file: " << gltfPath <<
                " Error: " << result << std::endl;
        return {};
    }

    Scene scene;
    scene.vulkanContext = vk;

    parseImages(data, scene, gltfPath);
    parseMesh(data, scene);
    parseNodes(data, scene);

    cgltf_free(data);
    return scene;
}

void parseImages(const cgltf_data *data, Scene &scene, std::filesystem::path gltfPath) {
    std::filesystem::path directory = gltfPath.parent_path();

    for (size_t image_i = 0; image_i < data->images_count; image_i++) {
        const cgltf_image *gltfImage = &data->images[image_i];
        const char *uri = gltfImage->uri;

        VulkanImage newImage = {};
        int width, height, numChannels;

        if (uri) {
            if (strncmp(uri, "data:", 5) == 0) {
                // embedded image
                const char *comma = strchr(uri, ',');
                if (comma && comma - uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0) {
                    const char *base64 = comma + 1;
                    const size_t base64Size = strlen(base64);
                    size_t decodedBinarySize = base64Size - base64Size / 4;

                    if (base64Size >= 2) {
                        decodedBinarySize -= base64[base64Size - 1] == '=';
                        decodedBinarySize -= base64[base64Size - 2] == '=';
                    }

                    void *imageData = nullptr;
                    cgltf_options options = {};
                    if (cgltf_load_buffer_base64(&options, decodedBinarySize, base64, &imageData) !=
                        cgltf_result_success) {
                        std::cerr << "Failed to parse base64 image uri" << std::endl;
                        return;
                    }

                    unsigned char *stbData = stbi_load_from_memory(
                        static_cast<const unsigned char *>(imageData), decodedBinarySize, &width, &height,
                        &numChannels, 4);

                    VkExtent3D imageExtent;
                    imageExtent.width = width;
                    imageExtent.height = height;
                    imageExtent.depth = 1;

                    newImage = scene.vulkanContext->createImage(stbData, imageExtent, VK_FORMAT_R8G8B8A8_UNORM,
                                                                VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                false);

                    scene.images.push_back(newImage);

                    stbi_image_free(stbData);
                    free(imageData);
                } else {
                    std::cerr << "Invalid embedded image uri" << std::endl;
                    //todo: use default missing texture
                    return;
                }
            } else {
                // todo: load image from file and from buffer
                std::filesystem::path imageFile = directory / uri;
                unsigned char *stbData = stbi_load(imageFile.c_str(), &width, &height, &numChannels, 4);
                if (stbData) {
                    VkExtent3D imageExtent;
                    imageExtent.width = width;
                    imageExtent.height = height;
                    imageExtent.depth = 1;

                    newImage = scene.vulkanContext->createImage(stbData, imageExtent, VK_FORMAT_R8G8B8A8_UNORM,
                                                                VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                false);

                    scene.images.push_back(newImage);

                    stbi_image_free(stbData);
                } else {
                    std::cerr << "Failed to read image file " << imageFile << std::endl;
                }
            }
        }
    }
}

void parseMesh(const cgltf_data *data, Scene &scene) {
    for (size_t mesh_i = 0; mesh_i < data->meshes_count; mesh_i++) {
        Mesh newMesh = {};

        const cgltf_mesh *gltfMesh = &data->meshes[mesh_i];

        if (gltfMesh->name != nullptr) {
            newMesh.name = gltfMesh->name;
        }

        for (size_t primitive_i = 0; primitive_i < gltfMesh->primitives_count; primitive_i++) {
            const cgltf_primitive *gltfPrimitive = &gltfMesh->primitives[primitive_i];

            uint32_t indexStart = static_cast<uint32_t>(scene.indexBuffer.size());
            uint32_t vertexStart = static_cast<uint32_t>(scene.vertexBuffer.size());
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            bool hasIndices = gltfPrimitive->indices != nullptr;

            const cgltf_accessor *positionAccessor = nullptr;
            const cgltf_buffer_view *positionBufferView = nullptr;
            const std::byte *positionBuffer = nullptr;

            const cgltf_accessor *normalAccessor = nullptr;
            const cgltf_buffer_view *normalBufferView = nullptr;
            const std::byte *normalBuffer = nullptr;

            const cgltf_accessor *uvAccessor = nullptr;
            const cgltf_buffer_view *uvBufferView = nullptr;
            const std::byte *uvBuffer = nullptr;

            for (size_t attr_i = 0; attr_i < gltfPrimitive->attributes_count; attr_i++) {
                const cgltf_attribute *gltfAttribute = &gltfPrimitive->attributes[attr_i];
                switch (gltfAttribute->type) {
                    case cgltf_attribute_type_position: {
                        positionAccessor = gltfAttribute->data;
                        positionBufferView = positionAccessor->buffer_view;
                        vertexCount = positionAccessor->count;
                        positionBuffer = static_cast<const std::byte *>(positionBufferView->buffer->data);
                        break;
                    }
                    case cgltf_attribute_type_normal: {
                        normalAccessor = gltfAttribute->data;
                        normalBufferView = normalAccessor->buffer_view;
                        normalBuffer = static_cast<const std::byte *>(normalBufferView->buffer->data);
                        break;
                    }
                    case cgltf_attribute_type_texcoord: {
                        uvAccessor = gltfAttribute->data;
                        uvBufferView = uvAccessor->buffer_view;
                        uvBuffer = static_cast<const std::byte *>(uvBufferView->buffer->data);
                        break;
                    }
                    default:
                        break;
                }
            }

            assert(positionBuffer &&
                (!normalBuffer || normalAccessor->count == positionAccessor->count) &&
                (!uvBuffer || uvAccessor->count == positionAccessor->count)
            );

            for (size_t vertex_i = 0; vertex_i < vertexCount; vertex_i++) {
                Vertex vertex = {};

                // todo: support types other than floats for textures?
                if (positionAccessor && positionBufferView) {
                    size_t positionOffset = positionAccessor->offset + positionBufferView->offset +
                                            vertex_i * positionAccessor->stride;
                    vertex.position = glm::make_vec3(reinterpret_cast<const float *>(&positionBuffer[positionOffset]));
                } else {
                    std::cerr << "Mesh primitive has no vertices" << std::endl;
                }

                if (normalAccessor && normalBufferView) {
                    size_t normalOffset = normalAccessor->offset + normalBufferView->offset +
                                          vertex_i * normalAccessor->stride;
                    vertex.normal = glm::make_vec3(reinterpret_cast<const float *>(&normalBuffer[normalOffset]));
                }

                if (uvAccessor && uvBufferView) {
                    size_t uvOffset = uvAccessor->offset + uvBufferView->offset +
                                      vertex_i * uvAccessor->stride;
                    glm::vec2 uv = glm::make_vec2(reinterpret_cast<const float *>(&uvBuffer[uvOffset]));
                    vertex.uv_x = uv.x;
                    vertex.uv_y = uv.y;
                }

                scene.vertexBuffer.push_back(vertex);
            }

            if (hasIndices) {
                const cgltf_accessor *gltfIndexAccessor = gltfPrimitive->indices;
                indexCount = gltfIndexAccessor->count;
                const cgltf_buffer_view *indexBufferView = gltfIndexAccessor->buffer_view;
                const std::byte *indBuffer = static_cast<const std::byte *>(indexBufferView->buffer->data);

                size_t bufOffset = gltfIndexAccessor->offset + indexBufferView->offset;

                switch (cgltf_component_size(gltfIndexAccessor->component_type)) {
                    case 4: {
                        const uint32_t *buffer = reinterpret_cast<const uint32_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            scene.indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    case 2: {
                        const uint16_t *buffer = reinterpret_cast<const uint16_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            scene.indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    case 1: {
                        const uint8_t *buffer = reinterpret_cast<const uint8_t *>(&indBuffer[bufOffset]);
                        for (size_t i = 0; i < indexCount; i++) {
                            scene.indexBuffer.push_back(buffer[i] + vertexStart);
                        }
                        break;
                    }
                    default:
                        std::cerr << newMesh.name << ": invalid primitive index component type" << std::endl;
                        return;
                }
            }

            newMesh.meshPrimitives.emplace_back(indexStart, vertexStart, indexCount, vertexCount, hasIndices);
        }
        scene.meshes.emplace_back(std::make_shared<Mesh>(std::move(newMesh)));
    }
    scene.buffers = scene.vulkanContext->uploadMesh(scene.indexBuffer, scene.vertexBuffer);
}

void parseNodes(const cgltf_data *data, Scene &scene) {
    for (size_t node_i = 0; node_i < data->nodes_count; node_i++) {
        cgltf_node gltfNode = data->nodes[node_i];
        std::shared_ptr<Node> newNode = std::make_shared<Node>();

        if (gltfNode.name != nullptr) {
            newNode->name = gltfNode.name;
        }

        if (gltfNode.mesh != nullptr) {
            newNode->mesh = scene.meshes[gltfNode.mesh - data->meshes];
        }

        glm::mat4 translation = glm::translate(glm::mat4(1.f), {
                                                   gltfNode.translation[0],
                                                   gltfNode.translation[1],
                                                   gltfNode.translation[2]
                                               });

        glm::quat rotation = glm::quat(gltfNode.rotation[3],
                                       gltfNode.rotation[0],
                                       gltfNode.rotation[1],
                                       gltfNode.rotation[2]);

        glm::mat4 scale = glm::scale(glm::mat4(1.f), {
                                         gltfNode.scale[0],
                                         gltfNode.scale[1],
                                         gltfNode.scale[2]
                                     });

        newNode->localTransform = translation * glm::toMat4(rotation) * scale;

        scene.nodes.emplace_back(newNode);
    }

    for (size_t parentNode_i = 0; parentNode_i < data->nodes_count; parentNode_i++) {
        cgltf_node gltfParentNode = data->nodes[parentNode_i];

        for (size_t child_i = 0; child_i < gltfParentNode.children_count; child_i++) {
            size_t childNode_i = gltfParentNode.children[child_i] - data->nodes;
            scene.nodes[parentNode_i]->children.push_back(scene.nodes[childNode_i]);
            scene.nodes[childNode_i]->parent = scene.nodes[parentNode_i];
        }
    }

    for (auto &node: scene.nodes) {
        if (node->parent.lock() == nullptr) {
            scene.topLevelNodes.push_back(node);
            node->updateWorldTransform(glm::mat4(1.f));
        }
    }
}

void Node::draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
    if (mesh != nullptr) {
        glm::mat4 nodeMatrix = topMatrix * worldTransform;

        for (auto &meshPrimitive: mesh->meshPrimitives) {
            RenderObject renderObject = {};
            renderObject.indexStart = meshPrimitive.indexStart;
            renderObject.vertexStart = meshPrimitive.vertexStart;
            renderObject.indexCount = meshPrimitive.indexCount;
            renderObject.vertexCount = meshPrimitive.vertexCount;
            renderObject.hasIndices = meshPrimitive.hasIndices;

            renderObject.transform = nodeMatrix;

            ctx.renderObjects.push_back(renderObject);
        }
    }

    for (auto &child: children) {
        child->draw(topMatrix, ctx);
    }
}

void Scene::draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
    for (auto &node: topLevelNodes) {
        node->draw(topMatrix, ctx);
    }
}

void Scene::clear() {
    vulkanContext->freeMesh(buffers);

    for (auto &image: images) {
        vulkanContext->destroyImage(image);
    }
}
