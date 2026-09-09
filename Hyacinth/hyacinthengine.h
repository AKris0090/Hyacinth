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
#include "raytracing.h"
#include "owDDGI.h"

#include "hcinth_assetdrawer.h"

#include "frustumcull.h"

#include "worldspace_health.h"

#include "csm.h"
#include "time.h"
#include "fpcam.h"

#include "imguihelper.h"

#include "skybox.h"
#include "ambient.h"
#include "specular.h"
#include "outline.h"

#include "net_ent.h"
#include "netDebugRenderer.h"

#include "hyacinth_ui.h"

#include "vk_mem_alloc.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "staticgameobject.h"
#include "animatedgameobject.h"

#ifdef NDEBUG
const bool enableValLayers = false;
#else
const bool enableValLayers = true;
#endif

// #define DEBUG_NETWORK

// stencil bits
constexpr uint8_t CURRENT_BIT = 0x01;
constexpr uint8_t ANY_BIT = 0x02;

const VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

struct volumeStencilPushConstant {
	VkDeviceAddress volumeTransformAddress;
	uint32_t volumeIndex;
};

struct tracerPushConstant {
	VkDeviceAddress tracerTransformsAddress;
	VkDeviceAddress materialBufferAddress;
	uint32_t matIndex;
	uint32_t tracerIndex;
	float alpha;
};

struct computeSkinPushConstant {
	VkDeviceAddress vertexBufferInAddress;
	VkDeviceAddress vertexBufferOutAddress;
	VkDeviceAddress jointBufferAddress;
	uint32_t numVertices;
	uint32_t srcVertexOffset;
	uint32_t dstVertexOffset;
};

struct UBO {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 viewPos;
	glm::vec4 lightPos;
	glm::vec4 ABOD;
	glm::mat4 globalShadowMatrix;
	glm::vec4 cascadeSplits;
	glm::mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
	glm::vec4 cascadeOffsets[SHADOW_MAP_CASCADE_COUNT];
	glm::vec4 cascadeScales[SHADOW_MAP_CASCADE_COUNT];
};

struct GBuffer {
	VulkanImage albedo;
	VulkanImage normal;
	VulkanImage AMR;

	VulkanImage ddgiImage;
	VulkanImage depth;

	VulkanImage compositeImage;

	VkDescriptorSet					m_compositeSet{ VK_NULL_HANDLE };
	VkDescriptorSet					m_postProcessSet{ VK_NULL_HANDLE };
};

class HyacinthEngine {
public:
	bool mouseLocked = true;
	struct SDL_Window* m_window{ nullptr };
	SWChainImageFormat				m_swImageFormat{};
	VkDescriptorSetLayout			m_descriptorSetLayout{ VK_NULL_HANDLE };
	NetworkEntityManager*			p_netEntManager;
	WorldHealthManager				m_worldHealthManager;
	std::mutex camMutex;
	Camera m_camera;

	HAssetDrawer					m_assetDrawer;
	owDDGI							m_owDDGIHelper;

	std::vector<HStaticGameObject*> m_staticObjects;
	std::vector<AnimatedObjectWrap> m_animatedObjects;

#ifdef DEBUG_NETWORK
	NetDebugRenderer m_netDebugRenderer;
#endif

	HyacinthEngine() {};

	void init();
	void draw();
	void shutdown();

	void addStaticGameObject(HStaticGameObject* gameObjectRef);
	void addAnimatedGameObject(HAnimatedGameObject* gameObjectRef);
	void bakeDDGI();

private:
	struct perFrame {
		VkCommandPool	commandPool;
		VkCommandBuffer commandBuffer;
		VulkanBuffer	uniformBuffer;
		VulkanBuffer	m_indirectDrawBuffer{};
		VulkanBuffer	m_renderListBuffer{};
		VulkanBuffer	m_skinnedVertexBuffer{};
		void*			mappedUniformBuffer;
		VkDescriptorSet uniformDescriptorSet;
		VkDescriptorSet shadowDescriptorSet;
	};

