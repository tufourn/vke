#include "VulkanContext.h"

#include "VulkanUtils.h"
#include "VulkanInit.h"
#include "Utils.h"

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
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
        vkDestroyFence(device, frames[i].renderFence, nullptr);
        vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(device, frames[i].imageAvailableSemaphore, nullptr);
    }

    vkDestroyImageView(device, drawImage.imageView, nullptr);
    vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);

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

    VkExtent3D drawImageExtent = {
            windowExtent.width,
            windowExtent.height,
            1,
    };

    drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages = {};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo drawImageInfo = VkInit::imageCreateInfo(drawImage.imageFormat, drawImageUsages,
                                                      drawImage.imageExtent);

    VmaAllocationCreateInfo drawImageAllocInfo = {};
    drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    drawImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(allocator, &drawImageInfo, &drawImageAllocInfo,
                   &drawImage.image, &drawImage.allocation, nullptr);

    VkImageViewCreateInfo drawImageViewInfo = VkInit::imageViewCreateInfo(drawImage.imageFormat, drawImage.image,
                                                                  VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &drawImageViewInfo, nullptr, &drawImage.imageView))
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
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
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

        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool = frames[i].commandPool;
        commandBufferInfo.commandBufferCount = 1;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &frames[i].commandBuffer))
    }
}

void VulkanContext::initSyncStructures() {
    VkFenceCreateInfo fenceInfo = VkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreInfo = VkInit::semaphoreCreateInfo();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].imageAvailableSemaphore))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].renderSemaphore))
    }
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
}

void VulkanContext::destroySwapchain() {
    vkb::destroy_swapchain(swapchain);

    for (auto &imageView: swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
}

VulkanBuffer VulkanContext::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const {
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

void VulkanContext::destroyBuffer(const VulkanBuffer &buffer) const {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

VkResult VulkanContext::createShaderModule(std::filesystem::path shaderFile, VkShaderModule *shaderModule) const {
    std::vector<char> code = readFile(shaderFile, true);

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    return vkCreateShaderModule(device, &info, nullptr, shaderModule);
}

