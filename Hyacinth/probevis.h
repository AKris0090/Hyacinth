#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"

struct probeVisObjects {
	uint32_t probeCount;

	VulkanPipelineBuilder pipelineUtil;
	uint32_t indexCount;

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	gltfObject sphereObject;

	VkDescriptorSetLayout visSetLayout;
	VkDescriptorSet visSet;
	DescriptorLayoutBuilder			visLayoutBuilder{};
	DescriptorAllocator				visDescriptorAllocator{};

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
	};

	void createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VulkanImage& irradianceImage, VulkanImage& visibilityImage, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDeviceAddress& probePositionAddress, VkDescriptorSet& descSet);
};