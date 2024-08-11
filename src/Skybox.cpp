#include "Skybox.h"

#include "MeshGenerator.h"
#include "VulkanContext.h"
#include "stb_image.h"
#include "VulkanUtils.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include <numbers>

Skybox::Skybox(VulkanContext *vulkanContext) : m_vulkanContext(vulkanContext) {
    auto meshBuffers = createCubeMesh(2.0, 2.0, 2.0);

    const size_t vertexBufferSize = meshBuffers.vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = meshBuffers.indices.size() * sizeof(uint32_t);

    VulkanBuffer stagingBuffer = m_vulkanContext->createBuffer(vertexBufferSize + indexBufferSize,
                                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                               VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    void *data = stagingBuffer.info.pMappedData;

    vertexBuffer = m_vulkanContext->createBuffer(vertexBufferSize,
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(data, meshBuffers.vertices.data(), vertexBufferSize);

    indexBuffer = m_vulkanContext->createBuffer(indexBufferSize,
                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    memcpy(static_cast<std::byte *>(data) + vertexBufferSize, meshBuffers.indices.data(), indexBufferSize);

    m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy = {0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, vertexBuffer.buffer,
                        1, &vertexCopy);

        VkBufferCopy indexCopy = {0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, indexBuffer.buffer,
                        1, &indexCopy);
    });

    m_vulkanContext->destroyBuffer(stagingBuffer);

    createOffscreenDrawImage();

    cubemap = createCubemapImage({cubeRes, cubeRes, 1}, VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    irradianceMap = createCubemapImage({cubeRes, cubeRes, 1}, VK_FORMAT_R32G32B32A32_SFLOAT,
                                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
        VkUtil::transitionImage(cmd, cubemap.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
        VkUtil::transitionImage(cmd, irradianceMap.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
    });
}

bool Skybox::load(std::filesystem::path filePath) {
    auto path = std::filesystem::current_path() / filePath;
    int width, height, numChannels;

    m_loaded = false;

    unsigned char *stbData = stbi_load(path.string().c_str(), &width, &height, &numChannels, 4);
    if (stbData) {
        VkExtent3D extent(width, height, 1);

        loadedImage = m_vulkanContext->createImage(stbData, extent, VK_FORMAT_R8G8B8A8_UNORM,
                                                   VK_IMAGE_USAGE_SAMPLED_BIT, true);

        stbi_image_free(stbData);
        m_loaded = true;

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 16.f;

        vkCreateSampler(m_vulkanContext->device, &samplerInfo, nullptr, &sampler);

        return true;
    }

    return false;
}

Skybox::~Skybox() {
    m_vulkanContext->destroyBuffer(vertexBuffer);
    m_vulkanContext->destroyBuffer(indexBuffer);

    if (m_loaded) {
        m_vulkanContext->destroyImage(loadedImage);
        vkDestroySampler(m_vulkanContext->device, sampler, nullptr);
    }

    m_vulkanContext->destroyImage(m_offscreenImage);
    m_vulkanContext->destroyImage(cubemap);
    m_vulkanContext->destroyImage(irradianceMap);

    vkDestroyPipelineLayout(m_vulkanContext->device, cubemapPipelineLayout, nullptr);
    vkDestroyPipeline(m_vulkanContext->device, cubemapPipeline, nullptr);
    vkDestroyPipeline(m_vulkanContext->device, irradianceMapPipeline, nullptr);

    cubemapDescriptors.destroyPools(m_vulkanContext->device);
    vkDestroyDescriptorSetLayout(m_vulkanContext->device, cubemapDescriptorLayout, nullptr);

}

void Skybox::createCubemap() {
    if (!m_loaded) {
        return;
    }

    VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(m_offscreenImage.imageView, nullptr);
    VkRenderingInfo renderInfo = VkInit::renderingInfo(
            {m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height}, &colorAttachment, nullptr);

    VkViewport viewport = VkInit::viewport(m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height);
    VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height}
    };

    SkyboxPushBlock pc = {};
    pc.vertexBuffer = m_vulkanContext->getBufferAddress(vertexBuffer);

    DescriptorWriter writer;
    writer.writeImage(0, loadedImage.imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    writer.updateSet(m_vulkanContext->device, cubemapDescriptorSet);

    for (size_t face_i = 0; face_i < faceMatrices.size(); face_i++) {
        pc.matrix =
                glm::perspective((float) (std::numbers::pi / 2.0), 1.0f, 0.1f, (float) cubeRes) * faceMatrices[face_i];

        m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
            // draw one face of cube
            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipeline);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipelineLayout,
                                    0, 1, &cubemapDescriptorSet, 0, nullptr);
            vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmd, cubemapPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(SkyboxPushBlock),
                               &pc);
            vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
            vkCmdEndRendering(cmd);

            // transition drawn image to transfer src to copy to cubemap texture
            VkUtil::transitionImage(cmd, m_offscreenImage.image,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = face_i;
            copyRegion.dstSubresource.mipLevel = 0; // TODO: add mipmap copy
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(
                    cmd, m_offscreenImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion
            );

            // transition drawn image back to color attachment
            VkUtil::transitionImage(cmd, m_offscreenImage.image,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
        });
    }

    m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
        VkUtil::transitionImage(cmd, cubemap.image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
    });
}

