#pragma once

#include "raytracing.h"
#include "vkimageutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"

const VkClearValue specularClearVal = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

struct SpecularPushConstant {
	VkDeviceAddress vertexBuffer;
	VkDeviceAddress indexBuffer;
	VkDeviceAddress renderCallBuffer;
	VkDeviceAddress materialBuffer;
	glm::vec3 cameraPos;
};

class SpecularTraceHelper {
private:
	std::vector<uint8_t> m_shaderHandles;
	VulkanBuffer m_sbtBuffer;
	VkStridedDeviceAddressRegionKHR raygenRegion{};
	VkStridedDeviceAddressRegionKHR missRegion{};
	VkStridedDeviceAddressRegionKHR hitRegion{};
	VkStridedDeviceAddressRegionKHR callableRegion{};

	rtHelper* m_rtHelper;
	VulkanPipeline rtPipeline;

	DescriptorAllocator	m_descriptorAllocator{};

	VkDescriptorSetLayout writeSpecularLayout;
	std::vector<VkDescriptorSet> writeSpecularSets;

	std::vector<VulkanImage> specularImages;

	void createRayTraceDescriptors(VulkanImage& skyboxImage);
	void createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);
	void createSpecularTracePipeline(VkDescriptorSetLayout& gBufferSetLayout, VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& textureSetLayout);

public:
	VkDescriptorSetLayout specularSamplerLayout;
	std::vector<VkDescriptorSet> specularSamplerSets;

	void setup(rtHelper* rtHelper, VkDescriptorSetLayout& gBufferSetLayout, VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& textureSetLayout, VulkanImage& skyboxImage, VkExtent2D swapExt);
	void resizeScreen(VkExtent2D swapExt);
	void traceScene(VkCommandBuffer& cmd, uint32_t swImageIndex, VkDeviceAddress& vbuff, VkDeviceAddress& iBuff, VkDeviceAddress& renderBuff, VkDeviceAddress& matBuff, VkDescriptorSet& gbuffSet, VkDescriptorSet& uniformSet, VkDescriptorSet& textureSet, glm::vec3 camPos, VkExtent2D swapChainExtent);
	void shutdown();
};