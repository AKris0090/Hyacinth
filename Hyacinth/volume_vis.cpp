#include "volume_vis.h"

void volumeVisHelper::createVolumeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	auto cubePath = vkdebugutils::getExeDir() / "objects" / "cube.glb";
	boxObject = gltfutils::loadFromFile(cubePath.string(), false);
	gltfNode* node = boxObject.nodes[0].get();
	for (const auto& p : node->primitives) {
		for (const auto& v : p.get()->vertices) {
			node->vertices.push_back(v);
		}
		for (const auto& index : p.get()->indices) {
			node->indices.push_back(index);
		}
	}
	indexCount = static_cast<uint32_t>(node->indices.size());

	VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(Vertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "volume_vis_vertex");
	indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "volume_vis_index");

	VulkanBuffer staging = vkdeviceutils::createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(staging.info.pMappedData, node->vertices.data(), vertexBufferSize);
	memcpy((char*)staging.info.pMappedData + vertexBufferSize, node->indices.data(), indexBufferSize);

	VkBufferCopy vertexCopyRegion{};
	vertexCopyRegion.srcOffset = 0;
	vertexCopyRegion.dstOffset = 0;
	vertexCopyRegion.size = vertexBufferSize;

	VkBufferCopy indexCopyRegion{};
	indexCopyRegion.srcOffset = vertexBufferSize;
	indexCopyRegion.dstOffset = 0;
	indexCopyRegion.size = indexBufferSize;

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(staging);

	// create probe vis pipeline
	pipelineUtil.addShader("shaders/volumeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	pipelineUtil.addShader("shaders/volumeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineUtil.setDefaultAttributes();
	pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineUtil.setColorAttachmentFormat(SWImageFormat.format);
	pipelineUtil.setMultisampling(msaaSamples);
	pipelineUtil.enableBlending();

	pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_LESS);
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
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 1> setLayouts = { descSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void volumeVisHelper::drawVolumes(VkCommandBuffer& cmd, VkDeviceAddress& volumeTransformAddress, VkDescriptorSet& descSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);

	std::array<VkDescriptorSet, 1> sets = { descSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, 1, sets.data(), 0, nullptr);

	volumePushContant pc{};
	pc.volumeTransformAddress = volumeTransformAddress;

	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(volumePushContant), &pc);

	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void volumeVisHelper::destroy() {
	vkdeviceutils::destroyBuffer(vertexBuffer);
	vkdeviceutils::destroyBuffer(indexBuffer);
	vkDestroyPipeline(vkdeviceutils::device, pipelineUtil.m_pipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(vkdeviceutils::device, pipelineUtil.m_pipeline.layout, nullptr);
}