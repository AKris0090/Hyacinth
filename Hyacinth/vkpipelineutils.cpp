#include "vkpipelineutils.h"
#include "vkdebugutils.h"
#include "vkmeshutils.h"

void VulkanPipelineBuilder::reset() {
    m_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_colorBlendAttachment = {};
    m_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	m_viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    m_vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    m_pipeline = {};
    m_shaderStages.clear();
}

void VulkanPipelineBuilder::addShader(VkDevice& device, std::string shaderFile, VkShaderStageFlagBits stage) {
    auto shaderCode = readFile(shaderFile);

    VkShaderModule shaderModule = createShaderModule(device, shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    m_shaderStages.push_back(shaderStageInfo);
}

void VulkanPipelineBuilder::buildPipeline(VkDevice& dev) {
    m_viewportState.viewportCount = 1;
    m_viewportState.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_colorBlendAttachment;

    auto bindings = Vertex::getBindingDescription();
    auto attributesNormal = Vertex::getAttributeDescriptions();
    auto attributesPosOnly = Vertex::getPositionAttributeDescription();

    m_vertexInputInfo.vertexBindingDescriptionCount = 1;
    m_vertexInputInfo.pVertexBindingDescriptions = &bindings;
    if (depthPass) {
        m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributesPosOnly.size());
        m_vertexInputInfo.pVertexAttributeDescriptions = attributesPosOnly.data();
    }
    else {
        m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributesNormal.size());
        m_vertexInputInfo.pVertexAttributeDescriptions = attributesNormal.data();
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.pNext = &m_renderInfo;
    pipelineInfo.stageCount = (uint32_t)m_shaderStages.size();
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = &m_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &m_viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.layout = m_pipeline.layout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;
    pipelineInfo.pDynamicState = &dynamicInfo;

    VK_CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline.pipeline));
    if (m_pipeline.pipeline == VK_NULL_HANDLE) { throw std::runtime_error("Failed to create graphics pipeline!"); }

    for (auto & stage : m_shaderStages) {
        vkDestroyShaderModule(dev, stage.module, nullptr);
	}
}

void VulkanPipelineBuilder::setInputTopology(VkPrimitiveTopology topology) {
    m_inputAssembly.topology = topology;
	m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void VulkanPipelineBuilder::setPolygonMode(VkPolygonMode polygonMode) {
    m_rasterizer.polygonMode = polygonMode;
    m_rasterizer.lineWidth = 1.0f;
    m_rasterizer.depthClampEnable = VK_FALSE;
    m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
    m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    m_rasterizer.lineWidth = 1.0f;
    m_rasterizer.depthBiasEnable = VK_FALSE;
}

void VulkanPipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
    m_rasterizer.cullMode = cullMode;
    m_rasterizer.frontFace = frontFace;
}

void VulkanPipelineBuilder::setMultisamplingNone() {
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void VulkanPipelineBuilder::setMultisampling(VkSampleCountFlagBits sampleCount) {
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.minSampleShading = 0.0f;
    m_multisampling.rasterizationSamples = sampleCount;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void VulkanPipelineBuilder::disableBlending() {
    m_colorBlendAttachment.blendEnable = VK_FALSE;
	m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}

void VulkanPipelineBuilder::setColorAttachmentFormat(VkFormat format) {
    m_colorAttachmentformat = format;
    m_renderInfo.colorAttachmentCount = 1;
    m_renderInfo.pColorAttachmentFormats = &m_colorAttachmentformat;
}

void VulkanPipelineBuilder::setDepthAttachmentFormat(VkFormat format) {
    m_renderInfo.depthAttachmentFormat = format;
}

void VulkanPipelineBuilder::enableDepthTest(bool depthWrite, VkCompareOp op) {
    m_depthStencil.depthTestEnable = VK_TRUE;
    m_depthStencil.depthWriteEnable = depthWrite;
    m_depthStencil.depthCompareOp = op;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.front = {};
    m_depthStencil.back = {};
    m_depthStencil.minDepthBounds = 0.f;
    m_depthStencil.maxDepthBounds = 1.f;
}

void VulkanPipelineBuilder::setDefaultAttributes() {
    depthPass = false;
}

void VulkanPipelineBuilder::setPositionAttribute() {
    depthPass = true;
}

void VulkanPipelineBuilder::destroyPipeline(DeviceContext& ctx) {
    m_pipeline.destroy(*ctx.device);
}