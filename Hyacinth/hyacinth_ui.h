#pragma once

#include "vkpipelineutils.h"
#include <array>

const std::array<std::string, 3> UI_TEXTURE_NAMES = {
	"crosshair.png",
	"characterportrait.png",
	"bullet.png"
};

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
};

struct UIGPUUnit {
	glm::vec2 origin;
	glm::vec2 dimensions;
	uint32_t texIndex;
	uint32_t _pad[1];
};

class HyacinthUIManager {
private:
	std::vector<UIElement> elements;
	VulkanBuffer uiUnitStorageBuffer;

	UIGPUUnit calculateUIPosition(UIElement& e, glm::vec2 screenSize);
public:
	VulkanPipelineBuilder uiPipelineUtil;

	void setup(VkDescriptorSetLayout& uiTextureSetLayout, uint32_t textureOffset, glm::vec2 screenSize, SWChainImageFormat& swFormat, VkSampleCountFlagBits& msaaSamples);
	void update(int ammoDisplay);
	void draw(VkCommandBuffer& cmd);
	void shutdown();
};

