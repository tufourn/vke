#include "VulkanDescriptor.h"
#include "VulkanUtils.h"

void DescriptorAllocator::init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios) {
    m_ratios.clear();

    for (auto& ratio: poolRatios) {
        m_ratios.push_back(ratio);
    }

    VkDescriptorPool newPool = createPool(device, initialSets, poolRatios);

    m_readyPools.push_back(newPool);
}

void DescriptorAllocator::clearPools(VkDevice device) {
    for (auto& pool: m_readyPools) {
        vkResetDescriptorPool(device, pool, 0);
    }

    for (auto& pool: m_fullPools) {
        vkResetDescriptorPool(device, pool, 0);
        m_readyPools.push_back(pool);
    }

    m_fullPools.clear();
}

void DescriptorAllocator::destroyPools(VkDevice device) {
    for (auto& pool: m_readyPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    for (auto& pool: m_fullPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    m_readyPools.clear();
    m_fullPools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void *pNext) {
    VkDescriptorPool poolToUse = getPool(device);

    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = pNext;
    info.descriptorPool = poolToUse;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VkResult ret = vkAllocateDescriptorSets(device, &info, &descriptorSet);

    if (ret == VK_ERROR_OUT_OF_POOL_MEMORY || ret == VK_ERROR_FRAGMENTED_POOL) {
        m_fullPools.push_back(poolToUse);
        poolToUse = getPool(device);
        info.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &info, &descriptorSet))
    }

    m_readyPools.push_back(poolToUse);

    return descriptorSet;
}

VkDescriptorPool DescriptorAllocator::getPool(VkDevice device) {
    VkDescriptorPool newPool;
    if (!m_readyPools.empty()) {
        newPool = m_readyPools.back();
        m_readyPools.pop_back();
    } else {
        newPool = createPool(device, m_setsPerPool, m_ratios);
        m_setsPerPool *= 2; // mimic vector resize
        if (m_setsPerPool > MAX_SETS_PER_POOL) {
            m_setsPerPool = MAX_SETS_PER_POOL;
        }
    }
    return newPool;
}

VkDescriptorPool DescriptorAllocator::createPool(VkDevice device,
                                                 uint32_t setCount,
                                                 std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio: poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = static_cast<uint32_t>(ratio.ratio) * setCount,
        });
    }

    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = 0;
    info.maxSets = setCount;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &info, nullptr, &newPool);

    return newPool;
}

void DescriptorWriter::writeImage(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout,
                                  VkDescriptorType type) {
    VkDescriptorImageInfo &info = m_imageInfos.emplace_back(
            VkDescriptorImageInfo{
                    .sampler = sampler,
                    .imageView = imageView,
                    .imageLayout = layout,
            }
    );

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be filled in updateSets()
    write.pImageInfo = &info;

    m_writeSets.push_back(write);
}

void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    VkDescriptorBufferInfo &info = m_bufferInfos.emplace_back(
            VkDescriptorBufferInfo{
                    .buffer = buffer,
                    .offset = offset,
                    .range = size,
            }
    );

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be filled in updateSets()
    write.pBufferInfo = &info;

    m_writeSets.push_back(write);
}

void DescriptorWriter::clear() {
    m_imageInfos.clear();
    m_bufferInfos.clear();
    m_writeSets.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet &write: m_writeSets) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(m_writeSets.size()), m_writeSets.data(), 0, nullptr);
}

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.binding = binding;
    newBinding.descriptorType = type;
    newBinding.descriptorCount = 1;

    m_bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear() {
    m_bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void *pNext,
                                                     VkDescriptorSetLayoutCreateFlags flags) {
    for (auto& binding : m_bindings) {
        binding.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = pNext;
    info.bindingCount = static_cast<uint32_t>(m_bindings.size());
    info.pBindings = m_bindings.data();
    info.flags = flags;

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout))

    return layout;
}
