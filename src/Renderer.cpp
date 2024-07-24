#include "Renderer.h"

#include <VulkanInit.h>
#include <VulkanPipeline.h>
#include <VulkanUtils.h>
#include <stack>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer() {
    setupVulkan();
}

Renderer::~Renderer() {
    terminateVulkan();
}

void Renderer::run() {
    while (!glfwWindowShouldClose(m_vk.window)) {
        glfwPollEvents();
        m_camera.update();

        vkWaitForFences(m_vk.device, 1, &m_vk.frames[currentFrame].renderFence, VK_TRUE, 1e9);
        VK_CHECK(vkResetFences(m_vk.device, 1, &m_vk.frames[currentFrame].renderFence))

        m_vk.frames[currentFrame].frameDescriptors.clearPools(m_vk.device);

        uint32_t imageIndex;
        VkResult swapchainRet = vkAcquireNextImageKHR(m_vk.device, m_vk.swapchain, 1e9,
                                                      m_vk.frames[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE,
                                                      &imageIndex);
        if (swapchainRet == VK_ERROR_OUT_OF_DATE_KHR) {
            m_vk.resizeWindow();
            continue;
        }

        VkCommandBuffer cmd = m_vk.frames[currentFrame].commandBuffer;

        VK_CHECK(vkResetCommandBuffer(cmd, 0))

        VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

        VkUtil::transitionImage(cmd, m_vk.drawImage.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkClearColorValue clearValue = {{0.2f, 0.2f, 0.2f, 1.0f}};

        VkImageSubresourceRange clearRange = VkInit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdClearColorImage(cmd, m_vk.drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

        VkUtil::transitionImage(cmd, m_vk.drawImage.image,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        drawGeometry(cmd);

        VkUtil::transitionImage(cmd, m_vk.drawImage.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VkUtil::transitionImage(cmd, m_vk.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkUtil::copyImageToImage(cmd, m_vk.drawImage.image, m_vk.swapchainImages[imageIndex], m_vk.windowExtent,
                                 m_vk.windowExtent);

        VkUtil::transitionImage(cmd, m_vk.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VK_CHECK(vkEndCommandBuffer(cmd))

        VkCommandBufferSubmitInfo cmdSubmitInfo = VkInit::commandBufferSubmitInfo(cmd);

        VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                     m_vk.frames[currentFrame].imageAvailableSemaphore);
        VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                                       m_vk.frames[currentFrame].renderSemaphore);

        VkSubmitInfo2 submit = VkInit::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

        VK_CHECK(vkQueueSubmit2(m_vk.graphicsQueue, 1, &submit, m_vk.frames[currentFrame].renderFence))

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &m_vk.swapchain.swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &m_vk.frames[currentFrame].renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRet = vkQueuePresentKHR(m_vk.presentQueue, &presentInfo);
        if (presentRet == VK_ERROR_OUT_OF_DATE_KHR) {
            m_vk.resizeWindow();
        }

        currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
    }
}

void Renderer::loadGltf(std::filesystem::path filePath) {
    Scene scene(&m_vk);
    scene.load(filePath);

    m_indexBuffer.insert(m_indexBuffer.end(), scene.indexBuffer.begin(), scene.indexBuffer.end());
    m_vertexBuffer.insert(m_vertexBuffer.end(), scene.vertexBuffer.begin(), scene.vertexBuffer.end());

    scenes.emplace_back(scene);

    createDrawDatas();
    createBuffers();
}

void Renderer::createBuffers() {
    const size_t vertexBufferSize = m_vertexBuffer.size() * sizeof(Vertex);
    const size_t indexBufferSize = m_indexBuffer.size() * sizeof(uint32_t);
    const size_t transformBufferSize = m_transformBuffer.size() * sizeof(glm::mat4);

    VulkanBuffer stagingBuffer = m_vk.createBuffer(vertexBufferSize + indexBufferSize + transformBufferSize,
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    void *data = stagingBuffer.info.pMappedData;

    m_vulkanVertexBuffer = m_vk.createBuffer(vertexBufferSize,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                             VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(data, m_vertexBuffer.data(), vertexBufferSize);

    if (indexBufferSize != 0) {
        m_vulkanIndexBuffer = m_vk.createBuffer(indexBufferSize,
                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
        memcpy(static_cast<uint8_t *>(data) + vertexBufferSize, m_indexBuffer.data(), indexBufferSize);
    }

    m_vulkanTransformBuffer = m_vk.createBuffer(transformBufferSize,
                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(static_cast<uint8_t *>(data) + vertexBufferSize + indexBufferSize, m_transformBuffer.data(),
           transformBufferSize);

    m_vk.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy = {0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_vulkanVertexBuffer.buffer,
                        1, &vertexCopy);

        if (indexBufferSize != 0) {
            VkBufferCopy indexCopy = {0};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_vulkanIndexBuffer.buffer,
                            1, &indexCopy);
        }

        VkBufferCopy transformCopy = {0};
        transformCopy.dstOffset = 0;
        transformCopy.srcOffset = vertexBufferSize + indexBufferSize;
        transformCopy.size = transformBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_vulkanTransformBuffer.buffer,
                        1, &transformCopy);
    });

    m_vk.destroyBuffer(stagingBuffer);
}

void Renderer::createDrawDatas() {
    for (const auto &scene: scenes) {
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
                        //todo: support multiple scenes
                        drawData.hasIndices = meshPrimitive.hasIndices;
                        drawData.indexOffset = meshPrimitive.indexStart;
                        drawData.vertexOffset = meshPrimitive.vertexStart;
                        drawData.indexCount = meshPrimitive.indexCount;
                        drawData.vertexCount = meshPrimitive.vertexCount;
                        drawData.transformOffset = m_transformBuffer.size();

                        drawDatas.emplace_back(drawData);
                    }
                    m_transformBuffer.emplace_back(currentNode->worldTransform);
                }

                for (const auto &child: currentNode->children) {
                    nodeStack.push(child);
                }
            }
        }
    }
}

void Renderer::setupVulkan() {
    m_vk.windowExtent = {1920, 1080};
    m_vk.init();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        };

        m_vk.frames[i].frameDescriptors = DescriptorAllocator();
        m_vk.frames[i].frameDescriptors.init(m_vk.device, 1000, frameSizes);
    }

    glfwSetWindowUserPointer(m_vk.window, &m_camera);
    glfwSetCursorPosCallback(m_vk.window, Camera::mouseCallback);
    glfwSetKeyCallback(m_vk.window, Camera::keyCallback);
    glfwSetMouseButtonCallback(m_vk.window, Camera::mouseButtonCallback);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;

    vkCreateSampler(m_vk.device, &samplerInfo, nullptr, &defaultSamplerLinear);

    DescriptorLayoutBuilder descriptorLayoutBuilder;
    descriptorLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorLayoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    singleImageDescriptorLayout = descriptorLayoutBuilder.build(m_vk.device,
                                                                VK_SHADER_STAGE_VERTEX_BIT |
                                                                VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPushConstantRange pushConstantsRange = {};
    pushConstantsRange.offset = 0;
    pushConstantsRange.size = sizeof(PushConstantsBindless);
    pushConstantsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &singleImageDescriptorLayout;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantsRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vk.device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout))

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(m_vk.createShaderModule("shaders/mesh_bindless.vert.spv", &triangleVertShader))
    VK_CHECK(m_vk.createShaderModule("shaders/textured_triangle.frag.spv", &triangleFragShader))

    PipelineBuilder trianglePipelineBuilder;
    trianglePipelineBuilder
            .setLayout(trianglePipelineLayout)
            .setShaders(triangleVertShader, triangleFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setMultisamplingNone()
            .disableBlending()
            // .disableDepthTest()
            .enableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
            .setColorAttachmentFormat(m_vk.drawImage.imageFormat)
            .setDepthAttachmentFormat(m_vk.depthImage.imageFormat);

    trianglePipeline = trianglePipelineBuilder.build(m_vk.device);

    vkDestroyShaderModule(m_vk.device, triangleVertShader, nullptr);
    vkDestroyShaderModule(m_vk.device, triangleFragShader, nullptr);
}

