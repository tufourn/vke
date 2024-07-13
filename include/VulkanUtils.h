#pragma once

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include "VulkanTypes.h"

#include <iostream>
#include <cassert>
#include <span>

#define VK_CHECK(func) \
{ \
    const VkResult vkCheckRet = func; \
    if (vkCheckRet != VK_SUCCESS) { \
        std::cerr << "Error calling function " << #func \
        << " at " << __FILE__ << ":" \
        << __LINE__ << ". Result is " \
        << string_VkResult(vkCheckRet) \
        << std::endl; \
        assert(false); \
    } \
}

namespace VkUtil {

    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                         VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

    void copyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);
}