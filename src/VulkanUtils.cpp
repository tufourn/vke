#include <array>
#include "VulkanUtils.h"
#include "VulkanInit.h"

namespace VkUtil {

    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                         VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask) {

        VkImageAspectFlags aspectMask =
                newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                      : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageSubresourceRange subresourceRange = VkInit::imageSubresourceRange(aspectMask);

        VkImageMemoryBarrier2 imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = srcStageMask;
        imageBarrier.srcAccessMask = srcAccessMask;
        imageBarrier.dstStageMask = dstStageMask;
        imageBarrier.dstAccessMask = dstAccessMask;

        imageBarrier.oldLayout = oldLayout;
        imageBarrier.newLayout = newLayout;

        imageBarrier.subresourceRange = subresourceRange;
        imageBarrier.image = image;

        VkDependencyInfo depInfo = {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    void copyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize) {
        VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

        blitRegion.srcOffsets[1].x = static_cast<int>(srcSize.width);
        blitRegion.srcOffsets[1].y = static_cast<int>(srcSize.height);
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = static_cast<int>(dstSize.width);
        blitRegion.dstOffsets[1].y = static_cast<int>(dstSize.height);
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
        blitInfo.dstImage = dst;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = src;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_LINEAR;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize) {
        int mipLevels = int(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

        for (size_t mip_i = 0; mip_i < mipLevels; mip_i++) {
            VkExtent2D halfSize = imageSize;
            halfSize.width /= 2;
            halfSize.height /= 2;

            VkImageMemoryBarrier2 imageBarrier = {};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange = VkInit::imageSubresourceRange(aspectMask);
            imageBarrier.subresourceRange.levelCount = 1;
            imageBarrier.subresourceRange.baseMipLevel = mip_i;
            imageBarrier.image = image;

            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &imageBarrier;

            vkCmdPipelineBarrier2(cmd, &dependencyInfo);

            if (mip_i < mipLevels - 1) {
                VkImageBlit2 blitRegion = {};
                blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

                blitRegion.srcOffsets[1].x = imageSize.width;
                blitRegion.srcOffsets[1].y = imageSize.height;
                blitRegion.srcOffsets[1].z = 1;

                blitRegion.dstOffsets[1].x = halfSize.width;
                blitRegion.dstOffsets[1].y = halfSize.height;
                blitRegion.dstOffsets[1].z = 1;

                blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcSubresource.mipLevel = mip_i;

                blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstSubresource.mipLevel = mip_i + 1;

                VkBlitImageInfo2 blitInfo = {};
                blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
                blitInfo.dstImage = image;
                blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                blitInfo.srcImage = image;
                blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                blitInfo.filter = VK_FILTER_LINEAR;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;

                vkCmdBlitImage2(cmd, &blitInfo);

                imageSize = halfSize;
            }
        }

        transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT);
    }

    std::array<uint32_t, 8 * 8> createCheckerboard(uint32_t color1, uint32_t color2) {
        std::array<uint32_t, 8 * 8> checker = {};
        for (size_t x = 0; x < 8; x++) {
            for (size_t y = 0; y < 8; y++) {
                checker[y * 8 + x] = ((x % 2) ^ (y % 2)) ? color1 : color2;
            }
        }

        return checker;
    }
}