#pragma once

#include <vector>
#include "vkpipelineutils.h"
#include "vkimageutils.h"
#include "vkswapchainutils.h"
#include "vkdeviceutils.h"
#include "vkdescriptorutils.h"
#include "fullscreen_quad.h"
#include <random>

constexpr int NUM_SAMPLES = 64;

struct AmbientPC {
	glm::vec4 samples[64];
	glm::vec2 screenSize;
};

class AmbientHelper {
private:
	VulkanPipelineBuilder m_ambientPipelineUtil;
	VulkanImage noiseTexture;
	VulkanBuffer samplesUBO;

	DescriptorAllocator	descriptorAllocator{};
	VkDescriptorSetLayout noiseLayout;
	VkDescriptorSet noiseSet;

public:
	std::vector<glm::vec4> ssaoKernel;
	std::vector<glm::vec4> ssaoNoise;

	void setup(VkDescriptorSetLayout& uboLayout, VkDescriptorSetLayout& compLayout, SWChainImageFormat& swapchainFormat);
	void drawAO(VkCommandBuffer& cmd, VkDescriptorSet& compSet, VkDescriptorSet& uboSet, VulkanImage ambientImage, SWChainImageFormat& swapchainFormat);
	void shutdown();
};