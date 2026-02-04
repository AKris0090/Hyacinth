#pragma once

#include "vkdeviceutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "fpcam.h"

class FrustumCullHelper {
private:
	struct ComputeCullPushConstant {
		VkDeviceAddress drawBufferAddress;
		VkDeviceAddress bbAddress;
		VkDeviceAddress matrixAddress;
		VkDeviceAddress drawDataAddress;
		uint32_t numDraws;
	};

	std::vector<VulkanBuffer> m_uniformPlaneBuffers;
	std::vector<void*> m_mappedUniformPlaneBuffers;
	DescriptorAllocator m_computeDescAlloc;

public:
	VulkanPipeline m_computeCullPipeline;
	std::vector<VkDescriptorSet> m_computeSets;
	VkDescriptorSetLayout m_computeLayout;

	void shutdown();
	void setup();
	void update(FPSCam::UniformPlanes& planes, int index);
	void executeCull(VkCommandBuffer& cmd, VkDescriptorSet& set, VkDeviceAddress& drawBufferAddress, VkDeviceAddress& bbAddress, VkDeviceAddress& matrixAddress, VkDeviceAddress& drawDataAddress, uint32_t numDraws);
};