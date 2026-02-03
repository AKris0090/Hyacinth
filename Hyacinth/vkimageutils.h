#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include "vk_mem_alloc.h"
#include <cmath>
#include <algorithm>

struct VulkanImage {
	VkImage image;
	VkImageView imageView;
	VkSampler imageSampler;
	VmaAllocation imageAllocation;
	VkExtent3D extent;
	VkFormat imageFormat;
	uint32_t mipLevels = 1;
};

namespace vkimageutils {
	extern float maxAnisotropy;

	void setMaxAnisotropy(float max);
	float getMaxAnisotropy();
	void setLinear(VkFormatFeatureFlags flag);
	bool getLinearBlit();

	void createImageSampler(VulkanImage& image);
	VkImageView createImageView(VulkanImage& image, uint32_t baseArrayLayer, uint32_t layerCount, VkImageAspectFlags aspectFlags);
	VulkanImage createImageandView(VkExtent3D size, uint32_t arrayLayers, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, bool mipped, std::string qual = "");
	void transitionImage(VkCommandBuffer& cmd, VkImage& image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
	VulkanImage createTextureImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipped);
	VkRenderingAttachmentInfo createColorAttachmentInfo(VkImageView& msaaColorView, VkImageView& resolveImageView, const VkClearValue& clearColor, VkImageLayout imageLayout);
	VkRenderingAttachmentInfo createDepthAttachmentInfo(VkImageView& msaaDepthView, VkImageView& resolvedDepthView);
	VkRenderingAttachmentInfo createShadowAttachmentInfo(VkImageView& view);
	void generateMipmaps(VkCommandBuffer& commandBuffer, VulkanImage& image);

	void destroyImage(VulkanImage& image);
}