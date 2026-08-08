#include "probevis.h"

void probeVisObjects::createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkDescriptorSetLayout& irradianceVisSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	// create probe vis pipeline
	pipelineUtil.addShader("shaders/probeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	pipelineUtil.addShader("shaders/probeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineUtil.setDefaultAttributes();
	pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineUtil.setColorAttachmentFormat(SWImageFormat.format, 1);
	pipelineUtil.setMultisampling(msaaSamples);
	pipelineUtil.disableBlending();

	pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_LESS);
	pipelineUtil.setDepthAttachmentFormat(depthFormat);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)SWImageFormat.extent.width;
	viewport.height = (float)SWImageFormat.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = SWImageFormat.extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipelineUtil.m_viewportState.pViewports = &viewport;
	pipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(probeVisObjects::probeVisPushContant);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayout, 2> setLayouts = { descSetLayout, irradianceVisSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 2;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void probeVisObjects::drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& irradianceVisSet, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet, int currentVolumeProbeCount, int width, int height, int depth) {
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);

	std::array<VkDescriptorSet, 2> sets = { descSet, irradianceVisSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

	probeVisObjects::probeVisPushContant pc{};
	pc.probePositionAddress = probePositionAddress;
	pc.volumeWidth = width;
	pc.volumeHeight = height;
	pc.volumeDepth = depth;  

	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeVisObjects::probeVisPushContant), &pc);

	vkCmdDrawIndexed(cmd, UNIT_CUBE_INDEX_COUNT, currentVolumeProbeCount, QUAD_INDEX_COUNT, QUAD_VERTEX_COUNT, 0);
}

void probeVisObjects::destroy() {
	pipelineUtil.destroyPipeline();
}