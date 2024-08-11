#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "VulkanContext.h"

#include "VulkanUtils.h"
#include "VulkanInit.h"
#include "Utils.h"

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>

void VulkanContext::init() {
    initVulkanInstance();
    initWindow();
    initVulkanDevice();
    initVmaAllocator();
    initSwapchain();
    initCommands();
    initSyncStructures();
}

void VulkanContext::initWindow() {
    if (windowExtent.width == 0 || windowExtent.height == 0) {
        throw std::runtime_error("Invalid extent");
    }
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(static_cast<int>(windowExtent.width),
                              static_cast<int>(windowExtent.height),
                              "Vulkan", nullptr, nullptr);
    glfwCreateWindowSurface(instance, window, nullptr, &surface);
}

void VulkanContext::initVulkanInstance() {
    VK_CHECK(volkInitialize())
    vkb::InstanceBuilder builder;
    auto instRet = builder.set_app_name("Vulkan App")
            .request_validation_layers(useValidationLayer)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

    instance = instRet.value();
    volkLoadInstance(instance);
}

void VulkanContext::initVulkanDevice() {
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = features.dynamicRendering ? VK_TRUE : VK_FALSE;
    features13.synchronization2 = features.synchronization2 ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = features.bufferDeviceAddress ? VK_TRUE : VK_FALSE;
    features12.descriptorIndexing = features.descriptorIndexing ? VK_TRUE : VK_FALSE;
    features12.runtimeDescriptorArray = features.runtimeDescriptorArray ? VK_TRUE : VK_FALSE;
    features12.shaderSampledImageArrayNonUniformIndexing = features.shaderSampledImageArrayNonUniformIndexing ? VK_TRUE : VK_FALSE;
    features12.shaderStorageBufferArrayNonUniformIndexing = features.shaderStorageBufferArrayNonUniformIndexing ? VK_TRUE : VK_FALSE;
    features12.descriptorBindingSampledImageUpdateAfterBind = features.descriptorBindingSampledImageUpdateAfterBind ? VK_TRUE : VK_FALSE;
    features12.descriptorBindingPartiallyBound = features.descriptorBindingPartiallyBound ? VK_TRUE : VK_FALSE;
    features12.descriptorBindingVariableDescriptorCount = features.descriptorBindingVariableDescriptorCount ? VK_TRUE : VK_FALSE;

    vkb::PhysicalDeviceSelector selector{instance};
    vkb::PhysicalDevice vkbPhysicalDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_12(features12)
            .set_surface(surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    physicalDevice = vkbPhysicalDevice;
    device = vkbDevice;
    volkLoadDevice(device);

    graphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
    graphicsFamily = device.get_queue_index(vkb::QueueType::graphics).value();
    presentQueue = device.get_queue(vkb::QueueType::present).value();
    presentFamily = device.get_queue_index(vkb::QueueType::present).value();
}

void VulkanContext::terminate() {
    vkDeviceWaitIdle(device);

    vkDestroyFence(device, m_immediateFence, nullptr);
    vkDestroyCommandPool(device, m_immediateCommandPool, nullptr);

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
        vkDestroyFence(device, frames[i].renderFence, nullptr);
        vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(device, frames[i].imageAvailableSemaphore, nullptr);
    }

    destroyImage(drawImage);
    destroyImage(depthImage);

    destroySwapchain();

    vmaDestroyAllocator(allocator);
    vkb::destroy_device(device);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_instance(instance);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void VulkanContext::initSwapchain() {
    createSwapchain();
    createDrawImage();
    createDepthImage();
}

void VulkanContext::createSwapchain() {
    vkb::SwapchainBuilder swapchainBuilder{physicalDevice, device, surface};

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto ret = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{
                .format = swapchainImageFormat,
                .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            })
            .set_desired_extent(windowExtent.width, windowExtent.height)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(swapchain)
            .build();

    // destroy old swapchain when rebuilding
    destroySwapchain();

    swapchain = ret.value();
    swapchainImages = swapchain.get_images().value();
    swapchainImageViews = swapchain.get_image_views().value();
}

