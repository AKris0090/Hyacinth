#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>
#include <utility>

#include "vkdeviceutils.h"
#include "vkdebugutils.h"
#include "vkimageutils.h"

#ifdef NDEBUG
const bool enableValLayers = false;
#else
const bool enableValLayers = true;
#endif

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
	int	 maxFramesInFlight = 1;

	VkInstance						m_instance				{ VK_NULL_HANDLE };
	VkPhysicalDevice				m_physicalDevice		{ VK_NULL_HANDLE };
	VkDevice						m_device				{ VK_NULL_HANDLE };
	VkSwapchainKHR					m_swapChain				{ VK_NULL_HANDLE };
	VkSurfaceKHR 					m_surface				{ VK_NULL_HANDLE };	
	VkDebugUtilsMessengerEXT		m_debugMessenger		{ VK_NULL_HANDLE };
	VkQueue							m_graphicsQueue			{ VK_NULL_HANDLE };
	VkQueue							m_presentQueue			{ VK_NULL_HANDLE };
	QueueFamilyIndices				m_qfIndices				{};
	std::vector<perFrame>			m_frameData				{};
	std::vector<VkSemaphore>		m_imageAcquiredSemas	{};
	std::vector<VkSemaphore>		m_imageFinishedSemas	{};
	std::vector<VkFence> 			m_inFlightFences		{};

	std::vector<VkImage>			m_swapChainImages		{};
	std::vector<VkImageView>		m_swapChainImageViews	{};
	SWChainImageFormat				m_swImageFormat			{};

	void createInstance();
	void createSwapchain();
	void createCommandBuffers();
	void createSyncObjects();

	inline perFrame& getCurrentFrame() {
		return m_frameData[m_frameIndex];
	}

	void incrementFrameIndex(int& frameInd)
	{
		frameInd = (frameInd + 1) % maxFramesInFlight;
	}
};