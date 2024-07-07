#pragma once

#include <volk.h>

namespace VkInit {

    VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

    VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

    VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);

    VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);

    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);

    VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usage, VkExtent3D extent);

    VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspect);

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                  VkShaderModule shaderModule,
                                                                  const char *entry = "main");

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo *colorAttachment,
                                  VkRenderingAttachmentInfo *depthAttachment);

    VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo *cmd,
                             VkSemaphoreSubmitInfo *signalSemaphoreInfo,
                             VkSemaphoreSubmitInfo *waitSemaphoreInfo);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count);

}