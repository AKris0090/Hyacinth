#include "netDebugRenderer.h"

void NetDebugRenderer::setup(SWChainImageFormat& swFormat, VkSampleCountFlagBits msaaSamples, VkDescriptorSetLayout& uniformSetLayout) {
	auto spherePath = vkdebugutils::getExeDir() / "objects" / "capsule.glb";
	sphereObject = gltfutils::loadFromFile(spherePath.string(), false);
	gltfNode* node = sphereObject.allNodes[0];
	for (const auto& p : node->primitives) {
		for (const auto& v : p->vertices) {
			node->vertices.push_back(v);
		}
		for (const auto& index : p->indices) {
			node->indices.push_back(index);
		}
	}
	indexCount = static_cast<uint32_t>(node->indices.size());

	VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(Vertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "net_debug_vertex");
	indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "net_debug_index");

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
	pipelineUtil.addShader("shaders/netDebugVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	pipelineUtil.addShader("shaders/netDebugFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineUtil.setDefaultAttributes();
	pipelineUtil.setPolygonMode(VK_POLYGON_MODE_LINE);
	pipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineUtil.setColorAttachmentFormat(swFormat.format, 1);
	pipelineUtil.setMultisampling(msaaSamples);
	pipelineUtil.disableBlending();

	pipelineUtil.m_rasterizer.lineWidth = 1.0;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swFormat.extent.width;
	viewport.height = (float)swFormat.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swFormat.extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipelineUtil.m_viewportState.pViewports = &viewport;
	pipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange pcRange{};
	pcRange.offset = 0;
	pcRange.size = sizeof(pCNetDebug);
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayout, 1> setLayouts = { uniformSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &pcRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void NetDebugRenderer::draw(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	std::array<VkDescriptorSet, 1> descSet = { uniformSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, descSet.size(), descSet.data(), 0, nullptr);

	pCNetDebug p;

	// draw client position in blue
	p.pos = clientEntityPosition;
	p.color = glm::vec4(0.f, 0.f, 1.f, 1.f);
	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pCNetDebug), &p);
	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

	// draw server position in red
	p.pos = serverEntityPosition;
	p.color = glm::vec4(1.f, 0.f, 0.f, 1.f);
	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pCNetDebug), &p);
	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void NetDebugRenderer::shutdown() {
	pipelineUtil.destroyPipeline();

	vkdeviceutils::destroyBuffer(vertexBuffer);
	vkdeviceutils::destroyBuffer(indexBuffer);
}