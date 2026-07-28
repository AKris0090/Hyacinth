#pragma once

#include "vkpipelineutils.h"
#include <array>

enum UI_ANCHOR {
	TOP_LEFT,
	TOP_MIDDLE,
	TOP_RIGHT,
	MIDDLE_LEFT,
	MIDDLE_MIDDLE,
	MIDDLE_RIGHT,
	BOTTOM_LEFT,
	BOTTOM_MIDDLE,
	BOTTOM_RIGHT
};

struct UIElement {
	glm::vec2 dimensions;
	glm::vec2 offset;
	UI_ANCHOR anchorPos;
	uint32_t texIndex;
	bool active;
	bool isFlash = false;
};

struct UIGPUUnit {
	glm::vec2 origin;
	glm::vec2 dimensions;
	glm::vec4 flashAmntXYApply;
	uint32_t texIndex;
	uint32_t _pad[3];
};

class HyacinthUIManager {
private:
	std::vector<UIElement> elements;
	std::vector<UIGPUUnit> uiUnits;
	VulkanBuffer uiUnitStorageBuffer;
	glm::vec2 ss;

	float flashNDCX, flashNDCY;

	void createUIElements(uint32_t textureOffset, glm::vec2 screenSize);
	UIGPUUnit calculateUIPosition(UIElement& e, glm::vec2 screenSize);
public:
	VulkanPipelineBuilder uiPipelineUtil;

	void setup(VkDescriptorSetLayout& uiTextureSetLayout, uint32_t textureOffset, glm::vec2 screenSize, SWChainImageFormat& swFormat, VkSampleCountFlagBits& msaaSamples);
	void onresize(uint32_t textureOffset, glm::vec2 newScreenSize);
	void update(int ammoDisplay, float flashPercentage, float ndcX, float ndcY);
	void draw(VkCommandBuffer& cmd);
	void shutdown();
};

