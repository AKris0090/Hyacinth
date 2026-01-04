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
#include "vkmeshutils.h"
#include "gltfutils.h"

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

class HyacinthEngine {
public:
	struct SDL_Window* m_window{ nullptr };

	HyacinthEngine() {};
	~HyacinthEngine() { cleanup(); };

	void init();
	void draw();
	void cleanup();

private:
	struct perFrame {
		VkCommandPool	commandPool;
		VkCommandBuffer commandBuffer;
	};

	bool m_initialized = false;
	int  m_frameIndex = 0;
	uint32_t m_imageIndex = 0;
	int	 maxFramesInFlight = 1;
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
	perFrame						uploadFrame				{};
	sceneGraph						m_scene					{};

	void createInstance();
	void createSwapchain();
	void createCommandBuffers();
	void createSyncObjects();
	void createGraphicsPipeline();
	void createBuffers();
	glm::mat4 getCamMatrix() const;

	void setupDraw();
	void endDraw();

	inline perFrame& getCurrentFrame() {
		return m_frameData[m_frameIndex];
	}

	void incrementFrameIndex(int& frameInd) const
	{
		frameInd = (frameInd + 1) % maxFramesInFlight;
	}
};