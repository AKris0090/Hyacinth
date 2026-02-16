#include "probevis.h"

void probeVisObjects::createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VulkanImage& irradianceImage, VulkanImage& visibilityImage, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	auto spherePath = vkdebugutils::getExeDir() / "objects" / "sphere.glb";
	sphereObject = gltfutils::loadFromFile(spherePath.string(), false);
	gltfNode* node = sphereObject.nodes[0].get();
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
	vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_vis_vertex");
	indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_vis_index");

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


	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f }
	};
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		visSetLayout = layoutBuilder.buildLayout(nullptr, 0);
	}
	visDescriptorAllocator.initPool(1, sizes);
	visSet = visDescriptorAllocator.allocate(visSetLayout);

	vkdescriptorutils::queueWriteImage(visSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, irradianceImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::queueWriteImage(visSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, visibilityImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::flushDescriptorWrites();

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

	pipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
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
	bufferRange.size = sizeof(probeVisObjects::probeVisPushContant);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 2> setLayouts = { descSetLayout, visSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 2;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void probeVisObjects::drawProbes(VkCommandBuffer& cmd, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);

	std::array<VkDescriptorSet, 2> sets = { descSet, visSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, 2, sets.data(), 0, nullptr);

	probeVisObjects::probeVisPushContant pc{};
	pc.probePositionAddress = probePositionAddress;

	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeVisObjects::probeVisPushContant), &pc);

	vkCmdDrawIndexed(cmd, indexCount, probeCount, 0, 0, 0);
}

void probeVisObjects::destroy() {
	vkdeviceutils::destroyBuffer(vertexBuffer);
	vkdeviceutils::destroyBuffer(indexBuffer);
	pipelineUtil.destroyPipeline();
	visDescriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, visSetLayout, nullptr);
}