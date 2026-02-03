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
	std::vector<VkDescriptorSet> m_computeSets;
	DescriptorAllocator m_computeDescAlloc;

	VkDescriptorSetLayout m_computeLayout;
	VulkanPipeline m_computeCullPipeline;

public:
	void shutdown(DeviceContext& ctx);
	void setup(DeviceContext& ctx);
	void update(FPSCam::UniformPlanes& planes, int index);
	void executeCull(VkCommandBuffer& cmd, VkDeviceAddress& drawBufferAddress, VkDeviceAddress& bbAddress, VkDeviceAddress& matrixAddress, VkDeviceAddress& drawDataAddress, int index, uint32_t numDraws);
};