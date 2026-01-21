#include "owDDGI.h"

void owDDGI::createRaytraceDescriptors(DeviceContext& ctx) {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4.f }, // out images (2 for rayData/irradiancce, 2 for compute version)
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f }, // vertex and index buffer
	};

	m_descriptorAllocator.initPool(*ctx.device, 2, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // rayData image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL); // vertex buffer
		layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL); // index buffer
		m_descriptorLayout = layoutBuilder.buildLayout(*ctx.device, nullptr, 0);
	}

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // rayData image
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // irradiance image
		m_computeDescriptorLayout = layoutBuilder.buildLayout(*ctx.device, nullptr, 0);
	}

	m_rtDescriptorSet = m_descriptorAllocator.allocate(*ctx.device, m_descriptorLayout);
	m_computeDescriptorSet = m_descriptorAllocator.allocate(*ctx.device, m_computeDescriptorLayout);

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

	VkDescriptorImageInfo rayDataImageInfo{};
	rayDataImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	rayDataImageInfo.imageView = m_probeVolume.rayDataImage.imageView;

	VkWriteDescriptorSet rayDataWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	rayDataWrite.dstSet = m_rtDescriptorSet;
	rayDataWrite.dstBinding = 1;
	rayDataWrite.dstArrayElement = 0;
	rayDataWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rayDataWrite.descriptorCount = 1;
	rayDataWrite.pImageInfo = &rayDataImageInfo;

	std::array<VkWriteDescriptorSet, 1> writes = { rayDataWrite, };
	vkUpdateDescriptorSets(*ctx.device, 1, writes.data(), 0, nullptr);


	rayDataWrite.dstBinding = 0;
	rayDataWrite.dstSet = m_computeDescriptorSet;

	VkDescriptorImageInfo irradianceImageInfo{};
	irradianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	irradianceImageInfo.imageView = m_probeVolume.irradianceImage.imageView;

	VkWriteDescriptorSet irradianceWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	irradianceWrite.dstSet = m_computeDescriptorSet;
	irradianceWrite.dstBinding = 1;
	irradianceWrite.dstArrayElement = 0;
	irradianceWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	irradianceWrite.descriptorCount = 1;
	irradianceWrite.pImageInfo = &irradianceImageInfo;

	std::array<VkWriteDescriptorSet, 2> computeWrites = { rayDataWrite, irradianceWrite };
	vkUpdateDescriptorSets(*ctx.device, 2, computeWrites.data(), 0, nullptr);
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
	uint32_t missSize = alignUp(handleSize * 2, handleAlignment);
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
	memcpy(pData + missOffset + handleSize, m_shaderHandles.data() + 2 * handleSize, handleSize);
	m_missRegion.deviceAddress = m_sbtBuffer.gpuAddress + missOffset;
	m_missRegion.stride = handleSize;
	m_missRegion.size = missSize;

	memcpy(pData + hitOffset, m_shaderHandles.data() + 3 * handleSize, handleSize);
	m_hitRegion.deviceAddress = m_sbtBuffer.gpuAddress + hitOffset;
	m_hitRegion.stride = hitSize;
	m_hitRegion.size = hitSize;

	m_callableRegion.deviceAddress = 0;
	m_callableRegion.stride = 0;
	m_callableRegion.size = 0;
}

