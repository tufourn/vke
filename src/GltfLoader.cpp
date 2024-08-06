#include "GltfLoader.h"
#include "Renderer.h"

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
#include <numeric>

#include "CalcTangents.h"

void Scene::parseTextures(const cgltf_data *data) {
    for (auto sampler_i = 0; sampler_i < data->samplers_count; sampler_i++) {
        const cgltf_sampler *gltfSampler = &data->samplers[sampler_i];
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = extractGltfMinFilter(gltfSampler->min_filter);
        samplerInfo.magFilter = extractGltfMagFilter(gltfSampler->mag_filter);
        samplerInfo.addressModeU = extractGltfWrapMode(gltfSampler->wrap_s);
        samplerInfo.addressModeV = extractGltfWrapMode(gltfSampler->wrap_t);
        samplerInfo.mipmapMode = extractGltfMipmapMode(gltfSampler->min_filter);
        samplerInfo.maxLod = 16;

        VkSampler sampler;
        vkCreateSampler(renderer->vulkanContext.device, &samplerInfo, nullptr, &sampler);

        samplers.emplace_back(sampler);
    }

    for (size_t texture_i = 0; texture_i < data->textures_count; texture_i++) {
        const cgltf_texture *gltfTexture = &data->textures[texture_i];
        Texture texture = {};
        if (gltfTexture->name) {
            texture.name = gltfTexture->name;
        }

        size_t imageIndex = gltfTexture->image - data->images;
        if (!images.empty() && images[imageIndex].has_value()) {
            texture.imageview = images[imageIndex].value().imageView;
        } else {
            // if image can't be loaded, use the error checkerboard texture
            texture.imageview = renderer->errorTextureImage.imageView;
        }

        if (gltfTexture->sampler && data->samplers) {
            size_t samplerIndex = gltfTexture->sampler - data->samplers;
            texture.sampler = samplers[samplerIndex];
        } else {
            texture.sampler = renderer->defaultSampler;
        }

        textures.emplace_back(std::make_shared<Texture>(texture));
    }
}

void Scene::parseMaterials(const cgltf_data *data) {
    for (size_t material_i = 0; material_i < data->materials_count; material_i++) {
        const cgltf_material *gltfMaterial = &data->materials[material_i];
        Material material = {};

        if (gltfMaterial->name) {
            materialNames.emplace_back(gltfMaterial->name);
        } else {
            materialNames.emplace_back();
        }

        if (gltfMaterial->has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness *gltfMetallicRoughness = &gltfMaterial->pbr_metallic_roughness;

            material.baseColorFactor = glm::make_vec4(gltfMetallicRoughness->base_color_factor);
            material.metallicFactor = gltfMetallicRoughness->metallic_factor;
            material.roughnessFactor = gltfMetallicRoughness->roughness_factor;
            material.emissiveFactor = glm::make_vec3(gltfMaterial->emissive_factor);

            const cgltf_texture_view *baseColorTextureView = &gltfMaterial->pbr_metallic_roughness.base_color_texture;
            if (baseColorTextureView && baseColorTextureView->texture) {
                material.baseTextureOffset = baseColorTextureView->texture - data->textures;
            } else {
                material.baseTextureOffset = NO_TEXTURE_INDEX;
            }

            const cgltf_texture_view *metallicRoughnessTextureView = &gltfMaterial->pbr_metallic_roughness.metallic_roughness_texture;
            if (metallicRoughnessTextureView && metallicRoughnessTextureView->texture) {
                material.metallicRoughnessTextureOffset = metallicRoughnessTextureView->texture - data->textures;
            } else {
                material.metallicRoughnessTextureOffset = NO_TEXTURE_INDEX;
            }
        }

        const cgltf_texture_view *normalTextureView = &gltfMaterial->normal_texture;
        if (normalTextureView && normalTextureView->texture) {
            material.normalTextureOffset = normalTextureView->texture - data->textures;
        } else {
            material.normalTextureOffset = NO_TEXTURE_INDEX;
        }

        const cgltf_texture_view *occlusionTextureView = &gltfMaterial->occlusion_texture;
        if (occlusionTextureView && occlusionTextureView->texture) {
            material.occlusionTextureOffset = occlusionTextureView->texture - data->textures;
        } else {
            material.occlusionTextureOffset = NO_TEXTURE_INDEX;
        }

        const cgltf_texture_view *emissiveTextureView = &gltfMaterial->emissive_texture;
        if (emissiveTextureView && emissiveTextureView->texture) {
            material.emissiveTextureOffset = emissiveTextureView->texture - data->textures;
        } else {
            material.emissiveTextureOffset = NO_TEXTURE_INDEX;
        }

        materials.emplace_back(material);
    }
}

