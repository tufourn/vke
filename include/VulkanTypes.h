#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <memory>

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

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
};

struct GPUMeshBuffers {
    VulkanBuffer indexBuffer;
    VulkanBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

