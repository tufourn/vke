#include "Renderer.h"

#include <VulkanInit.h>
#include <VulkanPipeline.h>
#include <VulkanUtils.h>

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
}

void Renderer::setupVulkan() {
    m_vk.windowExtent = {1920, 1080};
    m_vk.init();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
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
    singleImageDescriptorLayout = descriptorLayoutBuilder.build(m_vk.device, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPushConstantRange pushConstantsRange = {};
    pushConstantsRange.offset = 0;
    pushConstantsRange.size = sizeof(PushConstants);
    pushConstantsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &singleImageDescriptorLayout;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantsRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vk.device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout))

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(m_vk.createShaderModule("shaders/mesh_triangle.vert.spv", &triangleVertShader))
    VK_CHECK(m_vk.createShaderModule("shaders/textured_triangle.frag.spv", &triangleFragShader))

    PipelineBuilder trianglePipelineBuilder;
    trianglePipelineBuilder
            .setLayout(trianglePipelineLayout)
            .setShaders(triangleVertShader, triangleFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
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

    for (auto& scene : scenes) {
        scene.clear();
    }

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        m_vk.frames[i].frameDescriptors.destroyPools(m_vk.device);
    }

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

     // bind texture
     VkDescriptorSet imageSet = m_vk.frames[currentFrame].frameDescriptors
             .allocate(m_vk.device, singleImageDescriptorLayout); {
         DescriptorWriter writer;
         writer.writeImage(0, m_vk.defaultTextureImage.imageView, defaultSamplerLinear,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
         // writer.writeImage(0, sc.value().images[0].value().imageView, vkState.defaultSamplerLinear,
         //                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
         writer.updateSet(m_vk.device, imageSet);
     }

     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipelineLayout,
                             0, 1, &imageSet, 0, nullptr);

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

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), viewport.width / viewport.height, 0.1f, 1e9f);
    projection[1][1] *= -1; // flip y

    DrawContext ctx;
    ctx.indexBuffer = scenes[0].buffers.indexBuffer.buffer;
    ctx.vertexBufferAddress = scenes[0].buffers.vertexBufferAddress;

    scenes[0].draw(glm::mat4(1.f), ctx);

    PushConstants pc;

    for (auto &renderObject: ctx.renderObjects) {
        pc.worldMatrix = projection * m_camera.getViewMatrix() * renderObject.transform;
        pc.vertexBuffer = ctx.vertexBufferAddress;
        vkCmdPushConstants(cmd, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstants),
                           &pc);
        if (renderObject.hasIndices) {
            vkCmdBindIndexBuffer(cmd, ctx.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, renderObject.indexCount, 1, renderObject.indexStart, 0, 1);
        } else {
            vkCmdDraw(cmd, renderObject.vertexCount, 1, renderObject.vertexStart, 0);
        }
    }

    vkCmdEndRendering(cmd);
}