void owDDGI::createRaytracePipeline(DeviceContext& ctx, VkDescriptorSetLayout& textureLayout)
{
	enum StageIndices
	{
		eRaygen,
		eMiss,
		eProbeMiss,
		eClosestHit,
		eShaderGroupCount
	};
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	for (auto& s : stages) {
		s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}

	stages[eRaygen] = createShader(*ctx.device, "shaders/raygen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stages[eMiss] = createShader(*ctx.device, "shaders/rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eProbeMiss] = createShader(*ctx.device, "shaders/probeMiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
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

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eProbeMiss;
	shader_groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	shader_groups.push_back(group);

	VkPushConstantRange ddgiPCRange{
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		.offset = 0,
		.size = sizeof(ddgiPushConstant)
	};

	std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorLayout, textureLayout };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &ddgiPCRange;
	pipeline_layout_create_info.setLayoutCount = 2;
	pipeline_layout_create_info.pSetLayouts = setLayouts.data();
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

void owDDGI::createComputeResources(DeviceContext& ctx) {
	VkPipelineLayoutCreateInfo pipeLineLayoutCInfo{};
	pipeLineLayoutCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLineLayoutCInfo.setLayoutCount = 1;
	pipeLineLayoutCInfo.pSetLayouts = &m_computeDescriptorLayout;
	pipeLineLayoutCInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(*ctx.device, &pipeLineLayoutCInfo, nullptr, &m_irradianceComputePipelineLayout) != VK_SUCCESS) {
		std::cout << "nah you buggin on dis compute shit" << std::endl;
		std::_Xruntime_error("Failed to create brdfLUT pipeline layout!");
	}

	VkPipelineShaderStageCreateInfo compute = createShader(*ctx.device, "shaders/irradianceComp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

	VkPipelineShaderStageCreateInfo computeStageCInfo{};
	computeStageCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeStageCInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeStageCInfo.module = compute.module;
	computeStageCInfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCInfo{};
	computePipelineCInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCInfo.stage = computeStageCInfo;

	computePipelineCInfo.layout = m_irradianceComputePipelineLayout;

	vkCreateComputePipelines(*ctx.device, VK_NULL_HANDLE, 1, &computePipelineCInfo, nullptr, &m_irradianceComputePipeline);
}

void owDDGI::setup(DeviceContext& ctx, rtHelper* rtHelper, VkDescriptorSetLayout& textureLayout) {
	m_rtHelper = rtHelper;
	numProbes = PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH;

	// test volume for sponza
	m_probeVolume.transform.position = glm::vec3(-16.044f, -1.4202f, -9.08f);
	m_probeVolume.transform.scale = glm::vec3(31.855, 13.78, 18.87);

	// evenly disperse probes TODO: figure out why volume isn't matching up with blender
	float xSpace = m_probeVolume.transform.scale.x / PROBE_DENSITY_WIDTH;
	float ySpace = m_probeVolume.transform.scale.y / PROBE_DENSITY_HEIGHT;
	float zSpace = m_probeVolume.transform.scale.z / PROBE_DENSITY_DEPTH;

	glm::vec3 probeSpacing = glm::vec3(xSpace, ySpace, zSpace);
	std::cout << glm::to_string(probeSpacing) << std::endl;

	for (int i = 0; i < PROBE_DENSITY_HEIGHT; i++) {
		std::vector<std::vector<glm::vec3>> probePlane;
		for (int j = 0; j < PROBE_DENSITY_DEPTH; j++) {
			std::vector<glm::vec3> probeRow;
			for (int k = 0; k < PROBE_DENSITY_WIDTH; k++) {
				probeRow.push_back(glm::vec3(xSpace * k, ySpace * i, zSpace * j));
			}
			probePlane.push_back(probeRow);
		}
		m_probeVolume.probes.push_back(probePlane);
	}

	std::vector<glm::vec4> probePositions;
	for (int i = 0; i < PROBE_DENSITY_HEIGHT; i++) {
		for (int j = 0; j < PROBE_DENSITY_DEPTH; j++) {
			for (int k = 0; k < PROBE_DENSITY_WIDTH; k++) {
				// i controls y
				// j controls z
				probePositions.push_back(glm::vec4(m_probeVolume.probes[i][j][k] + m_probeVolume.transform.position, 1.0f));
			}
		}
	}
	VkDeviceSize probeBufferSize = probePositions.size() * sizeof(glm::vec4);
	m_probeVolume.probePositionBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, probeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	vkdeviceutils::uploadToBuffer(ctx, m_probeVolume.probePositionBuffer, probeBufferSize, probePositions.data());

	VkExtent3D rayDataExtent{
		.width = RAYS_PER_PROBE,
		.height = PROBE_DENSITY_DEPTH * PROBE_DENSITY_WIDTH,
		.depth = 1
	};

	VkImageCreateInfo imgInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.format = m_irradFormat;
	imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imgInfo.extent = rayDataExtent;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.arrayLayers = PROBE_DENSITY_HEIGHT;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.mipLevels = 1;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(*ctx.allocator, &imgInfo, &allocInfo, &m_probeVolume.rayDataImage.image, &m_probeVolume.rayDataImage.imageAllocation, nullptr));

	VkExtent3D irradianceExtent{
		.width = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_WIDTH,
		.height = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_DEPTH,
		.depth = 1
	};
	imgInfo.extent = irradianceExtent;
	VK_CHECK(vmaCreateImage(*ctx.allocator, &imgInfo, &allocInfo, &m_probeVolume.irradianceImage.image, &m_probeVolume.irradianceImage.imageAllocation, nullptr));

	VkImageViewCreateInfo completeViewInfo{};
	completeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	completeViewInfo.image = m_probeVolume.rayDataImage.image;
	completeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	completeViewInfo.format = m_irradFormat;
	completeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	completeViewInfo.subresourceRange.baseMipLevel = 0;
	completeViewInfo.subresourceRange.levelCount = 1;
	completeViewInfo.subresourceRange.baseArrayLayer = 0;
	completeViewInfo.subresourceRange.layerCount = PROBE_DENSITY_HEIGHT;

	VK_CHECK(vkCreateImageView(*ctx.device, &completeViewInfo, nullptr, &m_probeVolume.rayDataImage.imageView));

	completeViewInfo.image = m_probeVolume.irradianceImage.image;
	VK_CHECK(vkCreateImageView(*ctx.device, &completeViewInfo, nullptr, &m_probeVolume.irradianceImage.imageView));

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

	// create other RT resources
	createRaytraceDescriptors(ctx);
	createRaytracePipeline(ctx, textureLayout);
	createComputeResources(ctx);
}

