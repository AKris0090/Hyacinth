#pragma once

#include "raytracing.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "unit_cube.h"

class volumeVisHelper {
	uint32_t volumeCount;
	
	VulkanPipelineBuilder pipelineUtil;

	gltfObject boxObject;
	
	struct volumePushContant {
		VkDeviceAddress volumeTransformAddress;
		uint32_t volumeIndex;
	};
	
public:
	std::vector<VulkanBuffer> volumeTransformBuffers;

	void createVolumeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void update(std::vector<glm::mat4>& volumes, int frameIndex);
	void drawVolumes(VkCommandBuffer& cmd, VkDescriptorSet& descSet, int frameIndex, int numVolumes);
	void destroy();
};