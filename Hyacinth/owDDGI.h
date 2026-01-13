#pragma once

#include "raytracing.h"
#include "ecshelpers.h"
#include "vkdescriptorutils.h"
#include <array>

constexpr int PROBE_DENSITY_WIDTH = 10;  // x
constexpr int PROBE_DENSITY_HEIGHT = 10;  // y
constexpr int PROBE_DENSITY_DEPTH = 10;  // z

constexpr int IRRADIANCE_PIXEL_COUNT = 8;
constexpr int VISIBILITY_PIXEL_COUNT = 16;

struct DDGIVolume {
	transform transform;
	std::vector<std::vector<std::vector<glm::vec3>>> probes;

	VulkanImage irradianceImage;
	VulkanImage visibilityImage;
};

struct DDGIPushConstant {
	VkDeviceAddress probePositionBufferAddress;
	glm::vec4 probeDensities;
	uint32_t probeCount;
};

class owDDGI {
private:
	rtHelper* m_rtHelper;
	DDGIVolume m_probeVolume;

	uint32_t numProbes;

	VulkanBuffer                    m_sbtBuffer;
	std::vector<uint8_t>            m_shaderHandles;
	VkStridedDeviceAddressRegionKHR m_raygenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	VkStridedDeviceAddressRegionKHR m_callableRegion{};

	DescriptorLayoutBuilder			m_layoutBuilder{};
	DescriptorAllocator				m_descriptorAllocator{};
	VkDescriptorSetLayout			m_descriptorLayout{};
	VkDescriptorSet					m_rtDescriptorSet{};
	VkPipelineLayout				m_rtPipelineLayout{};
	VkPipeline						m_rtPipeline{};

	void createRaytraceDescriptors(DeviceContext& ctx);
	void createRaytracePipeline(DeviceContext& ctx);
	void createShaderBindingTable(DeviceContext& ctx, VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

public:
	void setup(DeviceContext& ctx, rtHelper* rtHelper);
	void bakeDDGI();
};