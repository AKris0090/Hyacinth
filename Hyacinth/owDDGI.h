#pragma once

#include "raytracing.h"
#include "ecshelpers.h"

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

public:
	void setup(DeviceContext& ctx, rtHelper* rtHelper);
	void bakeDDGI();
};