VkFilter extractGltfMagFilter(int gltfMagFilter) {
    switch (gltfMagFilter) {
        case 9728:
            return VK_FILTER_NEAREST;
        case 9729:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
    }
}

VkFilter extractGltfMinFilter(int gltfMinFilter) {
    switch (gltfMinFilter) {
        case 9728:
            return VK_FILTER_NEAREST;
        case 9729:
            return VK_FILTER_LINEAR;
        case 9984:
            return VK_FILTER_NEAREST;
        case 9985:
            return VK_FILTER_NEAREST;
        case 9986:
            return VK_FILTER_LINEAR;
        case 9987:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode extractGltfWrapMode(int gltfWrap) {
    switch (gltfWrap) {
        case 33071:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 33648:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case 10497:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkSamplerMipmapMode extractGltfMipmapMode(int gltfMinFilter) {
    switch (gltfMinFilter) {
        case 9984:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case 9985:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case 9986:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case 9987:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

Scene::Scene(Renderer *renderer) : renderer(renderer) {
}


void Scene::clear() {
    for (auto &image: images) {
        if (image.has_value()) {
            renderer->vulkanContext.destroyImage(image.value());
        }
    }

    for (auto &sampler: samplers) {
        vkDestroySampler(renderer->vulkanContext.device, sampler, nullptr);
    }
}

void Scene::load(std::filesystem::path filePath) {
    path = std::filesystem::current_path() / filePath;

    cgltf_options options = {};
    cgltf_data *data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load GLTF file: " << path <<
                  " Error: " << result << std::endl;
        return;
    }
    result = cgltf_load_buffers(&options, data, path.string().c_str());
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load buffers from file: " << path <<
                  " Error: " << result << std::endl;
        return;
    }

    parseImages(data);
    parseTextures(data);
    parseMaterials(data);
    parseMesh(data);
    parseNodes(data);
    parseAnimations(data);
    parseSkins(data);

    cgltf_free(data);
    loaded = true;
}

// todo: right now it's easiest to have scenes manage their own allocated images, but it might be a better idea to move this stuff inside the renderer class
void Scene::parseImages(const cgltf_data *data) {
    std::filesystem::path directory = path.parent_path();

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

                    newImage = renderer->vulkanContext.createImage(stbData, imageExtent, VK_FORMAT_R8G8B8A8_UNORM,
                                                                   VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                   true);

                    images.emplace_back(newImage);

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
                unsigned char *stbData = stbi_load(imageFile.string().c_str(), &width, &height, &numChannels, 4);
                if (stbData) {
                    VkExtent3D imageExtent;
                    imageExtent.width = width;
                    imageExtent.height = height;
                    imageExtent.depth = 1;

                    newImage = renderer->vulkanContext.createImage(stbData, imageExtent, VK_FORMAT_R8G8B8A8_UNORM,
                                                                   VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                   true);

                    images.emplace_back(newImage);

                    stbi_image_free(stbData);
                } else {
                    std::cerr << "Failed to read image file " << imageFile << std::endl;
                    images.emplace_back();
                }
            }
        } else {
            //todo: read image from buffer
        }
    }
}

void Scene::parseMesh(const cgltf_data *data) {
    for (size_t mesh_i = 0; mesh_i < data->meshes_count; mesh_i++) {
        Mesh newMesh = {};

        const cgltf_mesh *gltfMesh = &data->meshes[mesh_i];

        if (gltfMesh->name != nullptr) {
            newMesh.name = gltfMesh->name;
        }

        for (size_t primitive_i = 0; primitive_i < gltfMesh->primitives_count; primitive_i++) {
            const cgltf_primitive *gltfPrimitive = &gltfMesh->primitives[primitive_i];
            MeshPrimitive newPrimitive = {};

            uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            newPrimitive.indexStart = indexStart;
            newPrimitive.vertexStart = vertexStart;

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

            const cgltf_accessor *tangentAccessor = nullptr;
            const cgltf_buffer_view *tangentBufferView = nullptr;
            const std::byte *tangentBuffer = nullptr;

            const cgltf_accessor *jointAccessor = nullptr;
            const cgltf_buffer_view *jointBufferView = nullptr;
            const std::byte *jointBuffer = nullptr;

            const cgltf_accessor *weightAccessor = nullptr;
            const cgltf_buffer_view *weightBufferView = nullptr;
            const std::byte *weightBuffer = nullptr;

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
                    case cgltf_attribute_type_joints: {
                        jointAccessor = gltfAttribute->data;
                        jointBufferView = jointAccessor->buffer_view;
                        jointBuffer = static_cast<const std::byte *>(jointBufferView->buffer->data);
                        break;
                    }
                    case cgltf_attribute_type_weights: {
                        weightAccessor = gltfAttribute->data;
                        weightBufferView = weightAccessor->buffer_view;
                        weightBuffer = static_cast<const std::byte *>(weightBufferView->buffer->data);
                        break;
                    }
                    case cgltf_attribute_type_tangent: {
                        tangentAccessor = gltfAttribute->data;
                        tangentBufferView = tangentAccessor->buffer_view;
                        tangentBuffer = static_cast<const std::byte *>(tangentBufferView->buffer->data);
                        break;
                    }
                    default:
                        break;
                }
            }

            assert(positionBuffer &&
                           (!normalBuffer || normalAccessor->count == positionAccessor->count) &&
                           (!uvBuffer || uvAccessor->count == positionAccessor->count) &&
                           (!tangentBuffer || tangentAccessor->count == positionAccessor->count) &&
                           (!jointBuffer || jointAccessor->count == positionAccessor->count) &&
                           (!weightBuffer || weightAccessor->count == positionAccessor->count)
            );

            newPrimitive.hasSkin = jointBuffer && weightBuffer;

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

                // todo: handle vertices with no specified normals
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

                if (tangentAccessor && tangentBufferView) {
                    size_t tangentOffset = tangentAccessor->offset + tangentBufferView->offset +
                                           vertex_i * tangentAccessor->stride;
                    vertex.tangent = glm::make_vec4(reinterpret_cast<const float *>(&tangentBuffer[tangentOffset]));
                    vertex.bitangent =
                            glm::vec4(glm::cross(vertex.normal,
                                                 glm::vec3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z)),
                                      vertex.tangent.w);
                } else if (!uvAccessor && normalAccessor) {
                    vertex.tangent =
                            glm::vec4(glm::cross(vertex.normal, glm::vec3(1.f, 1.f, 1.f)), 1.f);
                    vertex.bitangent =
                            glm::vec4(
                                    glm::cross(vertex.normal,
                                               glm::vec3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z)) *
                                    vertex.tangent.w,
                                    1.f);
                }

                //todo joints and weights
                if (newPrimitive.hasSkin) {
                    size_t jointOffset =
                            jointAccessor->offset + jointBufferView->offset + vertex_i * jointAccessor->stride;
                    switch (cgltf_component_size(jointAccessor->component_type)) {
                        case 2: {
                            const uint16_t *joints = reinterpret_cast<const uint16_t *>(&jointBuffer[jointOffset]);
                            vertex.jointIndices = glm::make_vec4(joints);
                            break;
                        }
                        case 1: {
                            const uint8_t *joints = reinterpret_cast<const uint8_t *>(&jointBuffer[jointOffset]);
                            vertex.jointIndices = glm::make_vec4(joints);
                            break;
                        }
                        default: {
                            std::cout << "unsupported joint index type" << std::endl;
                            break;
                        }
                    }

                    size_t weightOffset =
                            weightAccessor->offset + weightBufferView->offset + vertex_i * weightAccessor->stride;
                    switch (cgltf_component_size(weightAccessor->component_type)) {
                        case 4: {
                            const float *weights = reinterpret_cast<const float *>(&weightBuffer[weightOffset]);
                            vertex.jointWeights = glm::make_vec4(weights);
                            break;
                        }
                        case 2: {
                            const uint16_t *weights = reinterpret_cast<const uint16_t *>(&weightBuffer[weightOffset]);
                            vertex.jointWeights = glm::make_vec4(weights);
                            vertex.jointWeights /= UINT16_MAX;
                            break;
                        }
                        case 1: {
                            const uint8_t *weights = reinterpret_cast<const uint8_t *>(&weightBuffer[weightOffset]);
                            vertex.jointWeights = glm::make_vec4(weights);
                            vertex.jointWeights /= UINT8_MAX;
                            break;
                        }
                        default: {
                            std::cout << "unsupported weight type" << std::endl;
                            break;
                        }
                    }

                } else {
                    vertex.jointIndices = glm::vec4(0); // use identity matrix joint
                    vertex.jointWeights = glm::vec4(1.f, 0.f, 0.f, 0.f); // weight 1.f identity matrix
                }

                vertexBuffer.push_back(vertex);
            }

            if (hasIndices) {
                newPrimitive.hasIndices = true;

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

            if (gltfPrimitive->material) {
                newPrimitive.materialOffset = gltfPrimitive->material - data->materials;
            } else {
                newPrimitive.materialOffset = DEFAULT_MATERIAL;
            }

            newPrimitive.indexCount = indexCount;
            newPrimitive.vertexCount = vertexCount;

            if (!tangentAccessor && uvAccessor && normalAccessor) {
                CalcTangents mikktspace;
                CalcTangentData tangentData = {indexBuffer, vertexBuffer, newPrimitive};
                mikktspace.calculate(&tangentData);

                for (size_t vertex_i = newPrimitive.vertexStart; vertex_i < vertexBuffer.size(); vertex_i++) {
                    vertexBuffer[vertex_i].bitangent =
                            glm::vec4(glm::cross(vertexBuffer[vertex_i].normal,
                                                 glm::vec3(vertexBuffer[vertex_i].tangent.x,
                                                           vertexBuffer[vertex_i].tangent.y,
                                                           vertexBuffer[vertex_i].tangent.z)),
                                      vertexBuffer[vertex_i].tangent.w);
                }
            }

            newMesh.meshPrimitives.emplace_back(newPrimitive);
        }
        meshes.emplace_back(std::make_shared<Mesh>(std::move(newMesh)));
    }
}

