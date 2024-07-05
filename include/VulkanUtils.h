#pragma once

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <iostream>

#define VK_CHECK(func) \
{ \
    const VkResult result = func; \
    if (result != VK_SUCCESS) { \
        std::cerr << "Error calling function " << #func \
        << " at " << __FILE__ << ":" \
        << __LINE__ << ". Result is " \
        << string_VkResult(result) \
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