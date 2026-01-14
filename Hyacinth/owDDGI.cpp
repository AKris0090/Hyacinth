#include "owDDGI.h"

void owDDGI::createRaytraceDescriptors(DeviceContext& ctx) {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2.f }, // out images
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f } // vertex and index buffer
	};

	m_descriptorAllocator.initPool(*ctx.device, 1, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // radiance image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // visibility image
		layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL); // vertex buffer
		layoutBuilder.addBinding(4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL); // index buffer
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

	VkDescriptorBufferInfo vertexBufferInfo{};

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
	uint32_t handleSize = m_rtHelper->m_rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = m_rtHelper->m_rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = m_rtHelper->m_rtProperties.shaderGroupBaseAlignment;
	uint32_t groupCount = rtPipelineInfo.groupCount;

	size_t dataSize = handleSize * groupCount;
	m_shaderHandles.resize(dataSize);
	VK_CHECK(rt::GetHandles(*ctx.device, m_rtPipeline, 0, groupCount, dataSize, m_shaderHandles.data()));

	auto     alignUp = [](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
	uint32_t raygenSize = alignUp(handleSize, handleAlignment);
	uint32_t missSize = alignUp(handleSize, handleAlignment);
	uint32_t hitSize = alignUp(handleSize, handleAlignment);
	uint32_t callableSize = 0;

	uint32_t raygenOffset = 0;
	uint32_t missOffset = alignUp(raygenSize, baseAlignment);
	uint32_t hitOffset = alignUp(missOffset + missSize, baseAlignment);
	uint32_t callableOffset = alignUp(hitOffset + hitSize, baseAlignment);

	size_t bufferSize = callableOffset + callableSize;
	m_sbtBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, bufferSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	uint8_t* pData = static_cast<uint8_t*>(m_sbtBuffer.info.pMappedData);

	memcpy(pData + raygenOffset, m_shaderHandles.data() + 0 * handleSize, handleSize);
	m_raygenRegion.deviceAddress = m_sbtBuffer.gpuAddress + raygenOffset;
	m_raygenRegion.stride = raygenSize;
	m_raygenRegion.size = raygenSize;

	memcpy(pData + missOffset, m_shaderHandles.data() + 1 * handleSize, handleSize);
	m_missRegion.deviceAddress = m_sbtBuffer.gpuAddress + missOffset;
	m_missRegion.stride = missSize;
	m_missRegion.size = missSize;

	memcpy(pData + hitOffset, m_shaderHandles.data() + 2 * handleSize, handleSize);
	m_hitRegion.deviceAddress = m_sbtBuffer.gpuAddress + hitOffset;
	m_hitRegion.stride = hitSize;
	m_hitRegion.size = hitSize;

	m_callableRegion.deviceAddress = 0;
	m_callableRegion.stride = 0;
	m_callableRegion.size = 0;
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

	stages[eRaygen] = createShader(*ctx.device, "shaders/raygen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stages[eMiss] = createShader(*ctx.device, "shaders/rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eClosestHit] = createShader(*ctx.device, "shaders/rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

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
	rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rtPipelineInfo.pGroups = shader_groups.data();
	rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, m_rtHelper->m_rtProperties.maxRayRecursionDepth);
	rtPipelineInfo.layout = m_rtPipelineLayout;
	rt::createPipeline(*ctx.device, {}, {}, 1, & rtPipelineInfo, nullptr, & m_rtPipeline);

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

	std::vector<glm::vec3> probePositions;
	for (int i = 0; i < PROBE_DENSITY_HEIGHT; i++) {
		for (int j = 0; j < PROBE_DENSITY_WIDTH; j++) {
			for (int k = 0; k < PROBE_DENSITY_DEPTH; k++) {
				probePositions.push_back(glm::vec3(xSpace * j, ySpace * k, zSpace * i));
			}
		}
	}
	VkDeviceSize probeBufferSize = probePositions.size() * sizeof(glm::vec3);
	m_probeVolume.probePositionBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, probeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	vkdeviceutils::uploadToBuffer(ctx, m_probeVolume.probePositionBuffer, probeBufferSize, probePositions.data());

	// set up textures for volume
	VkExtent3D irradianceExtent{
		.width = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_WIDTH,
		.height = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_DEPTH,
		.depth = 1
	};
	VkImageCreateInfo imgInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.format = m_irradFormat;
	imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
		.width = VISIBILITY_PIXEL_COUNT * PROBE_DENSITY_WIDTH,
		.height = VISIBILITY_PIXEL_COUNT * PROBE_DENSITY_DEPTH,
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
	completeViewInfo.format = m_irradFormat;
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

void owDDGI::bakeDDGI(DeviceContext& ctx, SceneGraph& m_scene) {
	std::cout << "baking DDGI" << std::endl;

	gltfNode* node = m_scene.objects[0].nodes[0].get();

	std::vector<DDGIVertex> vertices;
	for (const auto& v : node->vertices) {
		DDGIVertex vD{
			.pos = v.pos,
			.normal = v.normal
		};
		vertices.push_back(vD);
	}

	VkDeviceSize vertexBufferSize = vertices.size() * sizeof(DDGIVertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	m_rtHelper->vertexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	m_rtHelper->indexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);

	VulkanBuffer staging = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(staging.info.pMappedData, vertices.data(), vertexBufferSize);
	memcpy((char*)staging.info.pMappedData + vertexBufferSize, node->indices.data(), indexBufferSize);

	VkBufferCopy vertexCopyRegion{};
	vertexCopyRegion.srcOffset = 0;
	vertexCopyRegion.dstOffset = 0;
	vertexCopyRegion.size = vertexBufferSize;

	VkBufferCopy indexCopyRegion{};
	indexCopyRegion.srcOffset = vertexBufferSize;
	indexCopyRegion.dstOffset = 0;
	indexCopyRegion.size = indexBufferSize;

	vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, m_rtHelper->vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, m_rtHelper->indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(*ctx.allocator, staging);

	VkClearValue clearValues[1]{};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = 1;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = PROBE_DENSITY_HEIGHT;

	vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
		vkimageutils::transitionImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkimageutils::transitionImage(cmd, m_probeVolume.visibilityImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0, 1, &m_rtDescriptorSet, 0, nullptr);

		DDGIPushConstant pc{
			.probePositionBufferAddress = m_probeVolume.probePositionBuffer.gpuAddress,
			.vertexAddress = m_rtHelper->vertexBuffer.gpuAddress,
			.indexAddress = m_rtHelper->indexBuffer.gpuAddress,
		};
		vkCmdPushConstants(cmd, m_rtPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(DDGIPushConstant), &pc);

		rt::Trace(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, RAYS_PER_PROBE, numProbes * PROBE_DENSITY_HEIGHT, 1);
	});
}