void Scene::parseNodes(const cgltf_data *data) {
    for (size_t node_i = 0; node_i < data->nodes_count; node_i++) {
        cgltf_node gltfNode = data->nodes[node_i];
        std::shared_ptr<Node> newNode = std::make_shared<Node>();

        if (gltfNode.name != nullptr) {
            newNode->name = gltfNode.name;
        }

        if (gltfNode.mesh != nullptr) {
            newNode->mesh = meshes[gltfNode.mesh - data->meshes];
        }

        newNode->translation = {
                gltfNode.translation[0],
                gltfNode.translation[1],
                gltfNode.translation[2]
        };

        newNode->rotation = glm::quat(gltfNode.rotation[3],
                                      gltfNode.rotation[0],
                                      gltfNode.rotation[1],
                                      gltfNode.rotation[2]);

        newNode->scale = {
                gltfNode.scale[0],
                gltfNode.scale[1],
                gltfNode.scale[2]
        };

        newNode->matrix = glm::make_mat4(gltfNode.matrix);

        newNode->hasSkin = false;
        if (gltfNode.skin != nullptr) {
            newNode->skin = gltfNode.skin - data->skins;
            newNode->hasSkin = true;
        }

        nodes.emplace_back(newNode);
    }

    for (size_t parentNode_i = 0; parentNode_i < data->nodes_count; parentNode_i++) {
        cgltf_node gltfParentNode = data->nodes[parentNode_i];

        for (size_t child_i = 0; child_i < gltfParentNode.children_count; child_i++) {
            size_t childNode_i = gltfParentNode.children[child_i] - data->nodes;
            nodes[parentNode_i]->children.push_back(nodes[childNode_i]);
            nodes[childNode_i]->parent = nodes[parentNode_i];
        }
    }

    for (auto &node: nodes) {
        if (node->parent.lock() == nullptr) {
            topLevelNodes.push_back(node);
        }
    }
}

