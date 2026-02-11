#pragma once

#include "vkdeviceutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "fpcam.h"

struct ComputeCullPushConstant {
	VkDeviceAddress drawBufferAddress;
	VkDeviceAddress bbAddress;
	VkDeviceAddress matrixAddress;
	VkDeviceAddress drawDataAddress;
	uint32_t numDraws;
};

class FrustumCullHelper {  
private:
	VulkanBuffer m_uniformPlaneBuffers[MAX_FRAMES_IN_FLIGHT];
	DescriptorAllocator m_computeDescAlloc;

public:
	VulkanPipeline m_computeCullPipeline;
	std::vector<VkDescriptorSet> m_computeSets;
	VkDescriptorSetLayout m_computeLayout;

	void shutdown();
	void setup();
	void update(CameraFrustumPlanes& planes, int index);
	void executeCull(VkCommandBuffer& cmd, VkDescriptorSet& set, VkDeviceAddress& drawBufferAddress, VkDeviceAddress& bbAddress, VkDeviceAddress& matrixAddress, VkDeviceAddress& drawDataAddress, uint32_t numDraws);
};