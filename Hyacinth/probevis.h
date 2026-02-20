#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"

class probeVisObjects {

	VulkanPipelineBuilder pipelineUtil;
	uint32_t indexCount;

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	gltfObject sphereObject;

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
		uint32_t volumeWidth;
		uint32_t volumeDepth;
	};

public:
	uint32_t probeCount;

	void createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkDescriptorSetLayout& irradianceVisSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& irradianceVisSet, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet, int currentVolumeProbeCount, int width, int depth);
	void destroy();
};