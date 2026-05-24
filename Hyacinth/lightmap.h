#pragma once

#include "vkdebugutils.h"
#include "vkimageutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "raytracing.h"

constexpr int LIGHTMAP_SIZE = 4096;

struct PosNormPC {
	VkDeviceAddress transformAddress;
	VkDeviceAddress drawDataAddress;
};

struct lightPosPC {
	glm::vec4 lightPos;
};

class LightMapper {
private:
	rtHelper* m_rtHelper;

	VulkanBuffer                    m_sbtBuffer;
	std::vector<uint8_t>            m_shaderHandles;
	VkStridedDeviceAddressRegionKHR m_raygenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	VkStridedDeviceAddressRegionKHR m_callableRegion{};

	VkFormat						m_lightMapFormat = VK_FORMAT_R16_SFLOAT;
	VkFormat						m_posNormalFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VulkanPipeline					m_lightMapTracePipeline;
	VulkanPipelineBuilder			m_posNormalPipeline;
	DescriptorAllocator				m_descriptorAllocator{};
	VkDescriptorSetLayout			m_descriptorLayout{};
	VkDescriptorSet					m_lightMapDescriptorSet;

	VkExtent2D						m_lightMapExtent;

	void createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);
	void createLightMapPipelines();
	void createLightMapDescriptors();

public:
	VulkanImage m_lightMapImage;

	VulkanImage m_worldPosImage;
	VulkanImage m_normalImage;

	void setup(rtHelper* rtHelper);
	void bakeLightMap(VkBuffer& staticDrawBuffer, uint32_t numStaticDraws, VkDeviceAddress& drawDataAddress, VkDeviceAddress& transformAddress, VkBuffer& vertexBuffer, VkBuffer& indexBuffer);
};