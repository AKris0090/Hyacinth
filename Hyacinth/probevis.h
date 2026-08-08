#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "unit_cube.h"
#include "fullscreen_quad.h"

class probeVisObjects {
	VulkanPipelineBuilder pipelineUtil;

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
		uint32_t volumeWidth;
		uint32_t volumeHeight;
		uint32_t volumeDepth;
	};

public:
	uint32_t probeCount;

	void createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkDescriptorSetLayout& irradianceVisSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& irradianceVisSet, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet, int currentVolumeProbeCount, int width, int height, int depth);
	void destroy();
};