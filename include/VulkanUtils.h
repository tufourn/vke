#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <VkBootstrap.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <array>

constexpr int MAX_CONCURRENT_FRAMES = 2;

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

struct FrameData {
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct VulkanContext {
    vkb::Instance instance = {};
    bool useValidationLayer = true;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    GLFWwindow *window = nullptr;
    VkExtent2D extent = {800, 600};

    vkb::Device device = {};
    vkb::PhysicalDevice physicalDevice = {};

    uint32_t graphicsFamily;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t presentFamily;
    VkQueue presentQueue = VK_NULL_HANDLE;

    vkb::Swapchain swapchain = {};
    std::vector<VkImage> swapchainImages = {};
    std::vector<VkImageView> swapchainImageViews = {};
    VkFormat swapchainImageFormat;

    std::array<FrameData, MAX_CONCURRENT_FRAMES> frames;
};

struct VulkanFeatures {
    bool dynamicRendering = true;
    bool synchronization2 = true;
    bool bufferDeviceAddress = true;
    bool descriptorIndexing = true;
};

void initVulkan(VulkanContext &vk, const VulkanFeatures &ctxFeatures);

void initVulkanInstance(VulkanContext &vk);

void initWindow(VulkanContext &vk);

void initVulkanDevice(VulkanContext &vk, const VulkanFeatures &ctxFeatures);

void terminateVulkan(VulkanContext &vk);

void initSwapchain(VulkanContext &vk);

void createSwapchain(VulkanContext &vk);

void destroySwapchain(VulkanContext &vk);

void resizeWindow(VulkanContext &vk);

void initCommands(VulkanContext &vk);

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

void initSyncStructures(VulkanContext &vk);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);

VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);

void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);

VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signalSemaphoreInfo,
                         VkSemaphoreSubmitInfo *waitSemaphoreInfo);

