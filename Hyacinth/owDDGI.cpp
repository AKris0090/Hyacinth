#include "owDDGI.h"

void owDDGI::setup(DeviceContext& ctx, rtHelper* rtHelper) {
	m_rtHelper = rtHelper;
	numProbes = PROBE_DENSITY_WIDTH * PROBE_DENSITY_HEIGHT * PROBE_DENSITY_DEPTH;

	// test volume for sponza
	m_probeVolume.transform.position = glm::vec3(0.f, 0.38f, 5.15f);
	m_probeVolume.transform.scale = glm::vec3(15.f, 8.84f, 6.6f);

	// evenly disperse probes
	float xSpace = m_probeVolume.transform.scale.x / PROBE_DENSITY_WIDTH;
	float ySpace = m_probeVolume.transform.scale.y / PROBE_DENSITY_HEIGHT;
	float zSpace = m_probeVolume.transform.scale.z / PROBE_DENSITY_DEPTH;
	for (int i = 0; i < PROBE_DENSITY_HEIGHT; i++) {
		std::vector<std::vector<glm::vec3>> probePlane;
		for (int j = 0; j < PROBE_DENSITY_WIDTH; j++) {
			std::vector<glm::vec3> probeRow;
			for (int k = 0; k < PROBE_DENSITY_DEPTH; k++) {
				probeRow.push_back(glm::vec3(xSpace * j, ySpace * k, zSpace * i));
			}
			probePlane.push_back(probeRow);
		}
		m_probeVolume.probes.push_back(probePlane);
	}

	// set up textures for volume
	VkExtent3D irradianceExtent{
		.width = IRRADIANCE_PIXEL_COUNT * numProbes,
		.height = IRRADIANCE_PIXEL_COUNT * numProbes,
		.depth = 1
	};
	VkImageCreateInfo imgInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.extent = irradianceExtent;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.arrayLayers = PROBE_DENSITY_HEIGHT;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.mipLevels = 1;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(*ctx.allocator, &imgInfo, &allocInfo, &m_probeVolume.irradianceImage.image, &m_probeVolume.irradianceImage.imageAllocation, nullptr));

	VkExtent3D visibilityExtent{
		.width = VISIBILITY_PIXEL_COUNT * numProbes,
		.height = VISIBILITY_PIXEL_COUNT * numProbes,
		.depth = 1
	};
	imgInfo.format = VK_FORMAT_D16_UNORM_S8_UINT;
	imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.extent = visibilityExtent;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.arrayLayers = PROBE_DENSITY_HEIGHT;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.mipLevels = 1;

	VK_CHECK(vmaCreateImage(*ctx.allocator, &imgInfo, &allocInfo, &m_probeVolume.visibilityImage.image, &m_probeVolume.visibilityImage.imageAllocation, nullptr));

	VkImageViewCreateInfo completeViewInfo{};
	completeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	completeViewInfo.image = m_probeVolume.irradianceImage.image;
	completeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	completeViewInfo.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	completeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	completeViewInfo.subresourceRange.baseMipLevel = 0;
	completeViewInfo.subresourceRange.levelCount = 1;
	completeViewInfo.subresourceRange.baseArrayLayer = 0;
	completeViewInfo.subresourceRange.layerCount = PROBE_DENSITY_HEIGHT;

	VK_CHECK(vkCreateImageView(*ctx.device, &completeViewInfo, nullptr, &m_probeVolume.irradianceImage.imageView));

	completeViewInfo.image = m_probeVolume.visibilityImage.image;
	completeViewInfo.format = VK_FORMAT_D16_UNORM_S8_UINT;
	completeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	VK_CHECK(vkCreateImageView(*ctx.device, &completeViewInfo, nullptr, &m_probeVolume.visibilityImage.imageView));

	VkSamplerCreateInfo samplerInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = samplerInfo.addressModeU;
	samplerInfo.addressModeW = samplerInfo.addressModeU;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(*ctx.device, &samplerInfo, nullptr, &m_probeVolume.irradianceImage.imageSampler));
	VK_CHECK(vkCreateSampler(*ctx.device, &samplerInfo, nullptr, &m_probeVolume.visibilityImage.imageSampler));


}

void owDDGI::bakeDDGI() {

}