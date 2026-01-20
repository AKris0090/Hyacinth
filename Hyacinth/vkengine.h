#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <imgui.h>
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

#include "vkdeviceutils.h"
#include "vkdebugutils.h"
#include "vkimageutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "vkmeshutils.h"
#include "gltfutils.h"
#include "raytracing.h"
#include "owDDGI.h"

#include "csm.h"
#include "time.h"
#include "fpcam.h"

#include "vk_mem_alloc.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef NDEBUG
const bool enableValLayers = false;
#else
const bool enableValLayers = true;
#endif

const VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

struct UBO {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 viewPos;
	glm::vec4 lightPos;
	glm::vec4 cascadeSplits;
	glm::mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
};

class HyacinthEngine {
public:
	struct SDL_Window* m_window{ nullptr };
	FPSCam							m_camera{};

	HyacinthEngine() {};
	~HyacinthEngine() { cleanup(); };

	void init();
	void draw();
	void cleanup();

private:
	struct perFrame {
		VkCommandPool	commandPool;
		VkCommandBuffer commandBuffer;
		VulkanBuffer	uniformBuffer;
		void*			mappedUniformBuffer;
		VkDescriptorSet descriptorSet;
		VkDescriptorSet textureSet;
	};

	bool m_initialized = false;
	uint32_t  m_frameIndex = 0;
	uint32_t m_swImageIndex = 0;
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkInstance						m_instance				{ VK_NULL_HANDLE };
	VkPhysicalDevice				m_physicalDevice		{ VK_NULL_HANDLE };
	VkDevice						m_device				{ VK_NULL_HANDLE };
	VkSwapchainKHR					m_swapChain				{ VK_NULL_HANDLE };
	VkSurfaceKHR 					m_surface				{ VK_NULL_HANDLE };	
	VkDebugUtilsMessengerEXT		m_debugMessenger		{ VK_NULL_HANDLE };
	VkQueue							m_graphicsQueue			{ VK_NULL_HANDLE };
	VkQueue							m_presentQueue			{ VK_NULL_HANDLE };
	QueueFamilyIndices				m_qfIndices				{};
	VmaAllocator					m_allocator				{};
	DeviceContext					m_devContext			{};
	std::vector<perFrame>			m_frameData				{};
	std::vector<VkSemaphore>		m_imageAcquiredSemas	{};
	std::vector<VkSemaphore>		m_imageFinishedSemas	{};
	std::vector<VkFence> 			m_inFlightFences		{};
	VkFence							m_uploadFence			{ VK_NULL_HANDLE };
	std::vector<VulkanImage>		m_colorImages			{};
	std::vector<VulkanImage>		m_swapChainImages		{}; // a.k.a color resolve
	std::vector<VulkanImage>		m_depthImages			{};
	std::vector<VulkanImage>		m_depthResolveImages	{};
	SWChainImageFormat				m_swImageFormat			{};
	VulkanPipelineBuilder 			m_pipelineUtil;
	GPUMeshBuffers					m_meshBuffers			{};
	VulkanBuffer 					m_indirectDrawBuffer	{};
	VulkanBuffer 					m_worldMatrixBuffer		{};
	VulkanBuffer					m_drawDataBuffer		{};
	VulkanBuffer					m_materialBuffer		{};
	perFrame						m_uploadFrame			{};
	SceneGraph						m_scene					{};
	DescriptorAllocator				m_descriptorAllocator	{};
	VkDescriptorSetLayout			m_descriptorSetLayout	{ VK_NULL_HANDLE };
	VkDescriptorSetLayout			m_textureSetLayout		{ VK_NULL_HANDLE };
	shadowHelper					m_shadowHelper;
	rtHelper						m_rtHelper;
	owDDGI							m_owDDGIHelper;

	void createInstance();
	void createSwapchain();
	void createCommandBuffers();
	void createSyncObjects();
	void createGraphicsPipeline();
	void createBuffers();
	void createDescriptorSets();
	void setupImGUI();
	void drawImGui();
	void loadScene();
	void update();
	void setupDraw();
	void drawShadowMaps(VkCommandBuffer& cmd);
	void endDraw();

	inline perFrame& getCurrentFrame() {
		return m_frameData[m_frameIndex];
	}
};