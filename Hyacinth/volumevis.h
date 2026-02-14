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
	std::vector<VulkanBuffer> volumeTransformBuffers;

	void createVolumeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void update(std::vector<glm::mat4>& volumes, int frameIndex);
	void drawVolumes(VkCommandBuffer& cmd, VkDescriptorSet& descSet, int frameIndex);
	void destroy();
};