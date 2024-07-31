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

void Scene::parseTextures(const cgltf_data *data) {
    for (auto sampler_i = 0; sampler_i < data->samplers_count; sampler_i++) {
        const cgltf_sampler *gltfSampler = &data->samplers[sampler_i];
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = extractGltfMinFilter(gltfSampler->min_filter);
        samplerInfo.magFilter = extractGltfMagFilter(gltfSampler->mag_filter);
        samplerInfo.addressModeU = extractGltfWrapMode(gltfSampler->wrap_s);
        samplerInfo.addressModeV = extractGltfWrapMode(gltfSampler->wrap_t);

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
        if (images[imageIndex].has_value()) {
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

            const cgltf_texture_view *baseColorTextureView = &gltfMaterial->pbr_metallic_roughness.base_color_texture;
            if (baseColorTextureView && baseColorTextureView->texture) {
                material.baseTextureOffset = baseColorTextureView->texture - data->textures;
            } else {
                material.baseTextureOffset = OPAQUE_WHITE_TEXTURE;
            }
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
                                                                   false);

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
                                                                   false);

                    images.emplace_back(newImage);

                    stbi_image_free(stbData);
                } else {
                    std::cerr << "Failed to read image file " << imageFile << std::endl;
                    images.emplace_back();
                }
            }
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

//todo: wip
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
        }
    }
}

Scene::~Scene() {
    clear();
}
