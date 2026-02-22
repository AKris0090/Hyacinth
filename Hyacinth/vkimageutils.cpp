#include "vkimageutils.h"

namespace vkimageutils {
	float maxAnisotropy = 1.0f;
	VkFormatFeatureFlags linear;

	void setMaxAnisotropy(float max) {
		maxAnisotropy = max;
	}

	float getMaxAnisotropy() {
		return maxAnisotropy;
	}

	void setLinear(VkFormatFeatureFlags flag) {
		linear = flag;
	}

	bool getLinearBlit() {
		return linear & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	}

	void createImageSampler(VulkanImage& image) {
		VkSamplerCreateInfo samplerCInfo{};
		samplerCInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCInfo.magFilter = VK_FILTER_LINEAR;
		samplerCInfo.minFilter = VK_FILTER_LINEAR;
		samplerCInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCInfo.mipLodBias = 0.0f;
		samplerCInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCInfo.minLod = 0.0f;
		samplerCInfo.maxLod = (float)image.mipLevels;
		samplerCInfo.anisotropyEnable = VK_TRUE;
		samplerCInfo.maxAnisotropy = vkimageutils::getMaxAnisotropy();
		samplerCInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerCInfo, nullptr, &image.imageSampler));
	}

	VkImageView createImageView(VulkanImage& image, uint32_t baseArrayLayer, uint32_t layerCount, VkImageAspectFlags aspectFlags) {
		VkImageView imageView;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

		if (layerCount > 1) {
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		}

		viewInfo.format = image.imageFormat;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = image.mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
		viewInfo.subresourceRange.layerCount = layerCount;

		VK_CHECK(vkCreateImageView(vkdeviceutils::device, &viewInfo, nullptr, &imageView));
		return imageView;
	}

	VulkanImage createImageandView(VkExtent3D size, uint32_t arrayLayers, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, bool mipped, std::string qual) {
		VulkanImage newImage{};
		newImage.imageFormat = format;
		newImage.extent = VkExtent3D{ size.width, size.height, 1 };

		VkImageCreateInfo imgInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imgInfo.format = format;
		imgInfo.usage = usage;
		imgInfo.extent = size;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.arrayLayers = arrayLayers;
		imgInfo.samples = numSamples;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imgInfo.mipLevels = 1;

		if (mipped) {
			imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2((std::max)(size.width, size.height)))) + 1;
		}
		newImage.mipLevels = imgInfo.mipLevels;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK(vmaCreateImage(vkdeviceutils::allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.imageAllocation, nullptr));
		if (qual != "") {
			vmaSetAllocationName(vkdeviceutils::allocator, newImage.imageAllocation, qual.c_str());
		}

		VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
		if (format == VK_FORMAT_D32_SFLOAT) {
			aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (format == VK_FORMAT_S8_UINT) {
			aspectFlag = VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		newImage.imageView = vkimageutils::createImageView(newImage, 0, arrayLayers, aspectFlag);

		return newImage;
	}

	void vkimageutils::transitionImage(VkCommandBuffer& cmd, VkImage& image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask)
	{
		VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

		imageBarrier.oldLayout = currentLayout;
		imageBarrier.newLayout = newLayout;

		VkImageSubresourceRange subImage{};
		subImage.aspectMask = aspectMask;
		subImage.baseMipLevel = 0;
		subImage.levelCount = VK_REMAINING_MIP_LEVELS;
		subImage.baseArrayLayer = 0;
		subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
		imageBarrier.subresourceRange = subImage;
		imageBarrier.image = image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	VulkanImage createTextureImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipped) {
		size_t dataSize = size.depth * size.width * size.height * 4;
		VulkanBuffer uploadBuffer = vkdeviceutils::createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
		memcpy(uploadBuffer.info.pMappedData, data, dataSize);
		VulkanImage newImage = vkimageutils::createImageandView(size, 1, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT, mipped, "texture_image");

		vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
			vkimageutils::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;

			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = size;

			vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			vkimageutils::generateMipmaps(cmd, newImage);
			});

		vkdeviceutils::destroyBuffer(uploadBuffer);
		vmaSetAllocationName(vkdeviceutils::allocator, newImage.imageAllocation, "texture_image");
		return newImage;
	}

	VkRenderingAttachmentInfo createColorAttachmentInfo(VkImageView& msaaColorView, const VkClearValue& clearColor, VkImageLayout imageLayout, bool clear) {
		VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		attachmentInfo.imageView = msaaColorView;
		attachmentInfo.imageLayout = imageLayout;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		if (!clear) {
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.clearValue = clearColor;
		return attachmentInfo;
	}

	VkRenderingAttachmentInfo createDepthAttachmentInfo(VkImageView& msaaDepthView, bool clear) {
		VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		attachmentInfo.imageView = msaaDepthView;
		attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		if (!clear) {
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.clearValue.depthStencil.depth = 1.f;
		return attachmentInfo;
	}

	VkRenderingAttachmentInfo createStencilAttachmentInfo(VkImageView& stencilImageView, bool clear) {
		VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		attachmentInfo.imageView = stencilImageView;
		attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		if (!clear) {
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.clearValue.depthStencil.stencil = 0;

		return attachmentInfo;
	}

	VkRenderingAttachmentInfo createShadowAttachmentInfo(VkImageView& imageView)
	{
		VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		attachmentInfo.imageView = imageView;
		attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.clearValue.depthStencil.depth = 1.f;
		return attachmentInfo;
	}

	void generateMipmaps(VkCommandBuffer& commandBuffer, VulkanImage& image) {
		if (!(vkimageutils::getLinearBlit())) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image.image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = image.extent.width;
		int32_t mipHeight = image.extent.height;

		for (uint32_t i = 1; i < image.mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = image.mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	void destroyImage(VulkanImage& image) {
		if (image.imageSampler != VK_NULL_HANDLE) vkDestroySampler(vkdeviceutils::device, image.imageSampler, nullptr);
		if (image.imageView != VK_NULL_HANDLE) vkDestroyImageView(vkdeviceutils::device, image.imageView, nullptr);
		vmaDestroyImage(vkdeviceutils::allocator, image.image, image.imageAllocation);
	}
}