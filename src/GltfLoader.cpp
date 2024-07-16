#include "GltfLoader.h"

#define CGLTF_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL
#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

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

    parseMesh(data, scene.meshes, indexBuffer, vertexBuffer);
    parseNodes(data, scene);
    scene.buffers = vk->uploadMesh(indexBuffer, vertexBuffer);


    cgltf_free(data);
    return scene;
}

void parseMesh(cgltf_data *data, std::vector<std::shared_ptr<Mesh> > &meshes,
               std::vector<uint32_t> &indexBuffer, std::vector<Vertex> &vertexBuffer) {
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
                        size_t bufOffset = gltfAccessor->offset + posBufferView->offset +
                                          pos_i * gltfAccessor->stride;

                        vertex.position = glm::make_vec3(
                            reinterpret_cast<const float *>(&posBuffer[bufOffset])
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

                size_t bufOffset = gltfIndexAccessor->offset + indexBufferView->offset;

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
                        return;
                }
            }

            newMesh.meshPrimitives.emplace_back(indexStart, vertexStart, indexCount, vertexCount, hasIndices);
        }
        meshes.emplace_back(std::make_shared<Mesh>(std::move(newMesh)));
    }
}

void parseNodes(cgltf_data *data, Scene &scene) {
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

    for (auto& node : scene.nodes) {
        if (node->parent.lock() == nullptr) {
            scene.topLevelNodes.push_back(node);
            node->updateWorldTransform(glm::mat4(1.f));
        }
    }
}

void Node::draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
    if (mesh != nullptr) {
        glm::mat4 nodeMatrix = topMatrix * worldTransform;

        for (auto& meshPrimitive : mesh->meshPrimitives) {
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

    for (auto& child : children) {
        child->draw(topMatrix, ctx);
    }
}

void Scene::draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
    for (auto& node : topLevelNodes) {
        node->draw(topMatrix, ctx);
    }
}

