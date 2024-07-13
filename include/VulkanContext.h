#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <array>
#include <filesystem>
#include <functional>

#include "VulkanTypes.h"
#include "VulkanDescriptor.h"

struct FrameData {
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    DescriptorAllocator frameDescriptors = {};
};

constexpr int MAX_CONCURRENT_FRAMES = 2;

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
    VulkanImage depthImage = {};

    std::array<FrameData, MAX_CONCURRENT_FRAMES> frames;

    void init();

    void terminate();

    void resizeWindow();

    VkResult createShaderModule(const std::filesystem::path &shaderFile, VkShaderModule *shaderModule) const;

    [[nodiscard]] VulkanBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags,
                                            VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO) const;

    void destroyBuffer(const VulkanBuffer &buffer) const;

    [[nodiscard]] VulkanImage createImage(VkExtent3D extent,
                                          VkFormat format, VkImageUsageFlags usage, bool mipmapped) const;

    [[nodiscard]] VulkanImage createImage(const void *data, VkExtent3D extent,
                                          VkFormat format, VkImageUsageFlags usage, bool mipmapped) const;

    void destroyImage(const VulkanImage &img) const;

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function) const;

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
    void freeMesh(GPUMeshBuffers& meshBuffers);

private:
    VkFence m_immediateFence = VK_NULL_HANDLE;
    VkCommandBuffer m_immediateCommandBuffer = VK_NULL_HANDLE;
    VkCommandPool m_immediateCommandPool = VK_NULL_HANDLE;

    void initVulkanInstance();

    void initWindow();

    void initVulkanDevice();

    void initSwapchain();

    void createSwapchain();

    void createDrawImage();

    void createDepthImage();

    void destroySwapchain();

    void initCommands();

    void initSyncStructures();

    void initVmaAllocator();
};
