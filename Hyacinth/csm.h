#pragma once

#define NOMINMAX

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "vkdeviceutils.h"
#include "vkimageutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "fpcam.h"

constexpr int SHADOW_MAP_CASCADE_COUNT = 3;
constexpr float cascadeSplitLambda = 0.95f;
constexpr int cascadeImageSize = 4096;

struct shadowUniform {
	glm::mat4 viewProj[SHADOW_MAP_CASCADE_COUNT];
};

struct Cascade {
	VkImageView cascadeImageView;
	float splitDepth;
	VkDescriptorSet uniformDescriptorSet;
	glm::mat4 viewProj;
};

struct shadowGPUPushConstant {
	uint32_t cascadeIndex;
	VkDeviceAddress transformAddress;
	VkDeviceAddress drawDataAddress;
};

class shadowHelper {
private:
	VkFormat shadowFormat = VK_FORMAT_D32_SFLOAT;
	std::vector<glm::vec4> corners;

	void updateFrustumCorners(float camNear, float camFar, glm::mat4 proj, glm::mat4 view);

public:
	Transform transform;
	std::vector<Cascade> m_cascades;
	VkExtent2D extent;
	VulkanImage m_shadowImage;
	std::vector<VulkanBuffer> m_uniformBuffers;
	VulkanPipelineBuilder m_shadowPipelineUtil;
	DescriptorAllocator	m_descriptorAllocator{};
	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };

	void setup(int maxFramesInFlight);
	void update(FPSCam::CameraProps& cam, int currentFrame);
	void destroy();
};