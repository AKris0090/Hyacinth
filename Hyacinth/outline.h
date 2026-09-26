#pragma once

#include "vkpipelineutils.h"
#include "vkdeviceutils.h"
#include "vkimageutils.h"
#include "vkdescriptorutils.h"
#include "vkmeshutils.h"

// idea is draw outlines into an image, use a grow shader, and then use sobol operator? 

// doesnt really maake sense. Just use the grow and a subtract?   

// can also use stencil? render to stencil, grow in stencil, and write a fullscreen pass with the stencil test

constexpr uint8_t OUTLINE_BIT = 0x01;

const VkClearValue fullClear = { {{0.0f, 0.0f, 0.0f, 0.0f}} };

struct OutlinePushConstant {
	VkDeviceAddress renderCallBuffer;
	VkDeviceAddress materialCallBuffer;
};

struct OutlineGrowPC {
	glm::vec2 screenSize;
};

class OutlineHelper {
private:
	DescriptorAllocator	m_descriptorAllocator{};
	VkDescriptorSetLayout computeGrowSetLayout;
	std::vector<VulkanImage> characterDepthImages;
	VkSampler stencilImageSampler;
	std::vector<VkDescriptorSet> computeGrowSets;

	VulkanPipelineBuilder stencilDrawPipeline;
	VulkanPipeline computeGrowPipeline; // should use both depth buffer from stencil draw pass and depth buffer from depth prepass to assess if the pixel should be grown into

public:
	VulkanPipelineBuilder layerOutlinePipeline;

	void setup(VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& gBufferSetLayout, VkExtent2D swapChainExtent, VkFormat depthFormat, std::vector<VulkanImage*>& depthStencilImages, std::vector<VulkanImage*>& amrImages);
	void drawEdges(VkCommandBuffer& cmd, uint32_t imageIndex, VkDescriptorSet& uniformSet, VkDescriptorSet& compositeSet, VulkanImage& depthStencilImage, VulkanImage& amrImage, VkExtent2D swExtent, VkDeviceAddress& renderCallAddress, VkDeviceAddress& materialAddress, std::vector<VkDrawIndexedIndirectCommand>& stencilRenderCalls, VulkanBuffer& skinnedVertexBuffer);
	void recreateImageResources(VkExtent2D swapChainExtent, std::vector<VulkanImage*>& depthStencilImages, std::vector<VulkanImage*>& amrImages);
	void shutdown();
};