void owDDGI::bakeDDGI(DeviceContext& ctx, SceneGraph& m_scene, VkDescriptorSet& textureSet) {
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
	m_probeVolume.materialIndexAddress = node->materialBuffer.gpuAddress;

	VkDeviceSize vertexBufferSize = vertices.size() * sizeof(DDGIVertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	closestHitVertexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	closestHitIndexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);

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
		vkCmdCopyBuffer(cmd, staging.buffer, closestHitVertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, closestHitIndexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(*ctx.allocator, staging);

	VkClearValue clearValues[1]{};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = 1;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = PROBE_DENSITY_HEIGHT;

	vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
		vkimageutils::transitionImage(cmd, m_probeVolume.rayDataImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.rayDataImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);

		std::array<VkDescriptorSet, 2> sets = { m_rtDescriptorSet, textureSet };

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0, 2, sets.data(), 0, nullptr);

		ddgiPushConstant ddgiPC{
			.probePositionBufferAddress = m_probeVolume.probePositionBuffer.gpuAddress,
			.vertexAddress = closestHitVertexBuffer.gpuAddress,
			.indexAddress = closestHitIndexBuffer.gpuAddress,
			.materialIndexAddress = m_probeVolume.materialIndexAddress
		};
		vkCmdPushConstants(cmd, m_rtPipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(ddgiPushConstant), &ddgiPC);

		// x should be num rays, y should be num probes per layer, z should be num probes vertically
		rt::Trace(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, RAYS_PER_PROBE, PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH, PROBE_DENSITY_HEIGHT);

		vkimageutils::transitionImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipelineLayout, 0, 1, &m_computeDescriptorSet, 0, nullptr);

		vkCmdDispatch(cmd, PROBE_DENSITY_WIDTH, PROBE_DENSITY_DEPTH, PROBE_DENSITY_HEIGHT);

		vkimageutils::transitionImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	});
}

// PROBE VISUALIZATION STUFF

