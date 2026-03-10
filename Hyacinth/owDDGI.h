#pragma once

#include "raytracing.h"
#include "probevis.h"
#include "volumevis.h"
#include "transform.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "glm/gtx/string_cast.hpp"
#include <array>

constexpr int PROBE_A_DENSITY_WIDTH		= 20;  // x 20
constexpr int PROBE_A_DENSITY_HEIGHT	= 10;  // y 14
constexpr int PROBE_A_DENSITY_DEPTH		= 20;  // z 20

constexpr int PROBE_B_DENSITY_WIDTH = 15;  // x 30
constexpr int PROBE_B_DENSITY_HEIGHT = 7;  // y 14
constexpr int PROBE_B_DENSITY_DEPTH = 7;  // z 20

constexpr int IRRADIANCE_PIXEL_COUNT = 8;
constexpr int VISIBILITY_PIXEL_COUNT = 16;

struct VolumeData {
	int densityWidth;
	int densityHeight;
	int densityDepth;
	int padding;
	glm::vec4 pos;				// pos.w is normalBias
	glm::vec4 spacing;			// spacing.w is viewBias
	glm::vec4 inverseSpacing;	// inverseSpacing.w is number of rays
};

struct DDGIVolume {
	Transform transform;
	std::vector<std::vector<std::vector<glm::vec3>>> probes;
	uint32_t totalNumProbes;
	VulkanBuffer probePositionBuffer;
	VolumeData data;

	VulkanImage rayDataImage;
	VulkanImage irradianceImage;
	VulkanImage visibilityImage;

	VkDescriptorSet rayDataDescriptorSet;
	VkDescriptorSet computeBuildDescriptorSet;
	VkDescriptorSet irradianceVisSet;

	VulkanBuffer probeOffsetBuffer;
};

struct ComputePushConstant {
	VkDeviceAddress volumeDataAddress;
	uint32_t volumeIndex;
};

struct ddgiPushConstant {
	VkDeviceAddress probePositionBufferAddress;
	VkDeviceAddress vertexAddress;
	VkDeviceAddress indexAddress;
	VkDeviceAddress volumeDataAddress;
	uint32_t volumeIndex;
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

	VulkanPipeline					m_rtPipeline				{};
	VulkanPipeline					m_irradianceComputePipeline	{};
	VulkanPipeline					m_visibilityComputePipeline	{};

	VkDescriptorSetLayout			m_computeDescriptorLayout{};

	VulkanBuffer closestHitVertexBuffer;
	VulkanBuffer closestHitIndexBuffer;

	void createRaytraceDescriptors();
	void createRaytracePipeline();
	void createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

	void addVolume(glm::vec3 pos, glm::vec3 scale, uint32_t densityWidth, uint32_t densityDepth, uint32_t densityHeight, float viewBias, float normalBias);

public:
	std::vector<DDGIVolume> m_probeVolumes;
	probeVisObjects	m_probeVis{};
	volumeVisHelper m_volumeVis;
	VulkanBuffer volumeDataBuffer;

	VkDescriptorSetLayout m_irradianceVisSetLayout;

	bool showProbes = false;

	bool showProbesA = false;
	bool showProbesB = false;

	bool showVolumes = false;

	void setup(rtHelper* rtHelper, SceneGraph& m_scene);
	void bakeDDGI(VkDescriptorSet& textureSet);
	void shutdown();
};