void VulkanContext::initCommands() {
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = graphicsFamily;

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool))

        VkCommandBufferAllocateInfo cmdBufferAllocInfo = VkInit::commandBufferAllocateInfo(frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &frames[i].commandBuffer))
    }

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &m_immediateCommandPool))
    VkCommandBufferAllocateInfo immCmdBufferAllocInfo = VkInit::commandBufferAllocateInfo(m_immediateCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &immCmdBufferAllocInfo, &m_immediateCommandBuffer))
}

void VulkanContext::initSyncStructures() {
    VkFenceCreateInfo fenceInfo = VkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreInfo = VkInit::semaphoreCreateInfo();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].imageAvailableSemaphore))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].renderSemaphore))
    }

    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_immediateFence))
}

void VulkanContext::initVmaAllocator() {
    VmaVulkanFunctions vmaVulkanFunc = {};
    vmaVulkanFunc.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaVulkanFunc.vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunc;

    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanContext::resizeWindow() {
    vkDeviceWaitIdle(device);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    windowExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    createSwapchain();

    destroyImage(drawImage);
    destroyImage(depthImage);
    createDrawImage();
    createDepthImage();
}

void VulkanContext::destroySwapchain() {
    vkb::destroy_swapchain(swapchain);

    for (auto &imageView: swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
}

VulkanBuffer VulkanContext::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags,
                                         VmaMemoryUsage memoryUsage) const {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = flags;
    VulkanBuffer newBuffer = {};

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo,
        &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info))

    return newBuffer;
}

void VulkanContext::destroyBuffer(const VulkanBuffer &buffer) const {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

VkResult VulkanContext::createShaderModule(const std::filesystem::path &shaderFile,
                                           VkShaderModule *shaderModule) const {
    std::vector<char> code = readFile(shaderFile, true);

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    return vkCreateShaderModule(device, &info, nullptr, shaderModule);
}

VulkanImage VulkanContext::createImage(VkExtent3D extent,
                                       VkFormat format,
                                       VkImageUsageFlags usage,
                                       bool mipmapped) const {
    VulkanImage newImage = {};
    newImage.imageFormat = format;
    newImage.imageExtent = extent;

    VkImageCreateInfo imgInfo = VkInit::imageCreateInfo(format, usage, extent);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.priority = 1.0f;

    VK_CHECK(vmaCreateImage(allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr))

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo imgViewInfo = VkInit::imageViewCreateInfo(format, newImage.image, aspectFlag);
    imgViewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &newImage.imageView))

    return newImage;
}

void VulkanContext::destroyImage(const VulkanImage &img) const {
    vkDestroyImageView(device, img.imageView, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
}

void VulkanContext::createDrawImage() {
    VkExtent3D drawImageExtent = {
        windowExtent.width,
        windowExtent.height,
        1,
    };

    VkImageUsageFlags drawImageUsages = 0;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    drawImage = createImage(drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, false);
}

void VulkanContext::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func) const {
    VK_CHECK(vkResetFences(device, 1, &m_immediateFence))
    VK_CHECK(vkResetCommandBuffer(m_immediateCommandBuffer, 0))

    VkCommandBuffer cmd = m_immediateCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

    func(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd))

    VkCommandBufferSubmitInfo cmdSubmitInfo = VkInit::commandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submitInfo = VkInit::submitInfo(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, m_immediateFence))

    VK_CHECK(vkWaitForFences(device, 1, &m_immediateFence, true, 1e10))
}

VkDeviceAddress VulkanContext::getBufferAddress(const VulkanBuffer &buffer) const {
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = buffer.buffer;

    return vkGetBufferDeviceAddress(device, &deviceAddressInfo);
}

VulkanImage VulkanContext::createImage(const void *data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
                                       bool mipmapped) const {
    size_t dataSize = extent.depth * extent.width * extent.height * 4; // 1 byte for each rgba channel

    VulkanBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    VulkanImage newImage = createImage(extent, format,
                                       usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                       mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) {
        VkUtil::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                0,
                                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = extent;

        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            VkUtil::generateMipmaps(cmd, newImage.image, {newImage.imageExtent.width, newImage.imageExtent.height});
        } else {
            VkUtil::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                    VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                    VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT);
        }
    });

    destroyBuffer(uploadBuffer);

    return newImage;
}

void VulkanContext::createDepthImage() {
    VkExtent3D depthImageExtent = {
        windowExtent.width,
        windowExtent.height,
        1,
    };

    depthImage = createImage(depthImageExtent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             false);
}
