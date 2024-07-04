#pragma once

#include <volk.h>

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
