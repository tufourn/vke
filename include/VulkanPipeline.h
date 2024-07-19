#pragma once

#include <volk.h>

#include <vector>

class PipelineBuilder {
public:
    PipelineBuilder();

    VkPipeline build(VkDevice device);

    PipelineBuilder &setLayout(VkPipelineLayout layout);

    PipelineBuilder &setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    PipelineBuilder &setInputTopology(VkPrimitiveTopology topology);

    PipelineBuilder &setPolygonMode(VkPolygonMode mode);

    PipelineBuilder &setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    PipelineBuilder &setMultisamplingNone();

    PipelineBuilder &disableBlending();

    PipelineBuilder &setColorAttachmentFormat(VkFormat format);

    PipelineBuilder &setDepthAttachmentFormat(VkFormat format);

    PipelineBuilder &disableDepthTest();

    PipelineBuilder &enableDepthTest(VkBool32 depthWriteEnable, VkCompareOp compareOp);

private:
    void clear();

    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages = {};
    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly = {};
    VkPipelineRasterizationStateCreateInfo m_rasterizer = {};
    VkPipelineColorBlendAttachmentState m_colorBlendAttachment = {};
    VkPipelineMultisampleStateCreateInfo m_multisampling = {};
    VkPipelineLayout m_pipelineLayout = {};
    VkPipelineDepthStencilStateCreateInfo m_depthStencil = {};
    VkPipelineRenderingCreateInfo m_renderInfo = {};
    VkFormat m_colorAttachmentFormat = {};
};
