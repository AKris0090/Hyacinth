#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>
#include <utility>

#include "vkdeviceutils.h"
#include "vkdebugutils.h"
#include "vkimageutils.h"
#include "vkpipelineutils.h"
#include "vkdescriptorutils.h"
#include "vkmeshutils.h"
#include "gltfutils.h"

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
constexpr int MAX_FRAMES_IN_FLIGHT = 3;

struct UBO {
	glm::mat4 view;
	glm::mat4 proj;
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
	std::vector<VkImage>			m_swapChainImages		{};
	std::vector<VkImageView>		m_swapChainImageViews	{};
	std::vector<VulkanImage>		m_depthImages			{};
	SWChainImageFormat				m_swImageFormat			{};
	VulkanPipelineBuilder 			m_pipelineUtil;
	GPUMeshBuffers					m_meshBuffers			{};
	VulkanBuffer 					m_indirectDrawBuffer	{};
	VulkanBuffer 					m_worldMatrixBuffer		{};
	perFrame						uploadFrame				{};
	sceneGraph						m_scene					{};
	DescriptorAllocator				m_descriptorAllocator	{};
	VkDescriptorSetLayout			m_descriptorSetLayout	{ VK_NULL_HANDLE };

	void createInstance();
	void createSwapchain();
	void createCommandBuffers();
	void createSyncObjects();
	void createGraphicsPipeline();
	void createBuffers();
	void createDescriptorSets();
	void update();
	void setupDraw();
	void endDraw();

	inline perFrame& getCurrentFrame() {
		return m_frameData[m_frameIndex];
	}
};