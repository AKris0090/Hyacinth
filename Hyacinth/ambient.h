#pragma once

#include <vector>
#include "vkpipelineutils.h"
#include "vkimageutils.h"
#include "vkswapchainutils.h"
#include "vkdeviceutils.h"
#include "vkdescriptorutils.h"
#include "fullscreen_quad.h"
#include <random>

constexpr int NUM_SAMPLES = 16;

struct AmbientPC {
	glm::mat4 inverseProj;
	glm::vec4 samples[NUM_SAMPLES];
	glm::vec2 screenSize;
};

class AmbientHelper {
private:
	VulkanPipelineBuilder m_ambientPipelineUtil;
	VulkanImage noiseTexture;
	std::vector<VulkanBuffer> samplesUBO;

	DescriptorAllocator	descriptorAllocator{};
	VkDescriptorSetLayout noiseLayout;
	std::vector<VkDescriptorSet> noiseSet;

public:
	std::vector<glm::vec4> ssaoKernel;
	std::vector<glm::vec4> ssaoNoise;

	void setup(VkDescriptorSetLayout& uboLayout, VkDescriptorSetLayout& compLayout, SWChainImageFormat& swapchainFormat, glm::mat4 camProj);
	void update(uint32_t imageIndex, SWChainImageFormat& swapchainFormat, glm::mat4 camProj);
	void drawAO(VkCommandBuffer& cmd, uint32_t imageIndex, VkDescriptorSet& compSet, VkDescriptorSet& uboSet, VulkanImage ambientImage, SWChainImageFormat& swapchainFormat);
	void shutdown();
};