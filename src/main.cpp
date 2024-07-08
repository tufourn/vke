#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"

#include <glm/glm.hpp>

VulkanContext vk = {};

struct VulkanState {
    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

    VulkanImage whiteImage;
    VkSampler defaultSamplerLinear;

    VkDescriptorSetLayout singleImageDescriptorLayout;

} vkState;

uint32_t currentFrame = 0;

void drawGeometry(VkCommandBuffer cmd);

void setupVulkan();

void terminateVulkan();

int main() {
    setupVulkan();

    while (!glfwWindowShouldClose(vk.window)) {
        glfwPollEvents();

        vkWaitForFences(vk.device, 1, &vk.frames[currentFrame].renderFence, VK_TRUE, 1e9);
        VK_CHECK(vkResetFences(vk.device, 1, &vk.frames[currentFrame].renderFence))

        vk.frames[currentFrame].frameDescriptors.clearPools(vk.device);

        uint32_t imageIndex;
        VkResult swapchainRet = vkAcquireNextImageKHR(vk.device, vk.swapchain, 1e9,
                                                      vk.frames[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE,
                                                      &imageIndex);
        if (swapchainRet == VK_ERROR_OUT_OF_DATE_KHR) {
            vk.resizeWindow();
            continue;
        }

        VkCommandBuffer cmd = vk.frames[currentFrame].commandBuffer;

        VK_CHECK(vkResetCommandBuffer(cmd, 0))

        VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

        VkUtil::transitionImage(cmd, vk.drawImage.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkClearColorValue clearValue = {{0.0f, 0.0f, 1.0f, 1.0f}};

        VkImageSubresourceRange clearRange = VkInit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdClearColorImage(cmd, vk.drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

        VkUtil::transitionImage(cmd, vk.drawImage.image,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        drawGeometry(cmd);

        VkUtil::transitionImage(cmd, vk.drawImage.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VkUtil::transitionImage(cmd, vk.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);

        VkUtil::copyImageToImage(cmd, vk.drawImage.image, vk.swapchainImages[imageIndex], vk.windowExtent,
                                 vk.windowExtent);

        VkUtil::transitionImage(cmd, vk.swapchainImages[imageIndex],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

        VK_CHECK(vkEndCommandBuffer(cmd))

        VkCommandBufferSubmitInfo cmdSubmitInfo = VkInit::commandBufferSubmitInfo(cmd);

        VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                     vk.frames[currentFrame].imageAvailableSemaphore);
        VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                                       vk.frames[currentFrame].renderSemaphore);

        VkSubmitInfo2 submit = VkInit::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

        VK_CHECK(vkQueueSubmit2(vk.graphicsQueue, 1, &submit, vk.frames[currentFrame].renderFence))

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &vk.swapchain.swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &vk.frames[currentFrame].renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRet = vkQueuePresentKHR(vk.presentQueue, &presentInfo);
        if (presentRet == VK_ERROR_OUT_OF_DATE_KHR) {
            vk.resizeWindow();
        }

        currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
    }

    terminateVulkan();
}

void drawGeometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = vk.drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = VkInit::renderingInfo(vk.windowExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.trianglePipeline);

    // bind texture
    VkDescriptorSet imageSet = vk.frames[currentFrame].frameDescriptors
            .allocate(vk.device, vkState.singleImageDescriptorLayout);

    {
        DescriptorWriter writer;
        writer.writeImage(0, vkState.whiteImage.imageView, vkState.defaultSamplerLinear,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.updateSet(vk.device, imageSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.trianglePipelineLayout,
                            0, 1, &imageSet, 0, nullptr);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = vk.windowExtent.width;
    viewport.height = vk.windowExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = vk.windowExtent.width;
    scissor.extent.height = vk.windowExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    //launch a draw command to draw 3 vertices
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

void setupVulkan() {
    vk.windowExtent = {1920, 1080};
    vk.init();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        vk.frames[i].frameDescriptors = DescriptorAllocator();
        vk.frames[i].frameDescriptors.init(vk.device, 1000, frameSizes);
    }

    uint32_t white = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
    vkState.whiteImage = vk.createImage(reinterpret_cast<void *>(&white), {1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_SAMPLED_BIT, false);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;

    vkCreateSampler(vk.device, &samplerInfo, nullptr, &vkState.defaultSamplerLinear);

    {
        DescriptorLayoutBuilder descriptorLayoutBuilder;
        descriptorLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        vkState.singleImageDescriptorLayout = descriptorLayoutBuilder.build(vk.device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vkState.singleImageDescriptorLayout;
    VK_CHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &vkState.trianglePipelineLayout))

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(vk.createShaderModule("textured_triangle.vert.spv", &triangleVertShader))
    VK_CHECK(vk.createShaderModule("textured_triangle.frag.spv", &triangleFragShader))

    PipelineBuilder trianglePipelineBuilder;
    trianglePipelineBuilder
            .setLayout(vkState.trianglePipelineLayout)
            .setShaders(triangleVertShader, triangleFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setMultisamplingNone()
            .disableBlending()
            .disableDepthTest()
            .setColorAttachmentFormat(vk.drawImage.imageFormat)
            .setDepthAttachmentFormat(VK_FORMAT_UNDEFINED);

    vkState.trianglePipeline = trianglePipelineBuilder.build(vk.device);

    vkDestroyShaderModule(vk.device, triangleVertShader, nullptr);
    vkDestroyShaderModule(vk.device, triangleFragShader, nullptr);
}

void terminateVulkan() {
    vkDeviceWaitIdle(vk.device);

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        vk.frames[i].frameDescriptors.destroyPools(vk.device);
    }
    vkDestroyDescriptorSetLayout(vk.device, vkState.singleImageDescriptorLayout, nullptr);

    vk.destroyImage(vkState.whiteImage);
    vkDestroySampler(vk.device, vkState.defaultSamplerLinear, nullptr);

    vkDestroyPipelineLayout(vk.device, vkState.trianglePipelineLayout, nullptr);
    vkDestroyPipeline(vk.device, vkState.trianglePipeline, nullptr);
    vk.terminate();
}
