#pragma once

#include "raytracing.h"
#include "ecshelpers.h"
#include "vkdescriptorutils.h"
#include "vkpipelineutils.h"
#include <array>

constexpr int PROBE_DENSITY_WIDTH = 10;  // x
constexpr int PROBE_DENSITY_HEIGHT = 10;  // y
constexpr int PROBE_DENSITY_DEPTH = 10;  // z

constexpr int RAYS_PER_PROBE = 36;

constexpr int IRRADIANCE_PIXEL_COUNT = 8;
constexpr int VISIBILITY_PIXEL_COUNT = 16;

struct DDGIVolume {
	transform transform;
	std::vector<std::vector<std::vector<glm::vec3>>> probes;
	VulkanBuffer probePositionBuffer;

	VulkanImage irradianceImage;
	VulkanImage visibilityImage;
};

struct DDGIPushConstant {
	VkDeviceAddress probePositionBufferAddress;
	VkDeviceAddress vertexAddress;
	VkDeviceAddress indexAddress;
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
	DDGIVolume m_probeVolume;

	VkFormat m_irradFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat m_depthFormat = VK_FORMAT_R16_UNORM;

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

	probeVisObjects					m_probeVis{};

	void createRaytraceDescriptors(DeviceContext& ctx);
	void createRaytracePipeline(DeviceContext& ctx);
	void createShaderBindingTable(DeviceContext& ctx, VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

public:
	void setup(DeviceContext& ctx, rtHelper* rtHelper);
	void bakeDDGI(DeviceContext& ctx, SceneGraph& m_scene);


	// probe visualization stuff
	void createProbeVisualizationStructures(DeviceContext& ctx, VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples);
	void drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& descSet);
};