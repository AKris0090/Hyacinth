#pragma once

#include "vkpipelineutils.h"
#include "vkdebugutils.h"
#include <array>
#include <deque>
#include "transform.h"
#include <mutex>
#include <shared_mutex>

constexpr glm::vec3 bulletHoleDecaleSize = glm::vec3(0.1f, 0.1f, 0.1f);

constexpr uint32_t MAX_BULLET_DECALS = 50;

struct decalPushConstant {
	VkDeviceAddress renderCallBuffer;
	VkDeviceAddress materialBuffer;
};

class DecalManager {
public:
	VulkanPipelineBuilder decalPipelineUtil;

	std::shared_mutex instanceLock;
	std::deque<glm::mat4> decalInstances;

	void setup(SWChainImageFormat swImageFormat, VkFormat depthImageFormat, VkDescriptorSetLayout uniformSetLayout, VkDescriptorSetLayout textureSetLayout);
	void addDecal(glm::vec3 pos, glm::vec3 normal);
	void shutdown();
};