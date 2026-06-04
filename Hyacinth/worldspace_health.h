#pragma once

#include "vkpipelineutils.h"
#include "entity.h"
#include <array>

const std::array<std::string, 1> WORLD_UI_TEXTURE_NAMES = {
	"healthbar.png"
};

constexpr float WORLD_SPACE_UI_WIDTH = 0.75f;
constexpr float UI_TOP_OFFSET = 2.1f;
constexpr float WORLD_UI_TEXTURE_ASPECT = 342.f / 36.f;

struct worldUIPC {
	glm::mat4 worldUIMatrix;
	uint32_t texIndex = 0;
};

struct healthBarStruct {
	Transform worldTransform;
};

class WorldHealthManager {
private:
	VulkanBuffer healthBarWorldMatrices;
	std::vector<healthBarStruct> worldHealthBars;
	uint32_t healthBarTexInd = 0;

public:
	uint32_t numEntityHealthBars = 0;
	VulkanPipelineBuilder worldUIPipelineUtil;

	void setup(VkDescriptorSetLayout& uiTextureSetLayout, VkDescriptorSetLayout& uniformSetLayout, uint32_t textureOffset, SWChainImageFormat& swFormat, VkFormat& depthFormat, VkSampleCountFlagBits& msaaSamples);
	void update(std::vector<Entity>& entities, uint32_t selfID, Transform& camTransform);
	void draw(VkCommandBuffer& cmd);
	void shutdown();
};