void owDDGI::createProbeVisualizationStructures(DeviceContext& ctx, VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	auto spherePath = vkdebugutils::getExeDir() / "objects" / "sphere.glb";
	m_probeVis.sphereObject = gltfutils::loadFromFile(spherePath.string(), ctx);
	gltfNode* node = m_probeVis.sphereObject.nodes[0].get();
	for (const auto& p : node->primitives) {
		for (const auto& v : p.get()->vertices) {
			node->vertices.push_back(v);
		}
		for (const auto& index : p.get()->indices) {
			node->indices.push_back(index);
		}
	}
	m_probeVis.indexCount = node->indices.size();
	
	VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(Vertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	m_probeVis.vertexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	m_probeVis.indexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);

	VulkanBuffer staging = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(staging.info.pMappedData, node->vertices.data(), vertexBufferSize);
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
		vkCmdCopyBuffer(cmd, staging.buffer, m_probeVis.vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, m_probeVis.indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(*ctx.allocator, staging);


	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f }
	};
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_probeVis.visSetLayout = layoutBuilder.buildLayout(*ctx.device, nullptr, 0);
	}
	m_probeVis.visDescriptorAllocator.initPool(*ctx.device, 1, sizes);
	m_probeVis.visSet = m_probeVis.visDescriptorAllocator.allocate(*ctx.device, m_probeVis.visSetLayout);
	VkDescriptorImageInfo irradianceSamplerInfo{};
	irradianceSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	irradianceSamplerInfo.imageView = m_probeVolume.irradianceImage.imageView;
	irradianceSamplerInfo.sampler = m_probeVolume.irradianceImage.imageSampler;

	VkWriteDescriptorSet irradianceSamplerWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	irradianceSamplerWrite.dstSet = m_probeVis.visSet;
	irradianceSamplerWrite.dstBinding = 0;
	irradianceSamplerWrite.dstArrayElement = 0;
	irradianceSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	irradianceSamplerWrite.descriptorCount = 1;
	irradianceSamplerWrite.pImageInfo = &irradianceSamplerInfo;

	vkUpdateDescriptorSets(*ctx.device, 1, &irradianceSamplerWrite, 0, nullptr);


	// create probe vis pipeline
	m_probeVis.pipelineUtil.addShader(*ctx.device, "shaders/probeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	m_probeVis.pipelineUtil.addShader(*ctx.device, "shaders/probeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_probeVis.pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_probeVis.pipelineUtil.setDefaultAttributes();
	m_probeVis.pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_probeVis.pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_probeVis.pipelineUtil.setColorAttachmentFormat(SWImageFormat.format);
	m_probeVis.pipelineUtil.setMultisampling(msaaSamples);
	m_probeVis.pipelineUtil.disableBlending();

	m_probeVis.pipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
	m_probeVis.pipelineUtil.setDepthAttachmentFormat(depthFormat);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)SWImageFormat.extent.width;
	viewport.height = (float)SWImageFormat.extent.height;
	viewport.minDepth = 1.0f;
	viewport.maxDepth = 0.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = SWImageFormat.extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	m_probeVis.pipelineUtil.m_viewportState.pViewports = &viewport;
	m_probeVis.pipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(probeVisObjects::probeVisPushContant);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 2> setLayouts = { descSetLayout, m_probeVis.visSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 2;
	pipelineLayoutCInfo.pSetLayouts = setLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(*ctx.device, &pipelineLayoutCInfo, nullptr, &m_probeVis.pipelineUtil.m_pipeline.layout));

	m_probeVis.pipelineUtil.buildPipeline(*ctx.device);
}

void owDDGI::drawProbes(VkCommandBuffer& cmd, VkDescriptorSet& descSet) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &m_probeVis.vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, m_probeVis.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_probeVis.pipelineUtil.m_pipeline.pipeline);

	std::array<VkDescriptorSet, 2> sets = { descSet, m_probeVis.visSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_probeVis.pipelineUtil.m_pipeline.layout, 0, 2, sets.data(), 0, nullptr);

	probeVisObjects::probeVisPushContant pc{};
	pc.probePositionAddress = m_probeVolume.probePositionBuffer.gpuAddress;

	vkCmdPushConstants(cmd, m_probeVis.pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeVisObjects::probeVisPushContant), &pc);

	vkCmdDrawIndexed(cmd, m_probeVis.indexCount, numProbes * PROBE_DENSITY_HEIGHT, 0, 0, 0);
}