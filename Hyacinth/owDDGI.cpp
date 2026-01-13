#include "owDDGI.h"

void owDDGI::createRaytraceDescriptors(DeviceContext& ctx) {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2.f }, // out images
	};

	m_descriptorAllocator.initPool(*ctx.device, 1, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // radiance image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // visibility image
		m_descriptorLayout = layoutBuilder.buildLayout(*ctx.device, nullptr, 0);
	}

	m_rtDescriptorSet = m_descriptorAllocator.allocate(*ctx.device, m_descriptorLayout);

	VkWriteDescriptorSetAccelerationStructureKHR asInfo{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	asInfo.accelerationStructureCount = 1;
	asInfo.pAccelerationStructures = &m_rtHelper->m_tlAccelStrucutre.accel;

	VkWriteDescriptorSet descriptorWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.pNext = &asInfo;
	descriptorWrite.dstSet = m_rtDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorWrite.descriptorCount = 1;

	vkUpdateDescriptorSets(*ctx.device, 1, &descriptorWrite, 0, nullptr);

	VkDescriptorImageInfo radianceImageInfo{};
	radianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	radianceImageInfo.imageView = m_probeVolume.irradianceImage.imageView;

	VkDescriptorImageInfo visibilityImageInfo{};
	visibilityImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	visibilityImageInfo.imageView = m_probeVolume.visibilityImage.imageView;

	VkWriteDescriptorSet radianceWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	radianceWrite.dstSet = m_rtDescriptorSet;
	radianceWrite.dstBinding = 1;
	radianceWrite.dstArrayElement = 0;
	radianceWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	radianceWrite.descriptorCount = 1;
	radianceWrite.pImageInfo = &radianceImageInfo;

	VkWriteDescriptorSet visibilityWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	visibilityWrite.dstSet = m_rtDescriptorSet;
	visibilityWrite.dstBinding = 2;
	visibilityWrite.dstArrayElement = 0;
	visibilityWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	visibilityWrite.descriptorCount = 1;
	visibilityWrite.pImageInfo = &visibilityImageInfo;

	std::array<VkWriteDescriptorSet, 2> writes = { radianceWrite, visibilityWrite };
	vkUpdateDescriptorSets(*ctx.device, 2, writes.data(), 0, nullptr);
}

void owDDGI::createShaderBindingTable(DeviceContext& ctx, VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
{
	size_t bufferSize = 1024;
	m_sbtBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, bufferSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
}

void owDDGI::createRaytracePipeline(DeviceContext& ctx)
{
	enum StageIndices
	{
		eRaygen,
		eMiss,
		eClosestHit,
		eShaderGroupCount
	};
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	for (auto& s : stages) {
		s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}
	VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eRaygen;
	shader_groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss;
	shader_groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	shader_groups.push_back(group);

	VkPushConstantRange pcRange{
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset = 0,
		.size = sizeof(DDGIPushConstant)
	};

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &pcRange;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &m_descriptorLayout;
	vkCreatePipelineLayout(*ctx.device, &pipeline_layout_create_info, nullptr, &m_rtPipelineLayout);

	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };

	createShaderBindingTable(ctx, rtPipelineInfo);
}

void owDDGI::setup(DeviceContext& ctx, rtHelper* rtHelper) {
	m_rtHelper = rtHelper;
	numProbes = PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH;

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
	imgInfo.format = VK_FORMAT_D16_UNORM;
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
	completeViewInfo.format = VK_FORMAT_D16_UNORM;
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

	// create other RT resources
	createRaytraceDescriptors(ctx);
	createRaytracePipeline(ctx);
}

void owDDGI::bakeDDGI() {

}