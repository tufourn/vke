#pragma once

#include <volk.h>
#include <span>
#include <vector>
#include <deque>

constexpr uint32_t MAX_SETS_PER_POOL = 4092;

struct DescriptorAllocator {
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);

    void clearPools(VkDevice device);

    void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void *pNext = nullptr);

private:
    VkDescriptorPool getPool(VkDevice device);

    VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> m_ratios;
    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    uint32_t m_setsPerPool = 1024;
};

struct DescriptorWriter {
    void writeImage(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);

    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    void clear();

    void updateSet(VkDevice device, VkDescriptorSet set);

private:
    std::deque<VkDescriptorImageInfo> m_imageInfos;
    std::deque<VkDescriptorBufferInfo> m_bufferInfos;
    std::vector<VkWriteDescriptorSet> m_writeSets;
};

struct DescriptorLayoutBuilder {
    void addBinding(uint32_t binding, VkDescriptorType type);

    void clear();

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages,
                                void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};