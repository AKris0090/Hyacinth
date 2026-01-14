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

	void createImageView(VkDevice& device, VulkanImage& image, VkImageAspectFlags aspectFlags);
	VulkanImage createImage(DeviceContext& ctx, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, bool mipped);
	void transitionImage(VkCommandBuffer& cmd, VkImage& image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
	VulkanImage createImage(DeviceContext& ctx, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipped);
	void createImageSampler(VkDevice& device, VulkanImage& image);
	VkRenderingAttachmentInfo createColorAttachmentInfo(VkImageView& msaaColorView, VkImageView& resolveImageView, const VkClearValue& clearColor, VkImageLayout imageLayout);
	VkRenderingAttachmentInfo createDepthAttachmentInfo(VkImageView& msaaDepthView, VkImageView& resolvedDepthView);
	void storeTexture(VkDevice& dev, VkDescriptorSet& set, const VulkanImage& image, uint32_t arrayIndex);
	void generateMipmaps(VkCommandBuffer& commandBuffer, VulkanImage& image);
}