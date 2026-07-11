#pragma once

#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "vkimageutils.h"
#include "unit_cube.h"
#include "fullscreen_quad.h"
#include <string>
#include <array>
#include "stb_image.h"


const std::array<std::string, 6> skyboxPaths = {
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "px.png").string(),
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "nx.png").string(),
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "py.png").string(),
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "ny.png").string(),
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "pz.png").string(),
	(vkdebugutils::getExeDir() / "skybox" / "darkstorm" / "nz.png").string(),
};

constexpr VkFormat SKYBOX_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT;

class SkyboxHelper {
private:
	DescriptorAllocator				m_descriptorAllocator{};
	VkDescriptorSetLayout m_skyboxSetLayout;
	void createSkyboxImage();

public:
	VulkanImage				m_skyboxImage;
	VkDescriptorSet			m_skyboxSet;
	VulkanPipelineBuilder	m_skyboxPipelineUtil;

	void setup(SWChainImageFormat swapchainImageFormat, VkDescriptorSetLayout& uniformLayout);
	void drawSkybox(VkCommandBuffer& cmd);
	void shutdown();
};