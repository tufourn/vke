#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "VulkanUtils.h"
#include "Utils.h"

void initVulkan(VulkanContext &vk, const VulkanFeatures &ctxFeatures) {
    initVulkanInstance(vk);
    initWindow(vk);
    initVulkanDevice(vk, ctxFeatures);
    initVmaAllocator(vk);
    initSwapchain(vk);
    initCommands(vk);
    initSyncStructures(vk);
}

void initWindow(VulkanContext &vk) {
    if (vk.windowExtent.width == 0 || vk.windowExtent.height == 0) {
        throw std::runtime_error("Invalid extent");
    }
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    vk.window = glfwCreateWindow(static_cast<int>(vk.windowExtent.width),
                                 static_cast<int>(vk.windowExtent.height),
                                 "Vulkan", nullptr, nullptr);
    glfwCreateWindowSurface(vk.instance, vk.window, nullptr, &vk.surface);
}

void initVulkanInstance(VulkanContext &vk) {
    VK_CHECK(volkInitialize())
    vkb::InstanceBuilder builder;
    auto instRet = builder.set_app_name("Vulkan App")
            .request_validation_layers(vk.useValidationLayer)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

    vk.instance = instRet.value();
    volkLoadInstance(vk.instance);
}

void initVulkanDevice(VulkanContext &vk,
                      const VulkanFeatures &ctxFeatures) {
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = ctxFeatures.dynamicRendering ? VK_TRUE : VK_FALSE;
    features13.synchronization2 = ctxFeatures.synchronization2 ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = ctxFeatures.bufferDeviceAddress ? VK_TRUE : VK_FALSE;
    features12.descriptorIndexing = ctxFeatures.descriptorIndexing ? VK_TRUE : VK_FALSE;

    vkb::PhysicalDeviceSelector selector{vk.instance};
    vkb::PhysicalDevice vkbPhysicalDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_12(features12)
            .set_surface(vk.surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    vk.physicalDevice = vkbPhysicalDevice;
    vk.device = vkbDevice;
    volkLoadDevice(vk.device);

    vk.graphicsQueue = vk.device.get_queue(vkb::QueueType::graphics).value();
    vk.graphicsFamily = vk.device.get_queue_index(vkb::QueueType::graphics).value();
    vk.presentQueue = vk.device.get_queue(vkb::QueueType::present).value();
    vk.presentFamily = vk.device.get_queue_index(vkb::QueueType::present).value();
}

void terminateVulkan(VulkanContext &vk) {
    vkDeviceWaitIdle(vk.device);
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        vkDestroyCommandPool(vk.device, vk.frames[i].commandPool, nullptr);
        vkDestroyFence(vk.device, vk.frames[i].renderFence, nullptr);
        vkDestroySemaphore(vk.device, vk.frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(vk.device, vk.frames[i].imageAvailableSemaphore, nullptr);
    }


    vkDestroyImageView(vk.device, vk.drawImage.imageView, nullptr);
    vmaDestroyImage(vk.allocator, vk.drawImage.image, vk.drawImage.allocation);

    destroySwapchain(vk);

    vmaDestroyAllocator(vk.allocator);
    vkb::destroy_device(vk.device);

    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkb::destroy_instance(vk.instance);

    glfwDestroyWindow(vk.window);
    glfwTerminate();
}

void initSwapchain(VulkanContext &vk) {
    createSwapchain(vk);

    VkExtent3D drawImageExtent = {
            vk.windowExtent.width,
            vk.windowExtent.height,
            1,
    };

    vk.drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    vk.drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages = {};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo drawImageInfo = imageCreateInfo(vk.drawImage.imageFormat, drawImageUsages,
                                                      vk.drawImage.imageExtent);

    VmaAllocationCreateInfo drawImageAllocInfo = {};
    drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    drawImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(vk.allocator, &drawImageInfo, &drawImageAllocInfo,
                   &vk.drawImage.image, &vk.drawImage.allocation, nullptr);

    VkImageViewCreateInfo drawImageViewInfo = imageViewCreateInfo(vk.drawImage.imageFormat, vk.drawImage.image,
                                                                  VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(vk.device, &drawImageViewInfo, nullptr, &vk.drawImage.imageView))
}

void createSwapchain(VulkanContext &vk) {
    vkb::SwapchainBuilder swapchainBuilder{vk.physicalDevice, vk.device, vk.surface};

    vk.swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto ret = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{
                    .format = vk.swapchainImageFormat,
                    .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            })
            .set_desired_extent(vk.windowExtent.width, vk.windowExtent.height)
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(vk.swapchain)
            .build();

    // destroy old swapchain when rebuilding
    destroySwapchain(vk);

    vk.swapchain = ret.value();
    vk.swapchainImages = vk.swapchain.get_images().value();
    vk.swapchainImageViews = vk.swapchain.get_image_views().value();
}

void initCommands(VulkanContext &vk) {
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = vk.graphicsFamily;

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        VK_CHECK(vkCreateCommandPool(vk.device, &commandPoolInfo, nullptr, &vk.frames[i].commandPool))

        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool = vk.frames[i].commandPool;
        commandBufferInfo.commandBufferCount = 1;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(vk.device, &commandBufferInfo, &vk.frames[i].commandBuffer))
    }
}

void initSyncStructures(VulkanContext &vk) {
    VkFenceCreateInfo fenceInfo = fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreInfo = semaphoreCreateInfo();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        VK_CHECK(vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.frames[i].renderFence))
        VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.frames[i].imageAvailableSemaphore))
        VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.frames[i].renderSemaphore))
    }
}

void initVmaAllocator(VulkanContext &vk) {
    VmaVulkanFunctions vmaVulkanFunc = {};
    vmaVulkanFunc.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaVulkanFunc.vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = vk.instance;
    allocatorInfo.physicalDevice = vk.physicalDevice;
    allocatorInfo.device = vk.device;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunc;

    vmaCreateAllocator(&allocatorInfo, &vk.allocator);
}

void resizeWindow(VulkanContext &vk) {
    vkDeviceWaitIdle(vk.device);

    int width, height;
    glfwGetFramebufferSize(vk.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(vk.window, &width, &height);
        glfwWaitEvents();
    }
    vk.windowExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    createSwapchain(vk);
}

void destroySwapchain(VulkanContext &vk) {
    vkb::destroy_swapchain(vk.swapchain);

    for (auto &imageView: vk.swapchainImageViews) {
        vkDestroyImageView(vk.device, imageView, nullptr);
    }
}

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = flags;

    return info;
}

void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask) {

    VkImageAspectFlags aspectMask = newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageSubresourceRange subresourceRange = imageSubresourceRange(aspectMask);

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

VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signalSemaphoreInfo,
                         VkSemaphoreSubmitInfo *waitSemaphoreInfo) {
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

VulkanBuffer createBuffer(VmaAllocator allocator,
                          size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VulkanBuffer newBuffer = {};

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo,
                             &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info))

    return newBuffer;
}

void destroyBuffer(VmaAllocator allocator, const VulkanBuffer &buffer) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
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

VkResult createShaderModule(VkDevice device, std::filesystem::path shaderFile, VkShaderModule *shaderModule) {
    std::vector<char> code = readFile(shaderFile, true);

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    return vkCreateShaderModule(device, &info, nullptr, shaderModule);
}

