#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include "vk_mem_alloc.h"
#include <cmath>
#include <algorithm>
#include <array>

struct VulkanImage {
	VkImage image;
	VkImageView imageView;
	VkSampler imageSampler;
	VmaAllocation imageAllocation;
	VkExtent3D extent;
	VkFormat imageFormat;
	uint32_t mipLevels = 1;
	uint32_t arrayLayers = 1;

	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

namespace vkimageutils {
	extern float maxAnisotropy;

	void	setMaxAnisotropy(float max);
	float	getMaxAnisotropy();
	void	setLinear(VkFormatFeatureFlags flag);
	bool	getLinearBlit();

	void						createImageSampler(VulkanImage& image, VkSamplerAddressMode samplerMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, VkBorderColor bColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK);
	VkImageView					createImageView(VulkanImage& image, uint32_t baseArrayLayer, uint32_t layerCount, VkImageAspectFlags aspectFlags, bool cube);
	VulkanImage					createImageandView(VkExtent3D size, uint32_t arrayLayers, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, bool mipped, std::string qual = "", bool cube = false);
	VulkanImage					createTextureImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipped);
	VulkanImage					createSkyboxImage(std::array<float*, 6>& data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
	void						transitionImage(VkCommandBuffer& cmd, VulkanImage& image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
	void						generateMipmaps(VkCommandBuffer& commandBuffer, VulkanImage& image);
	VkRenderingAttachmentInfo	createColorAttachmentInfo(VkImageView& msaaColorView, const VkClearValue& clearColor, VkImageLayout imageLayout, bool clear = true);
	VkRenderingAttachmentInfo	createDepthAttachmentInfo(VkImageView& msaaDepthView, bool clear = true);
	VkRenderingAttachmentInfo	createStencilAttachmentInfo(VkImageView& stencilImageView, bool clear = true);
	VkRenderingAttachmentInfo	createShadowAttachmentInfo(VkImageView& view);

	void destroyImage(VulkanImage& image);
}