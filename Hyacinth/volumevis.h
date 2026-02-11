#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"

class volumeVisHelper {
	uint32_t volumeCount;
	
	VulkanPipelineBuilder pipelineUtil;
	
	uint32_t indexCount;
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	gltfObject boxObject;
	
	struct volumePushContant {
		VkDeviceAddress volumeTransformAddress;
	};
	
public:
	void createVolumeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawVolumes(VkCommandBuffer& cmd, VkDeviceAddress& volumeTranformAddress, VkDescriptorSet& descSet);
	void destroy();
};