#include "Renderer.h"

#include <VulkanInit.h>
#include <VulkanPipeline.h>
#include <VulkanUtils.h>
#include <stack>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr uint32_t MAX_TEXTURES = 1024;

static constexpr uint32_t UNIFORM_BINDING = 0;
static constexpr uint32_t TEXTURE_BINDING = 1;

Renderer::Renderer() {
    setupVulkan();
}

Renderer::~Renderer() {
    terminateVulkan();
}

void Renderer::run() {
    while (!glfwWindowShouldClose(vulkanContext.window)) {
        glfwPollEvents();
        m_camera.update();

        vkWaitForFences(vulkanContext.device, 1, &vulkanContext.frames[currentFrame].renderFence, VK_TRUE, 1e9);
        VK_CHECK(vkResetFences(vulkanContext.device, 1, &vulkanContext.frames[currentFrame].renderFence))

        uint32_t imageIndex;
        VkResult swapchainRet = vkAcquireNextImageKHR(vulkanContext.device, vulkanContext.swapchain, 1e9,
                                                      vulkanContext.frames[currentFrame].imageAvailableSemaphore,
                                                      VK_NULL_HANDLE,
                                                      &imageIndex);
        if (swapchainRet == VK_ERROR_OUT_OF_DATE_KHR) {
            vulkanContext.resizeWindow();
            continue;
        }

        VkCommandBuffer cmd = vulkanContext.frames[currentFrame].commandBuffer;

        VK_CHECK(vkResetCommandBuffer(cmd, 0))

        VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

        VkUtil::transitionImage(cmd, vulkanContext.drawImage.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkClearColorValue clearValue = {{0.2f, 0.2f, 0.2f, 1.0f}};

        VkImageSubresourceRange clearRange = VkInit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdClearColorImage(cmd, vulkanContext.drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

        VkUtil::transitionImage(cmd, vulkanContext.drawImage.image,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        drawGeometry(cmd);

        VkUtil::transitionImage(cmd, vulkanContext.drawImage.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VkUtil::transitionImage(cmd, vulkanContext.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkUtil::copyImageToImage(cmd, vulkanContext.drawImage.image, vulkanContext.swapchainImages[imageIndex],
                                 vulkanContext.windowExtent,
                                 vulkanContext.windowExtent);

        VkUtil::transitionImage(cmd, vulkanContext.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VK_CHECK(vkEndCommandBuffer(cmd))

        VkCommandBufferSubmitInfo cmdSubmitInfo = VkInit::commandBufferSubmitInfo(cmd);

        VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                     vulkanContext.frames[currentFrame].imageAvailableSemaphore);
        VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                                       vulkanContext.frames[currentFrame].renderSemaphore);

        VkSubmitInfo2 submit = VkInit::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

        VK_CHECK(
                vkQueueSubmit2(vulkanContext.graphicsQueue, 1, &submit, vulkanContext.frames[currentFrame].renderFence))

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &vulkanContext.swapchain.swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &vulkanContext.frames[currentFrame].renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRet = vkQueuePresentKHR(vulkanContext.presentQueue, &presentInfo);
        if (presentRet == VK_ERROR_OUT_OF_DATE_KHR) {
            vulkanContext.resizeWindow();
        }

        currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
    }
}

void Renderer::loadGltf(std::filesystem::path filePath) {
    Scene scene(this);
    scene.load(filePath);

    if (!scene.loaded) {
        return;
    }

    SceneData sceneData = {};

    sceneData.vertexOffset = m_vertices.size();
    m_vertices.insert(m_vertices.end(), scene.vertexBuffer.begin(), scene.vertexBuffer.end());

    sceneData.indexOffset = m_indices.size();
    m_indices.insert(m_indices.end(), scene.indexBuffer.begin(), scene.indexBuffer.end());

    // make sure index points to the right vertex
    for (size_t index_i = sceneData.indexOffset; index_i < m_indices.size(); index_i++) {
        m_indices[index_i] += sceneData.vertexOffset;
    }

    sceneData.textureOffset = m_textures.size();
    m_textures.insert(m_textures.end(), scene.textures.begin(), scene.textures.end());

    sceneData.materialOffset = m_materials.size();
    m_materials.insert(m_materials.end(), scene.materials.begin(), scene.materials.end());

    // make sure material points to the right texture
    for (size_t material_i = sceneData.materialOffset; material_i < m_materials.size(); material_i++) {
        if (m_materials[material_i].baseTextureOffset == OPAQUE_WHITE_TEXTURE) {
            m_materials[material_i].baseTextureOffset = 0; // opaque white texture at index 0
        } else {
            m_materials[material_i].baseTextureOffset += sceneData.textureOffset;
        }
    }

    for (const auto &topLevelNode: scene.topLevelNodes) {
        std::stack<std::shared_ptr<Node> > nodeStack;
        nodeStack.push(topLevelNode);

        while (!nodeStack.empty()) {
            auto currentNode = nodeStack.top();
            nodeStack.pop();

            if (auto parentNode = currentNode->parent.lock()) {
                currentNode->worldTransform = parentNode->worldTransform * currentNode->localTransform;
            } else {
                currentNode->worldTransform = currentNode->localTransform;
            }

            if (const auto &nodeMesh = currentNode->mesh) {
                for (const auto &meshPrimitive: nodeMesh->meshPrimitives) {
                    DrawData drawData = {};
                    drawData.hasIndices = meshPrimitive.hasIndices;
                    drawData.indexOffset = meshPrimitive.indexStart + sceneData.indexOffset;
                    drawData.vertexOffset = meshPrimitive.vertexStart + sceneData.vertexOffset;
                    drawData.indexCount = meshPrimitive.indexCount;
                    drawData.vertexCount = meshPrimitive.vertexCount;
                    if (meshPrimitive.materialOffset == DEFAULT_MATERIAL) {
                        drawData.materialOffset = 0; // default material at index 0
                    } else {
                        drawData.materialOffset = meshPrimitive.materialOffset + sceneData.materialOffset;
                    }
                    drawData.transformOffset = m_transforms.size();

                    sceneData.drawDatas.emplace_back(drawData);
                }
                m_transforms.emplace_back(currentNode->worldTransform);
            }

            for (const auto &child: currentNode->children) {
                nodeStack.push(child);
            }
        }
    }

    DescriptorWriter writer;
    for (size_t texture_i = sceneData.textureOffset; texture_i < m_textures.size(); texture_i++) {
        writer.writeImage(TEXTURE_BINDING, m_textures[texture_i]->imageview, m_textures[texture_i]->sampler,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          texture_i);
    }
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        writer.updateSet(vulkanContext.device, bindlessDescriptorSets[frame_i]);
    }

    m_scenes.emplace_back(scene, sceneData);

    destroyStaticBuffers();
    createStaticBuffers();
}

void Renderer::createStaticBuffers() {
    const size_t vertexBufferSize = m_vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = m_indices.size() * sizeof(uint32_t);
    const size_t transformBufferSize = m_transforms.size() * sizeof(glm::mat4);
    const size_t materialBufferSize = m_materials.size() * sizeof(Material);

    const size_t stagingBufferSize = vertexBufferSize + indexBufferSize + transformBufferSize + materialBufferSize;

    VulkanBuffer stagingBuffer = vulkanContext.createBuffer(stagingBufferSize,
                                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                            VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    void *data = stagingBuffer.info.pMappedData;

    m_boundedVertexBuffer = vulkanContext.createBuffer(vertexBufferSize,
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                       VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(data, m_vertices.data(), vertexBufferSize);

    if (indexBufferSize != 0) {
        m_boundedIndexBuffer = vulkanContext.createBuffer(indexBufferSize,
                                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
        memcpy(static_cast<uint8_t *>(data) + vertexBufferSize, m_indices.data(), indexBufferSize);
    }

    m_boundedTransformBuffer = vulkanContext.createBuffer(transformBufferSize,
                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(static_cast<uint8_t *>(data) + vertexBufferSize + indexBufferSize, m_transforms.data(),
           transformBufferSize);

    m_boundedMaterialBuffer = vulkanContext.createBuffer(materialBufferSize,
                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                         VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(static_cast<uint8_t *>(data) + vertexBufferSize + indexBufferSize + transformBufferSize, m_materials.data(),
           materialBufferSize);

    vulkanContext.immediateSubmit([&](VkCommandBuffer cmd) {
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

        VkBufferCopy transformCopy = {0};
        transformCopy.dstOffset = 0;
        transformCopy.srcOffset = vertexBufferSize + indexBufferSize;
        transformCopy.size = transformBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_boundedTransformBuffer.buffer,
                        1, &transformCopy);

        VkBufferCopy materialCopy = {0};
        materialCopy.dstOffset = 0;
        materialCopy.srcOffset = vertexBufferSize + indexBufferSize + transformBufferSize;
        materialCopy.size = materialBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_boundedMaterialBuffer.buffer,
                        1, &materialCopy);
    });

    vulkanContext.destroyBuffer(stagingBuffer);
}

void Renderer::setupVulkan() {
    vulkanContext.windowExtent = {1920, 1080};
    vulkanContext.init();

    glfwSetWindowUserPointer(vulkanContext.window, &m_camera);
    glfwSetCursorPosCallback(vulkanContext.window, Camera::mouseCallback);
    glfwSetKeyCallback(vulkanContext.window, Camera::keyCallback);
    glfwSetMouseButtonCallback(vulkanContext.window, Camera::mouseButtonCallback);

    DescriptorLayoutBuilder descriptorLayoutBuilder;
    descriptorLayoutBuilder.addBinding(UNIFORM_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptorLayoutBuilder.addBinding(TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    std::array<VkDescriptorBindingFlags, 2> flagArray = {
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

    globalDescriptorLayout = descriptorLayoutBuilder.build(vulkanContext.device,
                                                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           &bindFlags,
                                                           VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

    std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
    };

    globalDescriptors = DescriptorAllocator();
    globalDescriptors.init(vulkanContext.device, 1024, frameSizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        bindlessDescriptorSets[frame_i] = globalDescriptors.allocate(vulkanContext.device, globalDescriptorLayout);
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

    VK_CHECK(vkCreatePipelineLayout(vulkanContext.device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout))

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(vulkanContext.createShaderModule("shaders/mesh_bindless.vert.spv", &triangleVertShader))
    VK_CHECK(vulkanContext.createShaderModule("shaders/texture_bindless.frag.spv", &triangleFragShader))

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
            .setColorAttachmentFormat(vulkanContext.drawImage.imageFormat)
            .setDepthAttachmentFormat(vulkanContext.depthImage.imageFormat);

    trianglePipeline = trianglePipelineBuilder.build(vulkanContext.device);

    vkDestroyShaderModule(vulkanContext.device, triangleVertShader, nullptr);
    vkDestroyShaderModule(vulkanContext.device, triangleFragShader, nullptr);

    m_uniformBuffer = vulkanContext.createBuffer(sizeof(GlobalUniformData),
                                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        m_boundedUniformBuffers[frame_i] = vulkanContext.createBuffer(sizeof(GlobalUniformData),
                                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }

    initDefaultData();
}

void Renderer::terminateVulkan() {
    vkDeviceWaitIdle(vulkanContext.device);

    for (auto &scene: m_scenes) {
        scene.first.clear();
    }

    vulkanContext.destroyImage(errorTextureImage);
    vulkanContext.destroyImage(opaqueWhiteTextureImage);
    vkDestroySampler(vulkanContext.device, defaultSampler, nullptr);

    globalDescriptors.destroyPools(vulkanContext.device);

    destroyStaticBuffers();

    vulkanContext.destroyBuffer(m_uniformBuffer);

    for (auto &boundedUniformBuffer: m_boundedUniformBuffers) {
        vulkanContext.destroyBuffer(boundedUniformBuffer);
    }

    vkDestroyDescriptorSetLayout(vulkanContext.device, globalDescriptorLayout, nullptr);

    vkDestroyPipelineLayout(vulkanContext.device, trianglePipelineLayout, nullptr);
    vkDestroyPipeline(vulkanContext.device, trianglePipeline, nullptr);

    vulkanContext.terminate();
}

void Renderer::drawGeometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(vulkanContext.drawImage.imageView, nullptr);
    VkRenderingAttachmentInfo depthAttachment = VkInit::depthAttachmentInfo(vulkanContext.depthImage.imageView);

    VkViewport viewport = VkInit::viewport(vulkanContext.windowExtent.width, vulkanContext.windowExtent.height);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = vulkanContext.windowExtent.width;
    scissor.extent.height = vulkanContext.windowExtent.height;

    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), viewport.width / viewport.height, 0.1f, 1e9f);
    projection[1][1] *= -1; // flip y

    m_globalUniformData.view = view;
    m_globalUniformData.proj = projection;
    m_globalUniformData.projView = projection * view;

    GlobalUniformData *globalUniformData = static_cast<GlobalUniformData *>(m_uniformBuffer.info.pMappedData);
    *globalUniformData = m_globalUniformData;

    VkBufferCopy uniformCopy = {0};
    uniformCopy.dstOffset = 0;
    uniformCopy.srcOffset = 0;
    uniformCopy.size = sizeof(GlobalUniformData);
    vkCmdCopyBuffer(cmd, m_uniformBuffer.buffer, m_boundedUniformBuffers[currentFrame].buffer,
                    1, &uniformCopy);

    DescriptorWriter writer;
    writer.writeBuffer(0, m_boundedUniformBuffers[currentFrame].buffer, sizeof(GlobalUniformData), 0,
                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.updateSet(vulkanContext.device, bindlessDescriptorSets[currentFrame]);

    VkRenderingInfo renderInfo = VkInit::renderingInfo(vulkanContext.windowExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    auto start = std::chrono::system_clock::now();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipelineLayout,
                            0, 1, &bindlessDescriptorSets[currentFrame], 0, nullptr);
    if (!m_indices.empty()) {
        vkCmdBindIndexBuffer(cmd, m_boundedIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    PushConstantsBindless pcb = {};
    pcb.vertexBuffer = vulkanContext.getBufferAddress(m_boundedVertexBuffer);
    pcb.transformBuffer = vulkanContext.getBufferAddress(m_boundedTransformBuffer);
    pcb.materialBuffer = vulkanContext.getBufferAddress(m_boundedMaterialBuffer);

    for (const auto &scene: m_scenes) {
        for (const auto &drawData: scene.second.drawDatas) {
            pcb.transformOffset = drawData.transformOffset;
            pcb.materialOffset = drawData.materialOffset;

            vkCmdPushConstants(cmd, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(PushConstantsBindless),
                               &pcb);
            if (drawData.hasIndices) {
                vkCmdDrawIndexed(cmd, drawData.indexCount, 1, drawData.indexOffset, 0, 1);
            } else {
                vkCmdDraw(cmd, drawData.vertexCount, 1, drawData.vertexOffset, 0);
            }
        }
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "draw time: " << elapsed.count() / 1000.f << std::endl;

    vkCmdEndRendering(cmd);
}

void Renderer::initDefaultData() {
    m_textures.clear();
    m_materials.clear();

    uint32_t opaqueWhite = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
    uint32_t black = glm::packUnorm4x8(glm::vec4(0.f, 0.f, 0.f, 1.f));
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.f, 0.f, 1.f, 1.f));

    std::array<uint32_t, 8 * 8> checker = {};
    for (size_t x = 0; x < 8; x++) {
        for (size_t y = 0; y < 8; y++) {
            checker[y * 8 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    errorTextureImage = vulkanContext.createImage(checker.data(), VkExtent3D(8, 8, 1),
                                                  VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_IMAGE_USAGE_SAMPLED_BIT, false);

    opaqueWhiteTextureImage = vulkanContext.createImage(&opaqueWhite, VkExtent3D(1, 1, 1),
                                                        VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_IMAGE_USAGE_SAMPLED_BIT, false);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.magFilter = VK_FILTER_NEAREST;

    vkCreateSampler(vulkanContext.device, &samplerInfo, nullptr, &defaultSampler);

    Texture opaqueWhiteTexture = {
            "opaque_white_texture",
            opaqueWhiteTextureImage.imageView,
            defaultSampler
    };

    Texture errorTexture = {
            "error_missing_texture",
            errorTextureImage.imageView,
            defaultSampler
    };

    m_textures.emplace_back(std::make_shared<Texture>(opaqueWhiteTexture));
    m_textures.emplace_back(std::make_shared<Texture>(errorTexture));

    Material defaultMaterial = {
            .baseColorFactor = {1.f, 1.f, 1.f, 1.f},
            .metallicFactor = 1.f,
            .roughnessFactor = 1.f,
            .baseTextureOffset = 0,  // the default opaque white texture is at index 0
    };

    m_materials.emplace_back(defaultMaterial);

    DescriptorWriter writer;
    for (size_t texture_i = 0; texture_i < m_textures.size(); texture_i++) {
        writer.writeImage(TEXTURE_BINDING, m_textures[texture_i]->imageview, m_textures[texture_i]->sampler,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          texture_i);
    }
    for (size_t frame_i = 0; frame_i < MAX_CONCURRENT_FRAMES; frame_i++) {
        writer.updateSet(vulkanContext.device, bindlessDescriptorSets[frame_i]);
    }
}

void Renderer::destroyStaticBuffers() {
    if (m_boundedIndexBuffer.buffer != VK_NULL_HANDLE) {
        vulkanContext.destroyBuffer(m_boundedIndexBuffer);
    }
    if (m_boundedVertexBuffer.buffer != VK_NULL_HANDLE) {
        vulkanContext.destroyBuffer(m_boundedVertexBuffer);
    }
    if (m_boundedTransformBuffer.buffer != VK_NULL_HANDLE) {
        vulkanContext.destroyBuffer(m_boundedTransformBuffer);
    }
    if (m_boundedMaterialBuffer.buffer != VK_NULL_HANDLE) {
        vulkanContext.destroyBuffer(m_boundedMaterialBuffer);
    }
}