void Skybox::createPipelines() {
    VkPushConstantRange range = {};
    range.offset = 0;
    range.size = sizeof(SkyboxPushBlock);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    cubemapDescriptorLayout = layoutBuilder.build(m_vulkanContext->device, VK_SHADER_STAGE_FRAGMENT_BIT);

    cubemapDescriptors = DescriptorAllocator();
    std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    cubemapDescriptors.init(m_vulkanContext->device, 1, frameSizes);
    cubemapDescriptorSet = cubemapDescriptors.allocate(m_vulkanContext->device, cubemapDescriptorLayout);

    VkPipelineLayoutCreateInfo pipelineCI = VkInit::pipelineLayoutCreateInfo();
    pipelineCI.setLayoutCount = 1;
    pipelineCI.pSetLayouts = &cubemapDescriptorLayout;
    pipelineCI.pPushConstantRanges = &range;
    pipelineCI.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vulkanContext->device, &pipelineCI, nullptr, &cubemapPipelineLayout))

    VkShaderModule cubemapVertShader, cubemapFragShader;
    VK_CHECK(m_vulkanContext->createShaderModule("shaders/cubemap.vert.spv", &cubemapVertShader))
    VK_CHECK(m_vulkanContext->createShaderModule("shaders/cubemap.frag.spv", &cubemapFragShader))

    PipelineBuilder pipelineBuilder;
    pipelineBuilder
            .setLayout(cubemapPipelineLayout)
            .setShaders(cubemapVertShader, cubemapFragShader)
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setMultisamplingNone()
            .disableBlending()
            .disableDepthTest()
            .setColorAttachmentFormat(m_offscreenImage.imageFormat);

    cubemapPipeline = pipelineBuilder.build(m_vulkanContext->device);

    VkShaderModule irradianceMapFragShader;
    VK_CHECK(m_vulkanContext->createShaderModule("shaders/irradiance_map.frag.spv", &irradianceMapFragShader))

    pipelineBuilder.setShaders(cubemapVertShader, irradianceMapFragShader);
    irradianceMapPipeline = pipelineBuilder.build(m_vulkanContext->device);

    vkDestroyShaderModule(m_vulkanContext->device, cubemapVertShader, nullptr);
    vkDestroyShaderModule(m_vulkanContext->device, cubemapFragShader, nullptr);
    vkDestroyShaderModule(m_vulkanContext->device, irradianceMapFragShader, nullptr);
}

void Skybox::createOffscreenDrawImage() {
    m_offscreenImage = {};

    VkExtent3D offscreenImageExtent = {cubeRes, cubeRes, 1};
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    m_offscreenImage = m_vulkanContext->createImage(offscreenImageExtent, VK_FORMAT_R32G32B32A32_SFLOAT, usage, true);

    m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
        VkUtil::transitionImage(cmd, m_offscreenImage.image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
    });
}

VulkanImage Skybox::createCubemapImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage) {
    VulkanImage newImage = {};
    newImage.imageFormat = format;
    newImage.imageExtent = extent;

    VkImageCreateInfo imgInfo = VkInit::imageCreateInfo(format, usage, extent);
    // todo: add mipmaps
//    imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    imgInfo.arrayLayers = 6;
    imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.priority = 1.0f;

    VK_CHECK(vmaCreateImage(m_vulkanContext->allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation,
                            nullptr))

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo imgViewInfo = VkInit::imageViewCreateInfo(format, newImage.image, aspectFlag);
    imgViewInfo.subresourceRange.levelCount = imgInfo.mipLevels;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imgViewInfo.subresourceRange.layerCount = 6;

    VK_CHECK(vkCreateImageView(m_vulkanContext->device, &imgViewInfo, nullptr, &newImage.imageView))

    return newImage;
}

void Skybox::init() {
    createPipelines();
    createCubemap();
    createIrradianceMap();
}

void Skybox::createIrradianceMap() {
    if (!m_loaded) {
        return;
    }

    VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(m_offscreenImage.imageView, nullptr);
    VkRenderingInfo renderInfo = VkInit::renderingInfo(
            {m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height}, &colorAttachment, nullptr);

    VkViewport viewport = VkInit::viewport(m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height);
    VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {m_offscreenImage.imageExtent.width, m_offscreenImage.imageExtent.height}
    };

    SkyboxPushBlock pc = {};
    pc.vertexBuffer = m_vulkanContext->getBufferAddress(vertexBuffer);

    DescriptorWriter writer;
    writer.writeImage(0, cubemap.imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    writer.updateSet(m_vulkanContext->device, cubemapDescriptorSet);

    for (size_t face_i = 0; face_i < faceMatrices.size(); face_i++) {
        pc.matrix =
                glm::perspective((float) (std::numbers::pi / 2.0), 1.0f, 0.1f, (float) cubeRes) * faceMatrices[face_i];

        m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
            // draw one face of cube
            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradianceMapPipeline);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipelineLayout,
                                    0, 1, &cubemapDescriptorSet, 0, nullptr);
            vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmd, cubemapPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(SkyboxPushBlock),
                               &pc);
            vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
            vkCmdEndRendering(cmd);

            // transition drawn image to transfer src to copy to cubemap texture
            VkUtil::transitionImage(cmd, m_offscreenImage.image,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = face_i;
            copyRegion.dstSubresource.mipLevel = 0; // TODO: add mipmap copy
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(
                    cmd, m_offscreenImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    irradianceMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion
            );

            // transition drawn image back to color attachment
            VkUtil::transitionImage(cmd, m_offscreenImage.image,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
        });
    }

    m_vulkanContext->immediateSubmit([&](VkCommandBuffer cmd) {
        VkUtil::transitionImage(cmd, irradianceMap.image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
    });

}