void Renderer::terminateVulkan() {
    vkDeviceWaitIdle(m_vk.device);

    for (auto &scene: scenes) {
        scene.clear();
    }

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        m_vk.frames[i].frameDescriptors.destroyPools(m_vk.device);
    }

    if (!m_indexBuffer.empty()) {
        m_vk.destroyBuffer(m_vulkanIndexBuffer);
    }
    m_vk.destroyBuffer(m_vulkanVertexBuffer);
    m_vk.destroyBuffer(m_vulkanTransformBuffer);

    vkDestroyDescriptorSetLayout(m_vk.device, singleImageDescriptorLayout, nullptr);

    vkDestroySampler(m_vk.device, defaultSamplerLinear, nullptr);

    vkDestroyPipelineLayout(m_vk.device, trianglePipelineLayout, nullptr);
    vkDestroyPipeline(m_vk.device, trianglePipeline, nullptr);

    m_vk.terminate();
}

void Renderer::drawGeometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_vk.drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_vk.depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 1.f;

    VkRenderingInfo renderInfo = VkInit::renderingInfo(m_vk.windowExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = m_vk.windowExtent.width;
    viewport.height = m_vk.windowExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_vk.windowExtent.width;
    scissor.extent.height = m_vk.windowExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), viewport.width / viewport.height, 0.1f, 1e9f);
    projection[1][1] *= -1; // flip y

    m_globalUniformData.view = view;
    m_globalUniformData.proj = projection;
    m_globalUniformData.projView = projection * view;

    // uniform buffer for scene data
    VulkanBuffer globalUniformBuffer = m_vk.createBuffer(sizeof(GlobalUniformData),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    GlobalUniformData *globalUniformData = static_cast<GlobalUniformData *>(globalUniformBuffer.info.pMappedData);
    *globalUniformData = m_globalUniformData;

    // bind texture
    VkDescriptorSet imageSet = m_vk.frames[currentFrame].frameDescriptors
            .allocate(m_vk.device, singleImageDescriptorLayout); {
        DescriptorWriter writer;
        writer.writeImage(0, m_vk.defaultTextureImage.imageView, defaultSamplerLinear,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.writeBuffer(1, globalUniformBuffer.buffer, sizeof(GlobalUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // writer.writeImage(0, sc.value().images[0].value().imageView, vkState.defaultSamplerLinear,
        //                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.updateSet(m_vk.device, imageSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipelineLayout,
                            0, 1, &imageSet, 0, nullptr);
    vkCmdBindIndexBuffer(cmd, m_vulkanIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    PushConstantsBindless pcb = {};
    pcb.vertexBuffer = m_vk.getBufferAddress(m_vulkanVertexBuffer);
    pcb.transformBuffer = m_vk.getBufferAddress(m_vulkanTransformBuffer);

    for (const auto &drawData: drawDatas) {
        pcb.transformOffset = drawData.transformOffset;

        vkCmdPushConstants(cmd, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstantsBindless),
                           &pcb);
        if (drawData.hasIndices) {
            vkCmdDrawIndexed(cmd, drawData.indexCount, 1, drawData.indexOffset, 0, 1);
        } else {
            vkCmdDraw(cmd, drawData.vertexCount, 1, drawData.vertexOffset, 0);
        }
    }

    vkCmdEndRendering(cmd);

    m_vk.destroyBuffer(globalUniformBuffer);
}