void Scene::parseAnimations(const cgltf_data *data) {
    for (size_t animation_i = 0; animation_i < data->animations_count; animation_i++) {
        const cgltf_animation *gltfAnimation = &data->animations[animation_i];
        Animation animation = {};

        if (gltfAnimation->name) {
            animation.name = gltfAnimation->name;
        }

        for (size_t sampler_i = 0; sampler_i < gltfAnimation->samplers_count; sampler_i++) {
            const cgltf_animation_sampler *gltfSampler = &gltfAnimation->samplers[sampler_i];
            AnimationSampler sampler = {};

            switch (gltfSampler->interpolation) {
                case cgltf_interpolation_type_linear:
                    sampler.interpolation = AnimationSampler::Interpolation::eLinear;
                    break;
                case cgltf_interpolation_type_step:
                    sampler.interpolation = AnimationSampler::Interpolation::eStep;
                    break;
                case cgltf_interpolation_type_cubic_spline:
                    sampler.interpolation = AnimationSampler::Interpolation::eCubicSpline;
                    break;
                default:
                    sampler.interpolation = AnimationSampler::Interpolation::eLinear;
                    break;
            }

            const cgltf_accessor *inputAccessor = gltfSampler->input;
            const cgltf_buffer_view *inputBufferView = inputAccessor->buffer_view;
            const auto *inputBuffer = static_cast<const std::byte *>(inputBufferView->buffer->data);
            const auto *input = reinterpret_cast<const float *>(&inputBuffer[inputAccessor->offset +
                                                                             inputBufferView->offset]);

            for (size_t input_i = 0; input_i < inputAccessor->count; input_i++) {
                sampler.inputs.emplace_back(input[input_i]);
            }

            const cgltf_accessor *outputAccessor = gltfSampler->output;
            const cgltf_buffer_view *outputBufferView = outputAccessor->buffer_view;
            const auto *outputBuffer = static_cast<const std::byte *>(outputBufferView->buffer->data);

            switch (outputAccessor->type) {
                case cgltf_type_vec3: {
                    const auto *output = reinterpret_cast<const glm::vec3 *>(&outputBuffer[outputAccessor->offset +
                                                                                           outputBufferView->offset]);
                    for (size_t output_i = 0; output_i < outputAccessor->count; output_i++) {
                        sampler.outputs.emplace_back(output[output_i], 0.f);
                    }
                    break;
                }
                case cgltf_type_vec4: {
                    const auto *output = reinterpret_cast<const glm::vec4 *>(&outputBuffer[outputAccessor->offset +
                                                                                           outputBufferView->offset]);
                    for (size_t output_i = 0; output_i < outputAccessor->count; output_i++) {
                        sampler.outputs.emplace_back(output[output_i]);
                    }
                    break;
                }
                default: {
                    std::cout << "Invalid animation sampler output type" << std::endl;
                    break;
                }
            }

            for (const auto &timestamp: sampler.inputs) {
                if (timestamp < animation.start) {
                    animation.start = timestamp;
                }
                if (timestamp > animation.end) {
                    animation.end = timestamp;
                }
            }

            animation.samplers.emplace_back(sampler);
        }

        for (size_t channel_i = 0; channel_i < gltfAnimation->channels_count; channel_i++) {
            const cgltf_animation_channel *gltfChannel = &gltfAnimation->channels[channel_i];
            AnimationChannel channel = {};

            switch (gltfChannel->target_path) {
                case cgltf_animation_path_type_rotation:
                    channel.path = AnimationChannel::Path::eRotation;
                    break;
                case cgltf_animation_path_type_scale:
                    channel.path = AnimationChannel::Path::eScale;
                    break;
                case cgltf_animation_path_type_translation:
                    channel.path = AnimationChannel::Path::eTranslation;
                    break;
                case cgltf_animation_path_type_weights:
                    channel.path = AnimationChannel::Path::eWeights;
                    break;
                default:
                    std::cout << "Invalid animation channel path" << std::endl;
                    break;
            }

            channel.nodeIndex = gltfChannel->target_node - data->nodes;
            channel.samplerIndex = gltfChannel->sampler - gltfAnimation->samplers;

            animation.channels.emplace_back(channel);
        }

        animations.emplace_back(animation);
    }
}

