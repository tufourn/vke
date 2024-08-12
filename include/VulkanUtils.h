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

    // packed color values
    constexpr uint32_t opaqueBlack = 0xFF000000;
    constexpr uint32_t opaqueWhite = 0xFFFFFFFF;
    constexpr uint32_t opaqueCyan = 0xFFFFFF00;
    constexpr uint32_t opaqueMagenta = 0xFFFF00FF;

    // <0.5, 0.5, 1.0> when mapped to [-1, 1] becomes <0.0, 0.0, 1.0> which doesn't perturb normals
    constexpr uint32_t defaultNormalMapColor = 0xFFFF8080;

    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                         VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

    void copyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);

    void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
    void generateCubeMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, uint32_t layerCount);

    std::array<uint32_t, 8 * 8> createCheckerboard(uint32_t color1 = opaqueBlack, uint32_t color2 = opaqueMagenta);
}