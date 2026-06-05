#pragma once

#include "vkpipelineutils.h"
#include "gltfutils.h"

struct pCNetDebug {
	glm::vec4 pos;
	glm::vec4 color;
};

class NetDebugRenderer {
private:
	gltfObject sphereObject;
	uint32_t indexCount;

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;

public:
	VulkanPipelineBuilder pipelineUtil;

	glm::vec4 clientEntityPosition = glm::vec4(0.f, -100.f, 0.f, 1.f);
	glm::vec4 serverEntityPosition = glm::vec4(0.f, -100.f, 0.f, 1.f);

	void setup(SWChainImageFormat& swFormat, VkSampleCountFlagBits msaaSamples, VkDescriptorSetLayout& uniformSetLayout);
	void draw(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void shutdown();
};