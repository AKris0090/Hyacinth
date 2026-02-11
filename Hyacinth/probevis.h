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

	DescriptorLayoutBuilder			visLayoutBuilder{};
	DescriptorAllocator				visDescriptorAllocator{};

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
	};

public:
	uint32_t probeCount;
	VkDescriptorSetLayout visSetLayout;
	VkDescriptorSet visSet;

	void createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VulkanImage& irradianceImage, VulkanImage& visibilityImage, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet);
	void destroy();
};