#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <array>
#include <filesystem>

constexpr int MAX_CONCURRENT_FRAMES = 2;

struct VulkanFeatures {
    bool dynamicRendering = true;
    bool synchronization2 = true;
    bool bufferDeviceAddress = true;
    bool descriptorIndexing = true;
};

struct VulkanBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct VulkanImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

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

    VulkanFeatures features = {};

    GLFWwindow *window = nullptr;
    VkExtent2D windowExtent = {800, 600};

    vkb::Device device = {};
    vkb::PhysicalDevice physicalDevice = {};

    VmaAllocator allocator = nullptr;

    uint32_t graphicsFamily;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t presentFamily;
    VkQueue presentQueue = VK_NULL_HANDLE;

    vkb::Swapchain swapchain = {};
    std::vector<VkImage> swapchainImages = {};
    std::vector<VkImageView> swapchainImageViews = {};
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;

    VulkanImage drawImage = {};

    std::array<FrameData, MAX_CONCURRENT_FRAMES> frames;

    void init();

    void terminate();

    void resizeWindow();

    VkResult createShaderModule(std::filesystem::path shaderFile, VkShaderModule *shaderModule) const;

    VulkanBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    void destroyBuffer(const VulkanBuffer &buffer) const;

    VulkanImage createImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped);

    void destroyImage(const VulkanImage &img);

private:
    void initVulkanInstance();

    void initWindow();

    void initVulkanDevice();

    void initSwapchain();

    void createSwapchain();

    void createDrawImage();

    void destroySwapchain();

    void initCommands();

    void initSyncStructures();

    void initVmaAllocator();

};


