#include "worldspace_health.h"

void WorldHealthManager::setup(VkDescriptorSetLayout& uiTextureSetLayout, VkDescriptorSetLayout& uniformSetLayout, uint32_t textureOffset, SWChainImageFormat& swFormat, VkFormat& depthFormat, VkSampleCountFlagBits& msaaSamples) {
	// setup UI pipeline
	worldUIPipelineUtil.addShader("shaders/worldUIVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	worldUIPipelineUtil.addShader("shaders/uiQuadFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	worldUIPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	worldUIPipelineUtil.setDefaultAttributes();
	worldUIPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	worldUIPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	worldUIPipelineUtil.setColorAttachmentFormat(swFormat.format, 1);
	worldUIPipelineUtil.setMultisampling(msaaSamples);
	worldUIPipelineUtil.enableBlending();
	worldUIPipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
	worldUIPipelineUtil.setDepthAttachmentFormat(depthFormat);
	worldUIPipelineUtil.numColorAttachments = 1;

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

	worldUIPipelineUtil.m_viewportState.pViewports = &viewport;
	worldUIPipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange pcRange;
	pcRange.offset = 0;
	pcRange.size = sizeof(worldUIPC);
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 2> sets = { uiTextureSetLayout, uniformSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.setLayoutCount = sets.size();
	pipelineLayoutCInfo.pSetLayouts = sets.data();
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &pcRange;

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &worldUIPipelineUtil.m_pipeline.layout));

	worldUIPipelineUtil.buildPipeline();

	healthBarTexInd = textureOffset;
}

void WorldHealthManager::update(std::vector<Entity>& entities, uint32_t selfID, Transform& camTransform) {
	numEntityHealthBars = entities.size();
	worldHealthBars.clear();

	for (const auto& e : entities) {
		if (e.id == selfID) {
			continue;
		}

		healthBarStruct h{};
		h.health = e.health;

		// for each entity, add a health bar on top of it
		h.worldTransform.position = e.transform.position + glm::vec3(0.f, UI_TOP_OFFSET, 0.f);
		glm::vec3 worldDirVec = glm::normalize(camTransform.position - h.worldTransform.position);
		h.worldTransform.yaw = glm::degrees(-glm::atan2(worldDirVec.x, worldDirVec.z));
		h.worldTransform.setRotationPitchYaw();
		h.worldTransform.scale.x = WORLD_SPACE_UI_WIDTH;
		h.worldTransform.scale.y = WORLD_SPACE_UI_WIDTH / WORLD_UI_TEXTURE_ASPECT;

		worldHealthBars.push_back(h);
	}
}

void WorldHealthManager::draw(VkCommandBuffer& cmd) {
	worldUIPC pc;
	pc.texIndex = healthBarTexInd;
	for (auto& h : worldHealthBars) {
		pc.worldUIMatrix = h.worldTransform.getMatrix();
		pc.health = h.health;

		vkCmdPushConstants(cmd, worldUIPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(worldUIPC), &pc);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
	}
}