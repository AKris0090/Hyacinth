#include "volumevis.h"

void volumeVisHelper::createVolumeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	// create probe vis pipeline
	pipelineUtil.addShader("shaders/volumeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	pipelineUtil.addShader("shaders/volumeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineUtil.setDefaultAttributes();
	pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineUtil.setColorAttachmentFormat(SWImageFormat.format, 1);
	pipelineUtil.setMultisampling(msaaSamples);
	pipelineUtil.enableBlending();

	pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_ALWAYS);
	pipelineUtil.setDepthAttachmentFormat(depthFormat);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)SWImageFormat.extent.width;
	viewport.height = (float)SWImageFormat.extent.height;
	viewport.minDepth = 1.0f;
	viewport.maxDepth = 0.0f;

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
	bufferRange.size = sizeof(volumePushContant);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayout, 1> setLayouts = { descSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void volumeVisHelper::update(std::vector<glm::mat4>& volumeMatrices, int frameIndex) {
	memcpy(volumeTransformBuffers[frameIndex].pMappedData, volumeMatrices.data(), volumeMatrices.size() * sizeof(glm::mat4));
}

void volumeVisHelper::drawVolumes(VkCommandBuffer& cmd, VkDescriptorSet& descSet, int frameIndex, int numVolumes) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);

	std::array<VkDescriptorSet, 1> sets = { descSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, 1, sets.data(), 0, nullptr);
	for (int i = 0; i < numVolumes; i++) {
		volumePushContant pc{};
		pc.volumeTransformAddress = volumeTransformBuffers[frameIndex].gpuAddress;
		pc.volumeIndex = i;
		vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(volumePushContant), &pc);

		vkCmdDrawIndexed(cmd, UNIT_CUBE_INDEX_COUNT, 1, QUAD_INDEX_COUNT, QUAD_VERTEX_COUNT, 0);
	}
}

void volumeVisHelper::destroy() {
	vkDestroyPipeline(vkdeviceutils::device, pipelineUtil.m_pipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(vkdeviceutils::device, pipelineUtil.m_pipeline.layout, nullptr);
}