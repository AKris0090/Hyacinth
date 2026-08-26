#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "unit_cube.h"
#include "fullscreen_quad.h"

class probeVisObjects {
private:
	VulkanPipelineBuilder pipelineUtil;

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
		uint32_t volumeWidth;
		uint32_t volumeHeight;
		uint32_t volumeDepth;
	};

public:
	uint32_t probeCount = 0;

	uint32_t probeIndexOffset = 0;
	uint32_t probeNumIndices = 0;
	uint32_t probeVertexOffset = 0;

	void createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkDescriptorSetLayout& irradianceVisSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples, uint32_t indexOffset, uint32_t vertexOffset, uint32_t numIndices);
	void drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& irradianceVisSet, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet, int currentVolumeProbeCount, int width, int height, int depth);
	void destroy();
};