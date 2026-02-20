#include "vkpipelineutils.h"
#include "vkdebugutils.h"
#include "vkmeshutils.h"

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

static VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(vkdeviceutils::device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

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

void VulkanPipelineBuilder::addShader(std::string shaderFile, VkShaderStageFlagBits stage) {
    auto shaderCode = readFile(shaderFile);

    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    m_shaderStages.push_back(shaderStageInfo);
}

void VulkanPipelineBuilder::buildPipeline() {
    m_viewportState.viewportCount = 1;
    m_viewportState.scissorCount = 1;

    VkPipelineColorBlendAttachmentState attachment2{};
    attachment2.blendEnable = VK_FALSE;
    attachment2.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments = { m_colorBlendAttachment, attachment2 };

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = numColorAttachments;
    colorBlending.pAttachments = blendAttachments.data();

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

    VK_CHECK(vkCreateGraphicsPipelines(vkdeviceutils::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline.pipeline));
    if (m_pipeline.pipeline == VK_NULL_HANDLE) { throw std::runtime_error("Failed to create graphics pipeline!"); }

    for (auto & stage : m_shaderStages) {
        vkDestroyShaderModule(vkdeviceutils::device, stage.module, nullptr);
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

void VulkanPipelineBuilder::enableBlending() {
    m_colorBlendAttachment.blendEnable = VK_TRUE;

    m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

    m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}

void VulkanPipelineBuilder::setColorAttachmentFormat(VkFormat format, int numAttachments) {
    m_colorAttachmentformats.resize(numAttachments);
    for(int i = 0; i < numAttachments; i++) {
        m_colorAttachmentformats[i] = format;
	}
    m_renderInfo.colorAttachmentCount = numAttachments;
    m_renderInfo.pColorAttachmentFormats = m_colorAttachmentformats.data();
}

void VulkanPipelineBuilder::setDepthAttachmentFormat(VkFormat format) {
    m_renderInfo.depthAttachmentFormat = format;
}

void VulkanPipelineBuilder::setStencilAttachmentFormat(VkFormat format) {
    m_renderInfo.stencilAttachmentFormat = format;
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

void VulkanPipelineBuilder::destroyPipeline() {
    m_pipeline.destroy();
}

VkPipelineShaderStageCreateInfo vkpipelineutils::createShader(std::string shaderFile, VkShaderStageFlagBits stage) {
    auto shaderCode = readFile(shaderFile);

    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

VulkanPipeline vkpipelineutils::createComputePipeline(VkDescriptorSetLayout* layouts, int numLayouts, VkPushConstantRange* pcRanges, int numPcRanges, std::string shaderFile) {
    VulkanPipeline pipeline{};

    VkPipelineLayoutCreateInfo pipeLineLayoutCInfo{};
    pipeLineLayoutCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLineLayoutCInfo.setLayoutCount = numLayouts;
    pipeLineLayoutCInfo.pSetLayouts = layouts;
    pipeLineLayoutCInfo.pushConstantRangeCount = numPcRanges;
    pipeLineLayoutCInfo.pPushConstantRanges = pcRanges;

    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipeLineLayoutCInfo, nullptr, &pipeline.layout));

    VkPipelineShaderStageCreateInfo computeShader = createShader(shaderFile, VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo computePipelineCInfo{};
    computePipelineCInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCInfo.stage = computeShader;

    computePipelineCInfo.layout = pipeline.layout;

    VK_CHECK(vkCreateComputePipelines(vkdeviceutils::device, VK_NULL_HANDLE, 1, &computePipelineCInfo, nullptr, &pipeline.pipeline));
    vkDestroyShaderModule(vkdeviceutils::device, computeShader.module, nullptr);
    return pipeline;
}