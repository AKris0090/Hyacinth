#pragma once

#include "raytracing.h"
#include "ecshelpers.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include "glm/gtx/string_cast.hpp"
#include <array>

constexpr int PROBE_DENSITY_WIDTH = 30;  // x
constexpr int PROBE_DENSITY_HEIGHT = 14;  // y
constexpr int PROBE_DENSITY_DEPTH = 20;  // z

constexpr int RAYS_PER_PROBE = 250;

constexpr int IRRADIANCE_PIXEL_COUNT = 8;
constexpr int VISIBILITY_PIXEL_COUNT = 16;

struct DDGIVolume {
	transform transform;
	std::vector<std::vector<std::vector<glm::vec3>>> probes;
	VulkanBuffer probePositionBuffer;

	VulkanImage rayDataImage;
	VulkanImage irradianceImage;
	VulkanImage visibilityImage;

	VkDeviceAddress materialIndexAddress;
};

struct ddgiPushConstant {
	VkDeviceAddress probePositionBufferAddress;
	VkDeviceAddress vertexAddress;
	VkDeviceAddress indexAddress;
	VkDeviceAddress materialIndexAddress;
};

struct DDGIVertex {
	glm::vec4 pos;
	glm::vec4 normal;
};

struct probeVisObjects {
	VulkanPipelineBuilder pipelineUtil;
	uint32_t indexCount;

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	gltfObject sphereObject;

	VkDescriptorSetLayout visSetLayout;
	VkDescriptorSet visSet;
	DescriptorLayoutBuilder			visLayoutBuilder{};
	DescriptorAllocator				visDescriptorAllocator{};

	struct probeVisPushContant {
		VkDeviceAddress probePositionAddress;
	};
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
	VkPipelineLayout				m_rtPipelineLayout{};
	VkPipeline						m_rtPipeline{};
	
	VkPipelineLayout				m_irradianceComputePipelineLayout{};
	VkPipeline						m_irradianceComputePipeline{};
	VkPipelineLayout				m_visibilityComputePipelineLayout{};
	VkPipeline						m_visibilityComputePipeline{};
	VkDescriptorSetLayout			m_computeDescriptorLayout{};
	VkDescriptorSet					m_computeDescriptorSet{};

	VulkanBuffer closestHitVertexBuffer;
	VulkanBuffer closestHitIndexBuffer;

	void createRaytraceDescriptors(DeviceContext& ctx);
	void createRaytracePipeline(DeviceContext& ctx, VkDescriptorSetLayout& textureLayout);
	void createShaderBindingTable(DeviceContext& ctx, VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);
	void createComputeResources(DeviceContext& ctx);

public:
	DDGIVolume m_probeVolume;
	probeVisObjects	m_probeVis{};

	void setup(DeviceContext& ctx, rtHelper* rtHelper, SceneGraph& m_scene, VkDescriptorSetLayout& textureLayout);
	void bakeDDGI(DeviceContext& ctx, VkCommandBuffer& cmd, VkDescriptorSet& textureSet);

	// probe visualization stuff
	void createProbeVisualizationStructures(DeviceContext& ctx, VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& descSet);
};