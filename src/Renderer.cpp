#include "Renderer.h"

#include <VulkanInit.h>
#include <VulkanPipeline.h>
#include <VulkanUtils.h>
#include <stack>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr uint32_t MAX_TEXTURES = 1024;

static constexpr uint32_t UNIFORM_BINDING = 0;
static constexpr uint32_t TRANSFORM_BINDING = 1;
static constexpr uint32_t MATERIAL_BINDING = 2;
static constexpr uint32_t JOINT_BINDING = 3;
static constexpr uint32_t MODEL_TRANSFORM_BINDING = 4;
static constexpr uint32_t LIGHT_BINDING = 5;
static constexpr uint32_t IRRADIANCE_MAP_BINDING = 6;
static constexpr uint32_t PREFILTERED_CUBE_BINDING = 7;
static constexpr uint32_t BRDF_LUT_BINDING = 8;
static constexpr uint32_t TEXTURE_BINDING = 9;

Renderer::Renderer() {
    setupVulkan();
}

Renderer::~Renderer() {
    terminateVulkan();
}

void Renderer::run() {
    m_timer.reset();

    while (!glfwWindowShouldClose(m_vulkanContext.window)) {
        glfwPollEvents();

        m_timer.tick();
        std::cout << "frame time: " << m_timer.deltaTime() << std::endl;

        m_camera.update();

        vkWaitForFences(m_vulkanContext.device, 1, &m_vulkanContext.frames[currentFrame].renderFence, VK_TRUE, 1e9);
        VK_CHECK(vkResetFences(m_vulkanContext.device, 1, &m_vulkanContext.frames[currentFrame].renderFence))

        uint32_t imageIndex;
        VkResult swapchainRet = vkAcquireNextImageKHR(m_vulkanContext.device, m_vulkanContext.swapchain, 1e9,
                                                      m_vulkanContext.frames[currentFrame].imageAvailableSemaphore,
                                                      VK_NULL_HANDLE,
                                                      &imageIndex);
        if (swapchainRet == VK_ERROR_OUT_OF_DATE_KHR) {
            m_vulkanContext.resizeWindow();
            continue;
        }

        VkCommandBuffer cmd = m_vulkanContext.frames[currentFrame].commandBuffer;

        VK_CHECK(vkResetCommandBuffer(cmd, 0))

        VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

        VkUtil::transitionImage(cmd, m_vulkanContext.drawImage.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        drawGeometry(cmd);

        VkUtil::transitionImage(cmd, m_vulkanContext.drawImage.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VkUtil::transitionImage(cmd, m_vulkanContext.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkUtil::copyImageToImage(cmd, m_vulkanContext.drawImage.image, m_vulkanContext.swapchainImages[imageIndex],
                                 m_vulkanContext.windowExtent,
                                 m_vulkanContext.windowExtent);

        VkUtil::transitionImage(cmd, m_vulkanContext.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VK_CHECK(vkEndCommandBuffer(cmd))

        VkCommandBufferSubmitInfo cmdSubmitInfo = VkInit::commandBufferSubmitInfo(cmd);

        VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                     m_vulkanContext.frames[currentFrame].imageAvailableSemaphore);
        VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                                       m_vulkanContext.frames[currentFrame].renderSemaphore);

        VkSubmitInfo2 submit = VkInit::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

        VK_CHECK(
                vkQueueSubmit2(m_vulkanContext.graphicsQueue, 1, &submit,
                               m_vulkanContext.frames[currentFrame].renderFence))

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &m_vulkanContext.swapchain.swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &m_vulkanContext.frames[currentFrame].renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRet = vkQueuePresentKHR(m_vulkanContext.presentQueue, &presentInfo);
        if (presentRet == VK_ERROR_OUT_OF_DATE_KHR) {
            m_vulkanContext.resizeWindow();
        }

        currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
    }
}

uint32_t Renderer::loadGltf(std::filesystem::path filePath) {
    auto scene = std::make_unique<GltfScene>(&m_vulkanContext);
    scene->load(filePath);

    if (!scene->loaded) {
        return LOAD_FAILED;
    }

    uint32_t modelId = getLoadedModelId();

    ModelData modelData = {};

    modelData.vertexOffset = m_vertices.size();
    m_vertices.insert(m_vertices.end(), scene->vertices.begin(), scene->vertices.end());

    modelData.indexOffset = m_indices.size();
    m_indices.insert(m_indices.end(), scene->indices.begin(), scene->indices.end());

    // make sure index points to the right vertex
    for (size_t index_i = modelData.indexOffset; index_i < m_indices.size(); index_i++) {
        m_indices[index_i] += modelData.vertexOffset;
    }

    modelData.textureOffset = m_textures.size();
    m_textures.insert(m_textures.end(), scene->textures.begin(), scene->textures.end());

    modelData.materialOffset = m_materials.size();
    m_materials.insert(m_materials.end(), scene->materials.begin(), scene->materials.end());

    // make sure material points to the right texture
    for (size_t material_i = modelData.materialOffset; material_i < m_materials.size(); material_i++) {
        if (m_materials[material_i].baseTextureOffset == NO_TEXTURE_INDEX) {
            m_materials[material_i].baseTextureOffset = 0; // opaque white texture at index 0
        } else {
            m_materials[material_i].baseTextureOffset += modelData.textureOffset;
        }

        if (m_materials[material_i].metallicRoughnessTextureOffset == NO_TEXTURE_INDEX) {
            m_materials[material_i].metallicRoughnessTextureOffset = 1; // <0.0, 1.0, 1.0> texture at index 1
        } else {
            m_materials[material_i].metallicRoughnessTextureOffset += modelData.textureOffset;
        }

        if (m_materials[material_i].normalTextureOffset == NO_TEXTURE_INDEX) {
            m_materials[material_i].normalTextureOffset = 2; // <0.5, 0.5, 1.0> texture at index 2
        } else {
            m_materials[material_i].normalTextureOffset += modelData.textureOffset;
        }

        if (m_materials[material_i].emissiveTextureOffset == NO_TEXTURE_INDEX) {
            m_materials[material_i].emissiveTextureOffset = 0; // opaque white texture at index 0
        } else {
            m_materials[material_i].emissiveTextureOffset += modelData.textureOffset;
        }

        if (m_materials[material_i].occlusionTextureOffset == NO_TEXTURE_INDEX) {
            m_materials[material_i].occlusionTextureOffset = 0; // opaque white texture at index 0
        } else {
            m_materials[material_i].occlusionTextureOffset += modelData.textureOffset;
        }
    }

    DescriptorWriter writer;
    for (size_t texture_i = modelData.textureOffset; texture_i < m_textures.size(); texture_i++) {
        writer.writeImage(TEXTURE_BINDING, m_textures[texture_i]->imageview, m_textures[texture_i]->sampler,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          texture_i);
    }
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        writer.updateSet(m_vulkanContext.device, bindlessDescriptorSets[frame_i]);
    }

    m_sceneDatas.emplace_back(std::move(scene), modelId);

    m_modelDatas.emplace_back(modelData);

    destroyStaticBuffers();
    createStaticBuffers();

    return modelId;
}

void Renderer::createStaticBuffers() {
    const size_t vertexBufferSize = m_vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = m_indices.size() * sizeof(uint32_t);
    const size_t materialBufferSize = m_materials.size() * sizeof(Material);

    const size_t stagingBufferSize = vertexBufferSize + indexBufferSize + materialBufferSize;

    VulkanBuffer stagingBuffer = m_vulkanContext.createBuffer(stagingBufferSize,
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    void *data = stagingBuffer.info.pMappedData;

    m_boundedVertexBuffer = m_vulkanContext.createBuffer(vertexBufferSize,
                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                         VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(data, m_vertices.data(), vertexBufferSize);

    if (indexBufferSize != 0) {
        m_boundedIndexBuffer = m_vulkanContext.createBuffer(indexBufferSize,
                                                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
        memcpy(static_cast<uint8_t *>(data) + vertexBufferSize, m_indices.data(), indexBufferSize);
    }

    m_boundedMaterialBuffer = m_vulkanContext.createBuffer(materialBufferSize,
                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                           VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(static_cast<uint8_t *>(data) + vertexBufferSize + indexBufferSize, m_materials.data(),
           materialBufferSize);

    m_vulkanContext.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy = {0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_boundedVertexBuffer.buffer,
                        1, &vertexCopy);

        if (indexBufferSize != 0) {
            VkBufferCopy indexCopy = {0};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_boundedIndexBuffer.buffer,
                            1, &indexCopy);
        }

        VkBufferCopy materialCopy = {0};
        materialCopy.dstOffset = 0;
        materialCopy.srcOffset = vertexBufferSize + indexBufferSize;
        materialCopy.size = materialBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_boundedMaterialBuffer.buffer,
                        1, &materialCopy);
    });

    m_vulkanContext.destroyBuffer(stagingBuffer);
}

void Renderer::setupVulkan() {
    m_vulkanContext.windowExtent = {1920, 1080};
    m_vulkanContext.init();

    glfwSetWindowUserPointer(m_vulkanContext.window, &m_camera);
    glfwSetCursorPosCallback(m_vulkanContext.window, Camera::mouseCallback);
    glfwSetKeyCallback(m_vulkanContext.window, Camera::keyCallback);
    glfwSetMouseButtonCallback(m_vulkanContext.window, Camera::mouseButtonCallback);

    DescriptorLayoutBuilder descriptorLayoutBuilder;
    descriptorLayoutBuilder.addBinding(UNIFORM_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptorLayoutBuilder.addBinding(TRANSFORM_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorLayoutBuilder.addBinding(MATERIAL_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorLayoutBuilder.addBinding(JOINT_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorLayoutBuilder.addBinding(MODEL_TRANSFORM_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorLayoutBuilder.addBinding(LIGHT_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorLayoutBuilder.addBinding(IRRADIANCE_MAP_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorLayoutBuilder.addBinding(PREFILTERED_CUBE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorLayoutBuilder.addBinding(BRDF_LUT_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorLayoutBuilder.addBinding(TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    std::array<VkDescriptorBindingFlags, 10> flagArray = {
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
//            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
//            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //todo: variable descriptor count doesn't work?
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags = {};
    bindFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindFlags.bindingCount = static_cast<uint32_t>(flagArray.size());
    bindFlags.pBindingFlags = flagArray.data();
    descriptorLayoutBuilder.bindings[TEXTURE_BINDING].descriptorCount = MAX_TEXTURES;

    globalDescriptorLayout = descriptorLayoutBuilder.build(m_vulkanContext.device,
                                                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           &bindFlags,
                                                           VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

    std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         5},
    };

    globalDescriptors = DescriptorAllocator();
    globalDescriptors.init(m_vulkanContext.device, 1024, frameSizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        bindlessDescriptorSets[frame_i] = globalDescriptors.allocate(m_vulkanContext.device, globalDescriptorLayout);
    }

    VkPushConstantRange pushConstantsRange = {};
    pushConstantsRange.offset = 0;
    pushConstantsRange.size = sizeof(PushConstantsBindless);
    pushConstantsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &globalDescriptorLayout;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantsRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vulkanContext.device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout))

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(m_vulkanContext.createShaderModule("shaders/pbr/mesh_bindless.vert.spv", &triangleVertShader))
    VK_CHECK(m_vulkanContext.createShaderModule("shaders/pbr/texture_bindless.frag.spv", &triangleFragShader))

    PipelineBuilder trianglePipelineBuilder;
    trianglePipelineBuilder
            .setLayout(trianglePipelineLayout)
            .setShaders(triangleVertShader, triangleFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setMultisamplingNone()
            .disableBlending()
            .enableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
            .setColorAttachmentFormat(m_vulkanContext.drawImage.imageFormat)
            .setDepthAttachmentFormat(m_vulkanContext.depthImage.imageFormat);

    trianglePipeline = trianglePipelineBuilder.build(m_vulkanContext.device);

    vkDestroyShaderModule(m_vulkanContext.device, triangleVertShader, nullptr);
    vkDestroyShaderModule(m_vulkanContext.device, triangleFragShader, nullptr);

    m_uniformBuffer = m_vulkanContext.createBuffer(sizeof(GlobalUniformData),
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        m_boundedUniformBuffers[frame_i] = m_vulkanContext.createBuffer(sizeof(GlobalUniformData),
                                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    m_skybox = std::make_unique<Skybox>(&this->m_vulkanContext);
//    m_skybox->load("assets/skyboxes/equirectangular/816-hdri-skies-com.hdr");
    m_skybox->load("assets/skyboxes/equirectangular/free_hdri_sky_816.jpg");
    m_skybox->init();

    initDefaultData();

    initSkyboxPipeline();
}

void Renderer::terminateVulkan() {
    vkDeviceWaitIdle(m_vulkanContext.device);

    // todo: move memory allocation stuff in scene to renderer
    m_sceneDatas.clear();

    m_skybox.reset();

    m_vulkanContext.destroyImage(opaqueWhiteTextureImage);
    m_vulkanContext.destroyImage(opaqueCyanTextureImage);
    m_vulkanContext.destroyImage(defaultNormalTextureImage);
    vkDestroySampler(m_vulkanContext.device, defaultSampler, nullptr);

    globalDescriptors.destroyPools(m_vulkanContext.device);
    skyboxDescriptors.destroyPools(m_vulkanContext.device);

    destroyStaticBuffers();
    if (m_lightBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_lightBuffer);
    }
    if (m_modelTransformBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_modelTransformBuffer);
    }

    m_vulkanContext.destroyBuffer(m_uniformBuffer);
    m_vulkanContext.destroyBuffer(m_transformBuffer);
    m_vulkanContext.destroyBuffer(m_jointBuffer);

    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        if (m_boundedUniformBuffers[frame_i].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedUniformBuffers[frame_i]);
        }
        if (m_boundedTransformBuffers[frame_i].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedTransformBuffers[frame_i]);
        }
        if (m_boundedJointBuffers[frame_i].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedJointBuffers[frame_i]);
        }
        if (m_boundedLightBuffers[frame_i].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedLightBuffers[frame_i]);
        }
        if (m_boundedModelTransformBuffer[frame_i].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedModelTransformBuffer[frame_i]);
        }
    }

    vkDestroyDescriptorSetLayout(m_vulkanContext.device, globalDescriptorLayout, nullptr);
    vkDestroyPipelineLayout(m_vulkanContext.device, trianglePipelineLayout, nullptr);
    vkDestroyPipeline(m_vulkanContext.device, trianglePipeline, nullptr);

    vkDestroyDescriptorSetLayout(m_vulkanContext.device, skyboxDescriptorLayout, nullptr);
    vkDestroyPipelineLayout(m_vulkanContext.device, skyboxPipelineLayout, nullptr);
    vkDestroyPipeline(m_vulkanContext.device, skyboxPipeline, nullptr);

    m_vulkanContext.terminate();
}

void Renderer::drawGeometry(VkCommandBuffer cmd) {
    if (m_vertices.empty()) {
        return;
    }

    updateLightPos(0);
    rotateRenderObjects();
    updateLightBuffer(cmd);

    createDrawDatas(cmd);

    VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(m_vulkanContext.drawImage.imageView, nullptr);
    VkRenderingAttachmentInfo depthAttachment = VkInit::depthAttachmentInfo(m_vulkanContext.depthImage.imageView);

    VkViewport viewport = VkInit::viewport(m_vulkanContext.windowExtent.width, m_vulkanContext.windowExtent.height);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_vulkanContext.windowExtent.width;
    scissor.extent.height = m_vulkanContext.windowExtent.height;

    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(70.f), viewport.width / viewport.height, 0.001f, 1e9f);
    projection[1][1] *= -1; // flip y

    m_globalUniformData.view = view;
    m_globalUniformData.proj = projection;
    m_globalUniformData.projView = projection * view;
    m_globalUniformData.cameraPos = m_camera.position;
    m_globalUniformData.numLights = m_lights.size();

    GlobalUniformData *globalUniformData = static_cast<GlobalUniformData *>(m_uniformBuffer.info.pMappedData);
    *globalUniformData = m_globalUniformData;

    VkBufferCopy uniformCopy = {0};
    uniformCopy.dstOffset = 0;
    uniformCopy.srcOffset = 0;
    uniformCopy.size = sizeof(GlobalUniformData);
    vkCmdCopyBuffer(cmd, m_uniformBuffer.buffer, m_boundedUniformBuffers[currentFrame].buffer,
                    1, &uniformCopy);

    DescriptorWriter writer;
    writer.writeBuffer(UNIFORM_BINDING, m_boundedUniformBuffers[currentFrame].buffer, sizeof(GlobalUniformData), 0,
                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.writeBuffer(MATERIAL_BINDING, m_boundedMaterialBuffer.buffer, m_boundedMaterialBuffer.info.size, 0,
                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer.writeBuffer(TRANSFORM_BINDING, m_boundedTransformBuffers[currentFrame].buffer,
                       m_boundedTransformBuffers[currentFrame].info.size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer.writeBuffer(JOINT_BINDING, m_boundedJointBuffers[currentFrame].buffer,
                       m_boundedJointBuffers[currentFrame].info.size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    if (m_boundedModelTransformBuffer[currentFrame].buffer != VK_NULL_HANDLE) {
        writer.writeBuffer(MODEL_TRANSFORM_BINDING, m_boundedModelTransformBuffer[currentFrame].buffer,
                           m_boundedModelTransformBuffer[currentFrame].info.size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if (m_boundedLightBuffers[currentFrame].buffer != VK_NULL_HANDLE) {
        writer.writeBuffer(LIGHT_BINDING, m_boundedLightBuffers[currentFrame].buffer,
                           m_boundedLightBuffers[currentFrame].info.size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    writer.writeImage(IRRADIANCE_MAP_BINDING, m_skybox->irradianceMap.imageView, m_skybox->sampler,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    writer.writeImage(PREFILTERED_CUBE_BINDING, m_skybox->prefilteredCube.imageView, m_skybox->sampler,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    writer.writeImage(BRDF_LUT_BINDING, m_skybox->brdfLUT.imageView, m_skybox->sampler,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    writer.updateSet(m_vulkanContext.device, bindlessDescriptorSets[currentFrame]);

    VkRenderingInfo renderInfo = VkInit::renderingInfo(m_vulkanContext.windowExtent, &colorAttachment,
                                                       &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipelineLayout,
                            0, 1, &bindlessDescriptorSets[currentFrame], 0, nullptr);
    if (!m_indices.empty()) {
        vkCmdBindIndexBuffer(cmd, m_boundedIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    PushConstantsBindless pcb = {};
    pcb.vertexBuffer = m_vulkanContext.getBufferAddress(m_boundedVertexBuffer);

    for (const auto &drawData: m_drawDatas) {
        if (drawData.instanceCount == 0) {
            continue;
        }

        pcb.transformOffset = drawData.transformOffset;
        pcb.materialOffset = drawData.materialOffset;
        pcb.jointOffset = drawData.jointOffset;
        pcb.modelTransformOffset = drawData.modelTransformOffset;

        vkCmdPushConstants(cmd, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(PushConstantsBindless),
                           &pcb);
        if (drawData.hasIndices) {
            vkCmdDrawIndexed(cmd, drawData.indexCount, drawData.instanceCount, drawData.indexOffset, 0, 0);
        } else {
            vkCmdDraw(cmd, drawData.vertexCount, drawData.instanceCount, drawData.vertexOffset, 0);
        }
    }

    // skybox
    DescriptorWriter skyboxWriter;
    skyboxWriter.writeImage(0, m_skybox->prefilteredCube.imageView, m_skybox->sampler,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    skyboxWriter.updateSet(m_vulkanContext.device, skyboxDescriptorSets[currentFrame]);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

    PushConstantsSkybox pcs = {};
    pcs.matrix = projection * glm::mat4(glm::mat3(view));
    pcs.vertexBuffer = m_vulkanContext.getBufferAddress(m_skybox->vertexBuffer);
    vkCmdPushConstants(cmd, skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsSkybox), &pcs);
    vkCmdBindIndexBuffer(cmd, m_skybox->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout,
                            0, 1, &skyboxDescriptorSets[currentFrame], 0, nullptr);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    vkCmdEndRendering(cmd);
}

void Renderer::initDefaultData() {
    m_textures.clear();
    m_materials.clear();
    m_joints = {glm::mat4(1.f)};

    opaqueWhiteTextureImage = m_vulkanContext.createImage(&VkUtil::opaqueWhite, VkExtent3D(1, 1, 1),
                                                          VK_FORMAT_R8G8B8A8_UNORM,
                                                          VK_IMAGE_USAGE_SAMPLED_BIT, false);
    defaultNormalTextureImage = m_vulkanContext.createImage(&VkUtil::defaultNormalMapColor, VkExtent3D(1, 1, 1),
                                                            VK_FORMAT_R8G8B8A8_UNORM,
                                                            VK_IMAGE_USAGE_SAMPLED_BIT, false);
    opaqueCyanTextureImage = m_vulkanContext.createImage(&VkUtil::opaqueCyan, VkExtent3D(1, 1, 1),
                                                         VK_FORMAT_R8G8B8A8_UNORM,
                                                         VK_IMAGE_USAGE_SAMPLED_BIT, false);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = 16.f;

    vkCreateSampler(m_vulkanContext.device, &samplerInfo, nullptr, &defaultSampler);

    Texture opaqueWhiteTexture = {
            "opaque_white_texture",
            opaqueWhiteTextureImage.imageView,
            defaultSampler
    };

    Texture defaultNormalTexture = {
            "default_normal_texture",
            defaultNormalTextureImage.imageView,
            defaultSampler
    };

    Texture opaqueCyanTexture = {
            "opaque_cyan_texture",
            opaqueCyanTextureImage.imageView,
            defaultSampler
    };

    m_textures.emplace_back(std::make_shared<Texture>(opaqueWhiteTexture));
    m_textures.emplace_back(std::make_shared<Texture>(opaqueCyanTexture));
    m_textures.emplace_back(std::make_shared<Texture>(defaultNormalTexture));

    Material defaultMaterial = {
            .baseColorFactor = {1.f, 1.f, 1.f, 1.f},
            .metallicFactor = 1.f,
            .roughnessFactor = 1.f,
            .baseTextureOffset = 0,  // the default opaque white texture is at index 0
            .metallicRoughnessTextureOffset = 1, // the default roughness texture is at index 1
            .normalTextureOffset = 2, // the default normal texture is at index 2
    };

    m_materials.emplace_back(defaultMaterial);

    DescriptorWriter writer;
    for (size_t texture_i = 0; texture_i < m_textures.size(); texture_i++) {
        writer.writeImage(TEXTURE_BINDING, m_textures[texture_i]->imageview, m_textures[texture_i]->sampler,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          texture_i);
    }
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        writer.updateSet(m_vulkanContext.device, bindlessDescriptorSets[frame_i]);
    }
}

void Renderer::destroyStaticBuffers() {
    if (m_boundedIndexBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_boundedIndexBuffer);
    }
    if (m_boundedVertexBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_boundedVertexBuffer);
    }
    if (m_boundedMaterialBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_boundedMaterialBuffer);
    }
}

void Renderer::createDrawDatas(VkCommandBuffer cmd) {
    m_joints = {glm::mat4(1.f)}; // joint index zero is identity matrix
    m_transforms = {glm::mat4(1.f)}; // transform index zero is identity matrix
    m_drawDatas.clear();
    m_modelTransforms.clear();

    for (auto &scenePair: m_sceneDatas) {
        auto scene = scenePair.first.get();
        auto &modelData = m_modelDatas[scenePair.second];

        scene->updateAnimation(m_timer.deltaTime());

        modelData.drawDataCount = 0;
        modelData.drawDataOffset = m_drawDatas.size();

        modelData.jointOffset = m_joints.size();
        size_t numSceneJoints = 0;
        for (const auto &num: scene->skinJointCounts) {
            numSceneJoints += num;
        }
        m_joints.resize(m_joints.size() + numSceneJoints);

        // update world transform of nodes
        for (const auto &topLevelNode: scene->topLevelNodes) {
            std::stack<std::shared_ptr<Node>> nodeStack;
            nodeStack.push(topLevelNode);

            while (!nodeStack.empty()) {
                auto currentNode = nodeStack.top();
                nodeStack.pop();

                if (auto parentNode = currentNode->parent.lock()) {
                    currentNode->worldTransform = parentNode->worldTransform * currentNode->getLocalTransform();
                } else {
                    currentNode->worldTransform = currentNode->getLocalTransform();
                }
                for (const auto &child: currentNode->children) {
                    nodeStack.push(child);
                }
            }
        }

        // generate draw datas
        for (const auto &topLevelNode: scene->topLevelNodes) {
            std::stack<std::shared_ptr<Node> > nodeStack;
            nodeStack.push(topLevelNode);

            while (!nodeStack.empty()) {
                auto currentNode = nodeStack.top();
                nodeStack.pop();

                if (currentNode->hasSkin) {
                    glm::mat4 inverseWorldTransform = glm::inverse(currentNode->worldTransform);
                    const Skin *skin = scene->skins[currentNode->skin].get();
                    for (size_t joint_i = 0; joint_i < skin->jointNodeIndices.size(); joint_i++) {
                        m_joints[modelData.jointOffset + scene->jointOffsets[currentNode->skin] + joint_i] =
                                inverseWorldTransform * scene->nodes[skin->jointNodeIndices[joint_i]]->worldTransform *
                                skin->inverseBindMatrices[joint_i];
                    }
                }

                if (const auto &nodeMesh = currentNode->mesh) {
                    for (const auto &meshPrimitive: nodeMesh->meshPrimitives) {
                        DrawData drawData = {};
                        drawData.hasIndices = meshPrimitive.hasIndices;
                        drawData.indexOffset = meshPrimitive.indexStart + modelData.indexOffset;
                        drawData.vertexOffset = meshPrimitive.vertexStart + modelData.vertexOffset;
                        drawData.indexCount = meshPrimitive.indexCount;
                        drawData.vertexCount = meshPrimitive.vertexCount;
                        if (meshPrimitive.materialOffset == NO_MATERIAL_INDEX) {
                            drawData.materialOffset = 0; // default material at index 0
                        } else {
                            drawData.materialOffset = meshPrimitive.materialOffset + modelData.materialOffset;
                        }
                        drawData.transformOffset = m_transforms.size();
                        if (currentNode->hasSkin) {
                            drawData.jointOffset = scene->jointOffsets[currentNode->skin] + modelData.jointOffset;
                        } else {
                            drawData.jointOffset = 0;
                        }

                        modelData.drawDataCount++;
                        m_drawDatas.emplace_back(drawData);
                    }
                    m_transforms.emplace_back(currentNode->worldTransform);
                }

                for (const auto &child: currentNode->children) {
                    nodeStack.push(child);
                }
            }
        }
    }

    for (auto &generatedMeshPair: m_generatedMeshDatas) {
        auto &meshBuffers = generatedMeshPair.first;
        auto &modelData = m_modelDatas[generatedMeshPair.second];

        modelData.drawDataCount = 1;
        modelData.drawDataOffset = m_drawDatas.size();

        DrawData drawData = {};

        drawData.hasIndices = true;
        drawData.indexOffset = modelData.indexOffset;
        drawData.vertexOffset = modelData.vertexOffset;
        drawData.indexCount = meshBuffers->indices.size();
        drawData.vertexCount = meshBuffers->vertices.size();

        // todo: using default material and texture for now
        drawData.materialOffset = 0;
        drawData.jointOffset = 0;

        drawData.transformOffset = 0; // identity matrix at index 0

        m_drawDatas.emplace_back(drawData);
    }

    // create model transform buffer and update DrawData instance count
    // model transform for instance is at index (modelTransformOffset + gl_InstanceIndex) of modelTransformBuffer
    std::vector<std::pair<uint32_t, std::vector<glm::mat4>>> modelsDrawn;
    for (auto &renderObject: m_renderObjects) {
        uint32_t modelId = renderObject.second.modelId;
        glm::mat4 &modelTransform = renderObject.second.modelMatrix;

        auto it = std::find_if(modelsDrawn.begin(), modelsDrawn.end(),
                               [modelId](const std::pair<uint32_t, std::vector<glm::mat4>> &element) {
                                   return element.first == modelId;
                               });

        if (it != modelsDrawn.end()) {
            it->second.emplace_back(modelTransform);
        } else {
            modelsDrawn.emplace_back(modelId, std::vector<glm::mat4>{modelTransform});
        }
    }

    for (auto &model: modelsDrawn) {
        uint32_t modelId = model.first;
        uint32_t instanceCount = model.second.size();

        auto &modelData = m_modelDatas[modelId];

        for (size_t dd_i = 0; dd_i < modelData.drawDataCount; dd_i++) {
            auto &drawData = m_drawDatas[modelData.drawDataOffset + dd_i];
            drawData.modelTransformOffset = m_modelTransforms.size();
            drawData.instanceCount = instanceCount;
        }

        for (auto &modelMatrix: model.second) {
            m_modelTransforms.emplace_back(modelMatrix);
        }
    }

    // (re)create buffers if size changes
    uint32_t transformBufferSize = m_transforms.size() * sizeof(glm::mat4);
    uint32_t jointBufferSize = m_joints.size() * sizeof(glm::mat4);
    uint32_t modelTransformBufferSize = m_modelTransforms.size() * sizeof(glm::mat4);

    if (transformBufferSize != 0 && transformBufferSize != m_transformBuffer.info.size) {
        if (m_transformBuffer.buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_transformBuffer);
        }
        m_transformBuffer = m_vulkanContext.createBuffer(transformBufferSize,
                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                         VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    if (transformBufferSize != 0 && transformBufferSize != m_boundedTransformBuffers[currentFrame].info.size) {
        if (m_boundedTransformBuffers[currentFrame].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedTransformBuffers[currentFrame]);
        }
        m_boundedTransformBuffers[currentFrame] = m_vulkanContext.createBuffer(transformBufferSize,
                                                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                               VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    if (jointBufferSize != 0 && jointBufferSize != m_jointBuffer.info.size) {
        if (m_jointBuffer.buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_jointBuffer);
        }
        m_jointBuffer = m_vulkanContext.createBuffer(jointBufferSize,
                                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                     VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    if (jointBufferSize != 0 && jointBufferSize != m_boundedJointBuffers[currentFrame].info.size) {
        if (m_boundedJointBuffers[currentFrame].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedJointBuffers[currentFrame]);
        }
        m_boundedJointBuffers[currentFrame] = m_vulkanContext.createBuffer(jointBufferSize,
                                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                           VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    if (modelTransformBufferSize != 0 && modelTransformBufferSize != m_modelTransformBuffer.info.size) {
        if (m_modelTransformBuffer.buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_modelTransformBuffer);
        }
        m_modelTransformBuffer = m_vulkanContext.createBuffer(modelTransformBufferSize,
                                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    if (modelTransformBufferSize != 0 &&
        modelTransformBufferSize != m_boundedModelTransformBuffer[currentFrame].info.size) {
        if (m_boundedModelTransformBuffer[currentFrame].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedModelTransformBuffer[currentFrame]);
        }
        m_boundedModelTransformBuffer[currentFrame] = m_vulkanContext.createBuffer(modelTransformBufferSize,
                                                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                                   VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    // update buffers contents
    if (transformBufferSize != 0) {
        memcpy(m_transformBuffer.info.pMappedData, m_transforms.data(), transformBufferSize);
        VkBufferCopy transformCopy = {0};
        transformCopy.dstOffset = 0;
        transformCopy.srcOffset = 0;
        transformCopy.size = transformBufferSize;
        vkCmdCopyBuffer(cmd, m_transformBuffer.buffer, m_boundedTransformBuffers[currentFrame].buffer,
                        1, &transformCopy);
    }

    if (jointBufferSize != 0) {
        memcpy(m_jointBuffer.info.pMappedData, m_joints.data(), jointBufferSize);
        VkBufferCopy jointCopy = {0};
        jointCopy.dstOffset = 0;
        jointCopy.srcOffset = 0;
        jointCopy.size = jointBufferSize;
        vkCmdCopyBuffer(cmd, m_jointBuffer.buffer, m_boundedJointBuffers[currentFrame].buffer,
                        1, &jointCopy);
    }

    if (modelTransformBufferSize != 0) {
        memcpy(m_modelTransformBuffer.info.pMappedData, m_modelTransforms.data(), modelTransformBufferSize);

        VkBufferCopy modelTransformCopy = {0};
        modelTransformCopy.dstOffset = 0;
        modelTransformCopy.srcOffset = 0;
        modelTransformCopy.size = modelTransformBufferSize;
        vkCmdCopyBuffer(cmd, m_modelTransformBuffer.buffer, m_boundedModelTransformBuffer[currentFrame].buffer,
                        1, &modelTransformCopy);
    }
}

void Renderer::addLight(Light light) {
    m_lights.emplace_back(light);
}

void Renderer::updateLightBuffer(VkCommandBuffer cmd) {
    if (m_lights.empty()) {
        return;
    }

    size_t lightBufferSize = m_lights.size() * sizeof(Light);

    if (m_lightBuffer.buffer != VK_NULL_HANDLE) {
        m_vulkanContext.destroyBuffer(m_lightBuffer);
    }

    m_lightBuffer = m_vulkanContext.createBuffer(lightBufferSize,
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    void *data = m_lightBuffer.info.pMappedData;
    memcpy(data, m_lights.data(), lightBufferSize);

    if (lightBufferSize != m_boundedLightBuffers[currentFrame].info.size) {
        if (m_boundedLightBuffers[currentFrame].buffer != VK_NULL_HANDLE) {
            m_vulkanContext.destroyBuffer(m_boundedLightBuffers[currentFrame]);
        }
        m_boundedLightBuffers[currentFrame] = m_vulkanContext.createBuffer(lightBufferSize,
                                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                           VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    VkBufferCopy lightCopy = {0};
    lightCopy.dstOffset = 0;
    lightCopy.srcOffset = 0;
    lightCopy.size = lightBufferSize;
    vkCmdCopyBuffer(cmd, m_lightBuffer.buffer, m_boundedLightBuffers[currentFrame].buffer,
                    1, &lightCopy);
}

void Renderer::updateLightPos(uint32_t lightIndex) {
//    if (lightIndex >= m_lights.size()) {
//        std::cout << "invalid light index" << std::endl;
//        return;
//    }
//
//    float speed = 2.f;
//
//    float newX = m_lights[lightIndex].position.x + m_timer.deltaTime() * speed;
//    if (newX >= 10) {
//        newX = -10;
//    }
//
//    m_lights[lightIndex].position.x = newX;
}

uint32_t Renderer::loadGeneratedMesh(MeshBuffers *meshBuffer) {
    ModelData modelData = {};

    uint32_t modelId = getLoadedModelId();

    modelData.vertexOffset = m_vertices.size();
    m_vertices.insert(m_vertices.end(), meshBuffer->vertices.begin(), meshBuffer->vertices.end());

    modelData.indexOffset = m_indices.size();
    m_indices.insert(m_indices.end(), meshBuffer->indices.begin(), meshBuffer->indices.end());

    // make sure index points to the right vertex
    for (size_t index_i = modelData.indexOffset; index_i < m_indices.size(); index_i++) {
        m_indices[index_i] += modelData.vertexOffset;
    }

    // todo: use default material and textures for now, implement properly later
    modelData.textureOffset = 0;
    modelData.materialOffset = 0;

    m_generatedMeshDatas.emplace_back(meshBuffer, modelId);

    m_modelDatas.emplace_back(modelData);

    destroyStaticBuffers();
    createStaticBuffers();

    return modelId;
}

uint32_t Renderer::getLoadedModelId() {
    return m_loadedModelCount++;
}

uint32_t Renderer::getRenderObjectId() {
    return m_renderObjectCount++;
}

uint32_t Renderer::addRenderObject(RenderObjectInfo info) {
    if (info.modelId > m_loadedModelCount - 1) {
        std::cout << "invalid modelId" << std::endl;
        return LOAD_FAILED;
    }

    uint32_t renderObjId = getRenderObjectId();
    m_renderObjects.emplace_back(renderObjId, info);

    return renderObjId;
}

void Renderer::rotateRenderObjects() {
    for (auto &renderObject: m_renderObjects) {
        renderObject.second.modelMatrix = glm::rotate(renderObject.second.modelMatrix, 0.2f * m_timer.deltaTime(),
                                                      glm::vec3(0.0, 1.0, 0.0));
    }
}

void Renderer::initSkyboxPipeline() {
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    skyboxDescriptorLayout = layoutBuilder.build(m_vulkanContext.device,
                                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    skyboxDescriptors = DescriptorAllocator();
    skyboxDescriptors.init(m_vulkanContext.device, 1024, frameSizes);
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        skyboxDescriptorSets[frame_i] = skyboxDescriptors.allocate(m_vulkanContext.device, skyboxDescriptorLayout);
    }

    VkPushConstantRange range = {};
    range.offset = 0;
    range.size = sizeof(PushConstantsSkybox);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineCI = VkInit::pipelineLayoutCreateInfo();
    pipelineCI.setLayoutCount = 1;
    pipelineCI.pSetLayouts = &skyboxDescriptorLayout;
    pipelineCI.pPushConstantRanges = &range;
    pipelineCI.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vulkanContext.device, &pipelineCI, nullptr, &skyboxPipelineLayout))

    VkShaderModule skyboxVertShader, skyboxFragShader;
    VK_CHECK(m_vulkanContext.createShaderModule("shaders/pbr/skybox.vert.spv", &skyboxVertShader))
    VK_CHECK(m_vulkanContext.createShaderModule("shaders/pbr/skybox.frag.spv", &skyboxFragShader))

    PipelineBuilder pipelineBuilder;
    pipelineBuilder
            .setLayout(skyboxPipelineLayout)
            .setShaders(skyboxVertShader, skyboxFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setMultisamplingNone()
            .disableBlending()
            .enableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
            .setColorAttachmentFormat(m_vulkanContext.drawImage.imageFormat)
            .setDepthAttachmentFormat(m_vulkanContext.depthImage.imageFormat);

    skyboxPipeline = pipelineBuilder.build(m_vulkanContext.device);

    vkDestroyShaderModule(m_vulkanContext.device, skyboxVertShader, nullptr);
    vkDestroyShaderModule(m_vulkanContext.device, skyboxFragShader, nullptr);
}
