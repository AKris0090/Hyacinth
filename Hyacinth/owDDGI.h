#pragma once

#include "raytracing.h"
#include "probevis.h"
#include "volume_vis.h"
#include "ecshelpers.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "glm/gtx/string_cast.hpp"
#include <array>

constexpr int PROBE_DENSITY_WIDTH = 30;  // x
constexpr int PROBE_DENSITY_HEIGHT = 14;  // y
constexpr int PROBE_DENSITY_DEPTH = 20;  // z

constexpr int RAYS_PER_PROBE = 20000;

constexpr int IRRADIANCE_PIXEL_COUNT = 8;
constexpr int VISIBILITY_PIXEL_COUNT = 16;

struct DDGIVolume {
	Transform transform;
	std::vector<std::vector<std::vector<glm::vec3>>> probes;
	VulkanBuffer probePositionBuffer;

	VulkanImage rayDataImage;
	VulkanImage irradianceImage;
	VulkanImage visibilityImage;
};

struct ddgiPushConstant {
	VkDeviceAddress probePositionBufferAddress;
	VkDeviceAddress vertexAddress;
	VkDeviceAddress indexAddress;
};

struct DDGIVertex {
	glm::vec4 pos;
	glm::vec4 normal;
};

class owDDGI {
private:
	rtHelper* m_rtHelper;

	VkFormat m_irradFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat m_depthFormat = VK_FORMAT_R16G16_SFLOAT;

	uint32_t numProbes;

	VulkanBuffer                    m_sbtBuffer;
	std::vector<uint8_t>            m_shaderHandles;
	VkStridedDeviceAddressRegionKHR m_raygenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	VkStridedDeviceAddressRegionKHR m_callableRegion{};

	DescriptorAllocator				m_descriptorAllocator{};
	VkDescriptorSetLayout			m_descriptorLayout{};
	VkDescriptorSet					m_rtDescriptorSet{};

	VulkanPipeline					m_rtPipeline				{};
	VulkanPipeline					m_irradianceComputePipeline	{};
	VulkanPipeline					m_visibilityComputePipeline	{};

	VkDescriptorSetLayout			m_computeDescriptorLayout{};
	VkDescriptorSet					m_computeDescriptorSet{};

	VulkanBuffer closestHitVertexBuffer;
	VulkanBuffer closestHitIndexBuffer;

	void createRaytraceDescriptors();
	void createRaytracePipeline();
	void createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

public:
	DDGIVolume m_probeVolume;
	probeVisObjects	m_probeVis{};
	volumeVisHelper m_volumeVis;
	VulkanBuffer volumeTransformBuffer;
	bool showProbes = false;
	bool showVolumes = true;

	void setup(rtHelper* rtHelper, SceneGraph& m_scene);
	void bakeDDGI(VkDescriptorSet& textureSet);
	void shutdown();
};