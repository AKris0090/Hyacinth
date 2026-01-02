#include "vkpipelineutils.h"
#include "vkdebugutils.h"

void VulkanPipelineBuilder::reset() {
    m_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_colorBlendAttachment = {};
    m_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	m_viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

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

void VulkanPipelineBuilder::buildPipeline(VkDevice& dev, VkRenderPass& renderPass) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VK_CHECK(vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipeline.layout));

    m_viewportState.viewportCount = 1;
    m_viewportState.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_colorBlendAttachment;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.pNext = &m_renderInfo;
    pipelineInfo.stageCount = (uint32_t)m_shaderStages.size();
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &m_viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.layout = m_pipeline.layout;
	pipelineInfo.renderPass = renderPass;

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