	float volANormalBias;
	float volBNormalBias;
	float volAViewBias;
	float volBViewBias;

	uint32_t numStaticDrawCommands;
	uint32_t numDynamicDrawCommands;

	bool m_initialized = false;
	bool m_showImGui = false;
	float renderingModeToggle = 0;
	uint32_t m_frameIndex = 0;
	uint32_t m_swImageIndex = 0;
	uint32_t maxTracers = 10;
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
	std::vector<perFrame>			m_frameData				{};
	std::vector<VkSemaphore>		m_imageAcquiredSemas	{};
	std::vector<VkSemaphore>		m_imageFinishedSemas	{};
	std::vector<VkFence> 			m_inFlightFences		{};
	VkFence							m_uploadFence			{ VK_NULL_HANDLE };
	std::vector<GBuffer>			m_gBuffers				{};
	std::vector<VulkanImage>		m_swapChainImages		{}; // a.k.a color resolve
	VulkanPipelineBuilder 			m_pipelineUtil			{};
	VulkanPipelineBuilder 			m_depthPipelineUtil		{};
	VulkanPipelineBuilder 			m_tracerPipelineUtil	{};
	VulkanPipelineBuilder 			m_compositePipelineUtil {};
	VulkanPipelineBuilder			m_ddgiPipelineUtil		{};
	VulkanPipelineBuilder			m_skinnedPipelineUtil   {};
	VulkanPipelineBuilder			m_volumeStencilPipeline	{};
	VulkanPipelineBuilder			m_fxaaPipelineUtil		{};
	VulkanPipeline					m_computeSkinPipeline	{};

	std::vector<HRenderCall>		m_renderList;
	std::vector<VkDrawIndexedIndirectCommand>		m_stencilDrawList;

	uint32_t dynamicDrawCommandOffset = 0;
	std::vector<VkDrawIndexedIndirectCommand> m_drawCommands; // includes static and dynamic

	perFrame						m_uploadFrame			{};

	DescriptorAllocator				m_descriptorAllocator	{};
	DescriptorAllocator				m_imGuiAllocator		{};

	VkDescriptorSetLayout			m_textureSetLayout		{ VK_NULL_HANDLE };
	VkDescriptorSet					m_textureSet{ VK_NULL_HANDLE };

	VkDescriptorSetLayout			m_shadowSetLayout		{ VK_NULL_HANDLE };
	VkDescriptorSetLayout			m_diffuseSetLayout		{ VK_NULL_HANDLE };
	VkDescriptorSetLayout			m_compositeSetLayout	{ VK_NULL_HANDLE };
	VkDescriptorSetLayout			m_postProcessSetLayout	{ VK_NULL_HANDLE };
	shadowHelper					m_shadowHelper;
	rtHelper						m_rtHelper;
	FrustumCullHelper				m_frustumCullHelper;
	HyacinthUIManager				m_uiHelper;
	SkyboxHelper					m_skyboxHelper;
	AmbientHelper					m_ambientHelper;
	SpecularTraceHelper				m_specularTraceHelper;
	OutlineHelper					m_outlineHelper;

	void createInstance(); // also creates vma allocator
	void createSwapchain();
	void createColorImages();
	void recreateSwapchain();
	void createCommandBuffers();
	void createSyncObjects();
	void createDepthPipeline();
	void createGraphicsPipeline();
	void createCompositePipeline();
	void createDDGIPipeline();
	void createDDGIVolumePipeline();
	void createCompSkinPipeline();
	void createFXAAPipeline();
	void createBuffers();
	void createDescriptorSets();
	void setupImGUI();
	void drawImGui();
	void loadAssets();
	void generateRenderList();
	void generateDrawCommands();
	void update();
	int setupDraw();
	void endDraw();

	inline perFrame& getCurrentFrame() {
		return m_frameData[m_frameIndex];
	}
};