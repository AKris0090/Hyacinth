#include "hyacinth_ui.h"

// screensize is <width, height>
// u.dimensions should be the ratio, in screensize, converting from screenSize to the 1-1
// screen space coordinates go from -1 to 1, map to that
UIGPUUnit HyacinthUIManager::calculateUIPosition(UIElement& e, glm::vec2 screenSize) {
	UIGPUUnit u;
	u.texIndex = e.texIndex;
	u.dimensions = e.dimensions / screenSize;
	glm::vec2 ssOffset = ((e.offset / screenSize) * 2.f) - 1.f;
	switch (e.anchorPos) {
// 	case TOP_LEFT:
// 		u.origin = ssOffset;
// 		break;
// 	case TOP_MIDDLE:
// 		u.origin = ssOffset - glm::vec2(u.dimensions.x / 2.f, 0);
// 		break;
// 	case TOP_RIGHT:
// 		u.origin = ssOffset - glm::vec2(u.dimensions.x, 0);
// 		break;
	case BOTTOM_LEFT:
		u.origin = ssOffset + glm::vec2(u.dimensions.x, -u.dimensions.y);
		break;
// 	case BOTTOM_MIDDLE:
// 		u.origin = ssOffset - glm::vec2(u.dimensions.x / 2.f, u.dimensions.y);
// 		break;
// 	case BOTTOM_RIGHT:
// 		u.origin = ssOffset - glm::vec2(u.dimensions.x, u.dimensions.y);
// 		break;
// 	case MIDDLE_LEFT:
// 		u.origin = ssOffset + glm::vec2(u.dimensions.x / 2.f, 0);
// 		break;
 	case MIDDLE_MIDDLE:
 		u.origin = ssOffset;
 		break;
// 	case MIDDLE_RIGHT:
// 		u.origin = ssOffset - glm::vec2(u.dimensions.x / 2.f, 0);
// 		break;
	}
	return u;
}

void HyacinthUIManager::createUIElements(float textureOffset, glm::vec2 screenSize) {
	elements.clear();

	// crosshair
	UIElement cH;
	cH.active = true;
	cH.anchorPos = MIDDLE_MIDDLE;
	cH.dimensions = glm::vec2(32, 32);
	cH.offset = glm::vec2(screenSize.x / 2.f, screenSize.y / 2.f);
	cH.texIndex = textureOffset;
	elements.push_back(cH);

	UIElement port;
	port.active = true;
	port.anchorPos = BOTTOM_LEFT;
	port.dimensions = glm::vec2(384, 96);
	port.offset = glm::vec2(20.f, screenSize.y - 20.f);
	port.texIndex = textureOffset + 1;
	elements.push_back(port);

	// bullet array
	glm::vec2 spacing = glm::vec2(21.f, 0.f);
	glm::vec2 startBottom = glm::vec2(130.f, screenSize.y - 55.f);
	for (int i = 0; i < 10; i++) {
		UIElement bullet;
		bullet.active = true;
		bullet.anchorPos = BOTTOM_LEFT;
		bullet.dimensions = glm::vec2(32, 64);
		bullet.offset = startBottom + (spacing * (float)i);
		bullet.texIndex = textureOffset + 2;
		elements.push_back(bullet);
	}
}

void HyacinthUIManager::setup(VkDescriptorSetLayout& uiTextureSetLayout, uint32_t textureOffset, glm::vec2 screenSize, SWChainImageFormat& swFormat, VkSampleCountFlagBits& msaaSamples) {
	// setup UI pipeline
	uiPipelineUtil.addShader("shaders/uiQuadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	uiPipelineUtil.addShader("shaders/uiQuadFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	uiPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	uiPipelineUtil.setDefaultAttributes();
	uiPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	uiPipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	uiPipelineUtil.setColorAttachmentFormat(swFormat.format, 1);
	uiPipelineUtil.setMultisampling(msaaSamples);
	uiPipelineUtil.enableBlending();

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

	uiPipelineUtil.m_viewportState.pViewports = &viewport;
	uiPipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange pcRange;
	pcRange.offset = 0;
	pcRange.size = sizeof(VkDeviceAddress);
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 1> sets = { uiTextureSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.setLayoutCount = sets.size();
	pipelineLayoutCInfo.pSetLayouts = sets.data();
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &pcRange;

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &uiPipelineUtil.m_pipeline.layout));

	uiPipelineUtil.buildPipeline();

	// add UI elements

	// crosshair
	UIElement cH;
	cH.active = true;
	cH.anchorPos = MIDDLE_MIDDLE;
	cH.dimensions = glm::vec2(32, 32);
	cH.offset = glm::vec2(screenSize.x / 2.f, screenSize.y / 2.f);
	cH.texIndex = textureOffset;
	elements.push_back(cH);

	UIElement port;
	port.active = true;
	port.anchorPos = BOTTOM_LEFT;
	port.dimensions = glm::vec2(384, 96);
	port.offset = glm::vec2(20.f, screenSize.y - 20.f);
	port.texIndex = textureOffset + 1;
	elements.push_back(port);

	// bullet array
	glm::vec2 spacing = glm::vec2(21.f, 0.f);
	glm::vec2 startBottom = glm::vec2(130.f, screenSize.y - 55.f);
	for (int i = 0; i < 10; i++) {
		UIElement bullet;
		bullet.active = true;
		bullet.anchorPos = BOTTOM_LEFT;
		bullet.dimensions = glm::vec2(32, 64);
		bullet.offset = startBottom + (spacing * (float)i);
		bullet.texIndex = textureOffset + 2;
		elements.push_back(bullet);
	}

	// create UI buffer
	uiUnitStorageBuffer = vkdeviceutils::createBuffer(elements.size() * sizeof(UIGPUUnit), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "ui_storage_ssbo");

	// populate buffer
	std::vector<UIGPUUnit> units;
	for (auto& e : elements) {
		units.push_back(calculateUIPosition(e, screenSize));
	}
	memcpy(uiUnitStorageBuffer.pMappedData, units.data(), elements.size() * sizeof(UIGPUUnit));
}

void HyacinthUIManager::onresize(float textureOffset, glm::vec2 newScreenSize) {
	createUIElements(textureOffset, newScreenSize);

	std::vector<UIGPUUnit> units;
	for (auto& e : elements) {
		units.push_back(calculateUIPosition(e, newScreenSize));
	}
	memcpy(uiUnitStorageBuffer.pMappedData, units.data(), elements.size() * sizeof(UIGPUUnit));
}

void HyacinthUIManager::update(int ammoDisplay) {
	int used = ammoDisplay;
	for (int i = 2; i < 12; i++) {
		elements[i].active = used > 0;
		used--;
	}
}

void HyacinthUIManager::draw(VkCommandBuffer& cmd) {
	int numUIElements = 0;
	for (const auto& e : elements) {
		if (e.active) numUIElements++;
	}
	vkCmdPushConstants(cmd, uiPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &uiUnitStorageBuffer.gpuAddress);
	vkCmdDrawIndexed(cmd, 6, numUIElements, 0, 0, 0);
}

void HyacinthUIManager::shutdown() {
	uiPipelineUtil.destroyPipeline();
	vkdeviceutils::destroyBuffer(uiUnitStorageBuffer);
}