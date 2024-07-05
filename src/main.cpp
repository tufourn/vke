#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"

void drawGeometry(VulkanContext &vk, VkCommandBuffer cmd);

struct VulkanState {
    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

} vkState;

int main() {

    VulkanContext vk = {};
    vk.windowExtent = {1920, 1080};

    vk.init();

    VkShaderModule triangleVertShader, triangleFragShader;
    VK_CHECK(vk.createShaderModule("colored_triangle.vert.spv", &triangleVertShader))
    VK_CHECK(vk.createShaderModule("colored_triangle.frag.spv", &triangleFragShader))

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
    VK_CHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &vkState.trianglePipelineLayout));

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

    uint32_t currentFrame = 0;

    while (!glfwWindowShouldClose(vk.window)) {
        glfwPollEvents();

        vkWaitForFences(vk.device, 1, &vk.frames[currentFrame].renderFence, VK_TRUE, 1e9);
        VK_CHECK(vkResetFences(vk.device, 1, &vk.frames[currentFrame].renderFence))

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

        drawGeometry(vk, cmd);

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

    vkDeviceWaitIdle(vk.device);
    vkDestroyPipelineLayout(vk.device, vkState.trianglePipelineLayout, nullptr);
    vkDestroyPipeline(vk.device, vkState.trianglePipeline, nullptr);
    vk.terminate();
}

void drawGeometry(VulkanContext &vk, VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = vk.drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = VkInit::renderingInfo(vk.windowExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.trianglePipeline);

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