Scene::~Scene() {
    clear();
}

void Scene::updateAnimation(float deltaTime) {
    for (auto &animation: animations) {
        animation.currentTime += deltaTime;
        while (animation.currentTime > animation.end) {
            animation.currentTime -= animation.end;
        }

        for (auto &channel: animation.channels) {
            const AnimationSampler &sampler = animation.samplers[channel.samplerIndex];

            if (sampler.inputs.size() > sampler.outputs.size()) {
                std::cout << "Invalid sampler input/output sizes" << std::endl;
            }

            for (size_t timestamp_i = 0; timestamp_i < sampler.inputs.size() - 1; timestamp_i++) {
                if (animation.currentTime > sampler.inputs[timestamp_i] &&
                    animation.currentTime < sampler.inputs[timestamp_i + 1]) {
                    float td = sampler.inputs[timestamp_i + 1] - sampler.inputs[timestamp_i];
                    float interpolateValue = (animation.currentTime - sampler.inputs[timestamp_i]) / td;

                    switch (sampler.interpolation) {
                        case AnimationSampler::eLinear: {
                            if (sampler.inputs.size() != sampler.outputs.size()) {
                                std::cout << "input/output size must be equal for linear interpolation" << std::endl;
                            }
                            switch (channel.path) {
                                case AnimationChannel::eTranslation: {
                                    nodes[channel.nodeIndex]->translation = glm::mix(
                                            sampler.outputs[timestamp_i], sampler.outputs[timestamp_i + 1],
                                            interpolateValue
                                    );
                                    break;
                                }
                                case AnimationChannel::eRotation: {
                                    glm::quat q1;
                                    q1.x = sampler.outputs[timestamp_i].x;
                                    q1.y = sampler.outputs[timestamp_i].y;
                                    q1.z = sampler.outputs[timestamp_i].z;
                                    q1.w = sampler.outputs[timestamp_i].w;

                                    glm::quat q2;
                                    q2.x = sampler.outputs[timestamp_i + 1].x;
                                    q2.y = sampler.outputs[timestamp_i + 1].y;
                                    q2.z = sampler.outputs[timestamp_i + 1].z;
                                    q2.w = sampler.outputs[timestamp_i + 1].w;

                                    nodes[channel.nodeIndex]->rotation = glm::normalize(
                                            glm::slerp(q1, q2, interpolateValue));
                                    break;
                                }
                                case AnimationChannel::eScale: {
                                    nodes[channel.nodeIndex]->scale = glm::mix(
                                            sampler.outputs[timestamp_i], sampler.outputs[timestamp_i + 1],
                                            interpolateValue
                                    );
                                    break;
                                }
                                case AnimationChannel::eWeights: {
                                    std::cout << "weights animation channel not yet supported" << std::endl;
                                }
                            }
                            break;
                        }
                        case AnimationSampler::eCubicSpline: {
                            if (sampler.inputs.size() * 3 != sampler.outputs.size()) {
                                std::cout << "output size must be 3x input for cubic spline interpolation" << std::endl;
                            }

                            float t = interpolateValue;
                            float t2 = t * t;
                            float t3 = t2 * t;

                            glm::vec4 ak = sampler.outputs[timestamp_i * 3];
                            glm::vec4 vk = sampler.outputs[timestamp_i * 3 + 1];
                            glm::vec4 bk = sampler.outputs[timestamp_i * 3 + 2];

                            glm::vec4 ak_p1 = sampler.outputs[(timestamp_i + 1) * 3];
                            glm::vec4 vk_p1 = sampler.outputs[(timestamp_i + 1) * 3 + 1];

                            // calculation per gltf spec
                            glm::vec4 result = (2 * t3 - 3 * t2 + 1) * vk +
                                               td * (t3 - 2 * t2 + t) * bk +
                                               (-2 * t3 + 3 * t2) * vk_p1 +
                                               td * (t3 - t2) * ak_p1;

                            switch (channel.path) {
                                case AnimationChannel::eTranslation: {
                                    nodes[channel.nodeIndex]->translation = result;
                                    break;
                                }
                                case AnimationChannel::eRotation: {
                                    result = normalize(result);
                                    glm::quat quat;
                                    quat.x = result.x;
                                    quat.y = result.y;
                                    quat.z = result.z;
                                    quat.w = result.w;
                                    nodes[channel.nodeIndex]->rotation = quat;
                                    break;
                                }
                                case AnimationChannel::eScale: {
                                    nodes[channel.nodeIndex]->scale = result;
                                    break;
                                }
                                case AnimationChannel::eWeights: {
                                    std::cout << "weights animation channel not yet supported" << std::endl;
                                }
                            }
                            break;
                        }
                        case AnimationSampler::eStep: {
                            if (sampler.inputs.size() != sampler.outputs.size()) {
                                std::cout << "input/output size must be equal for step interpolation" << std::endl;
                            }

                            switch (channel.path) {
                                case AnimationChannel::eTranslation: {
                                    nodes[channel.nodeIndex]->translation = sampler.outputs[timestamp_i];
                                    break;
                                }
                                case AnimationChannel::eRotation: {
                                    glm::quat quat;
                                    quat.x = sampler.outputs[timestamp_i].x;
                                    quat.y = sampler.outputs[timestamp_i].y;
                                    quat.z = sampler.outputs[timestamp_i].z;
                                    quat.w = sampler.outputs[timestamp_i].w;
                                    nodes[channel.nodeIndex]->rotation = quat;
                                    break;
                                }
                                case AnimationChannel::eScale: {
                                    nodes[channel.nodeIndex]->scale = sampler.outputs[timestamp_i];
                                    break;
                                }
                                case AnimationChannel::eWeights: {
                                    std::cout << "weights animation channel not yet supported" << std::endl;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}

void Scene::parseSkins(const cgltf_data *data) {
    for (size_t skin_i = 0; skin_i < data->skins_count; skin_i++) {
        const cgltf_skin *gltfSkin = &data->skins[skin_i];
        auto skin = std::make_unique<Skin>();

        if (gltfSkin->name) {
            skin->name = gltfSkin->name;
        }

        if (gltfSkin->skeleton) {
            skin->skeletonNodeIndex = gltfSkin->skeleton - data->nodes;
        }

        for (size_t joint_i = 0; joint_i < gltfSkin->joints_count; joint_i++) {
            skin->jointNodeIndices.emplace_back(static_cast<uint32_t>(gltfSkin->joints[joint_i] - data->nodes));
        }

        const cgltf_accessor *inverseBindMatrixAccessor = gltfSkin->inverse_bind_matrices;
        const cgltf_buffer_view *inverseBindMatrixBufferView = inverseBindMatrixAccessor->buffer_view;
        const auto *inverseBindMatrixBuffer = static_cast<const std::byte *>(inverseBindMatrixBufferView->buffer->data);
        skin->inverseBindMatrices.resize(inverseBindMatrixAccessor->count);
        memcpy(skin->inverseBindMatrices.data(),
               &inverseBindMatrixBuffer[inverseBindMatrixAccessor->offset + inverseBindMatrixBufferView->offset],
               inverseBindMatrixAccessor->count * sizeof(glm::mat4));

        jointOffsets.emplace_back(std::accumulate(skinJointCounts.begin(), skinJointCounts.end(), 0));
        skinJointCounts.emplace_back(static_cast<uint32_t>(gltfSkin->joints_count));

        skins.emplace_back(std::move(skin));
    }
}
