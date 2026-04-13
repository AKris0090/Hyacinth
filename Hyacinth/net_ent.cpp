#include "net_ent.h"
#include "gltfutils.h"

void NetworkEntityManager::updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID) {
	for (const auto& e : p.entities) {
		if (e.id == currentClientID) {
			continue;
		}
		auto findit = entities.find(e.id);
		if (findit == entities.end()) {
			ids.push_back(e.id);
			entities[e.id] = new Entity();
			entities[e.id]->id = e.id;
			entityMutexes.emplace(e.id, std::make_unique<std::mutex>());
		}
		entityMutexes[e.id].get()->lock();
		entities[e.id]->transform.position = e.transform.position;
		entities[e.id]->transform.rotation = e.transform.rotation;
		entities[e.id]->transform.pitch = e.transform.pitch;
		entities[e.id]->transform.yaw = e.transform.yaw;
		entities[e.id]->transform.setRotationPitchYaw();
		entityMutexes[e.id].get()->unlock();
	}
}

void NetworkEntityManager::setupRenderingUtils() {
	auto spherePath = vkdebugutils::getExeDir() / "objects" / "cubeOrigin.glb";
	sphereObject = std::make_unique<gltfObject>(gltfutils::loadFromFile(spherePath.string(), false));
	gltfNode* node = sphereObject.get()->nodes[0].get();
	for (const auto& p : node->primitives) {
		for (const auto& v : p.get()->vertices) {
			Vertex upV = v;
			node->vertices.push_back(upV);
		}
		for (const auto& index : p.get()->indices) {
			node->indices.push_back(index);
		}
	}
	indexCount = static_cast<uint32_t>(node->indices.size());
	
	VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(Vertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "entity_vis_vertex");
	indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "entity_vis_index");
	
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
	pipelineUtil.addShader("shaders/netEntVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	pipelineUtil.addShader("shaders/netEntFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineUtil.setDefaultAttributes();
	pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineUtil.setColorAttachmentFormat(imageFormat.format, 1);
	pipelineUtil.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
	pipelineUtil.disableBlending();

	pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_LESS);
	pipelineUtil.setDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)imageFormat.extent.width;
	viewport.height = (float)imageFormat.extent.height;
	viewport.minDepth = 1.0f;
	viewport.maxDepth = 0.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = imageFormat.extent;

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
	bufferRange.size = sizeof(entityBufferPC);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 1> setLayouts = { *uniformSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &pipelineUtil.m_pipeline.layout));

	pipelineUtil.buildPipeline();
}

void NetworkEntityManager::setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID) {
	if (p.entities.size() > 0) {
		for (const auto& e : p.entities) {
			if (e.id == currentClientID) continue;
			Entity* newEnt = new Entity;
			newEnt->id = e.id;
			newEnt->transform.position = e.transform.position;
			newEnt->transform.rotation = e.transform.rotation;
			entities[e.id] = newEnt;
			entityMutexes.emplace(e.id, std::make_unique<std::mutex>());
			ids.push_back(e.id);
		}
	}
	
	// create entity position buffer
	size_t entityBufferSize = MAX_CONNECTIONS * sizeof(glm::mat4);
	entityPositionBuffer = vkdeviceutils::createBuffer(entityBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "entity_pos_buffer");

	setupRenderingUtils();
}

void NetworkEntityManager::update() {
	if (entities.size() > 0) {
		std::vector<glm::mat4> entityPositions;
		for (const auto& id : ids) {
			entityMutexes[id].get()->lock();
			entityPositions.push_back(entities[id]->transform.getMatrix());
			entityMutexes[id].get()->unlock();
		}
		size_t entityBufferSize = entityPositions.size() * sizeof(glm::mat4);
		memcpy(entityPositionBuffer.pMappedData, entityPositions.data(), entityBufferSize);
	}
}

void NetworkEntityManager::drawEntities(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);


	std::array<VkDescriptorSet, 1> sets = { uniformSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, sets.size(), sets.data(), 0, nullptr);

	entityBufferPC pc{};
	pc.entityPosBufferAddress = entityPositionBuffer.gpuAddress;

	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(entityBufferPC), &pc);

	vkCmdDrawIndexed(cmd, indexCount, entities.size(), 0, 0, 0);
}

void NetworkEntityManager::drawFPCharacter(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.pipeline);


	std::array<VkDescriptorSet, 1> sets = { uniformSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineUtil.m_pipeline.layout, 0, sets.size(), sets.data(), 0, nullptr);

	entityBufferPC pc{};
	pc.entityPosBufferAddress = entityPositionBuffer.gpuAddress;

	vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(entityBufferPC), &pc);

	vkCmdDrawIndexed(cmd, indexCount, entities.size(), 0, 0, 0);
}

void NetworkEntityManager::shutdown() {
	vkdeviceutils::destroyBuffer(entityPositionBuffer);
	vkdeviceutils::destroyBuffer(vertexBuffer);
	vkdeviceutils::destroyBuffer(indexBuffer);

	pipelineUtil.destroyPipeline();
}