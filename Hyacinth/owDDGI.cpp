   #include "owDDGI.h"

void owDDGI::createRaytraceDescriptors(VulkanImage& skyboxImage) {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4.f },				// out images (2 for rayData/irradiancce, 2 for compute version)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f },
	};

	m_descriptorAllocator.initPool(static_cast<uint32_t>(3 * m_probeVolumes.size()), sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);							// rayData image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR);
		m_descriptorLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL); // rayData image
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);			// irradiance image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);			// visibility image
		m_computeDescriptorLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL);			// irradiance image
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL);			// visibility image
		m_irradianceVisSetLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	for (int i = 0; i < m_probeVolumes.size(); i++) {
		m_probeVolumes[i].rayDataDescriptorSet = m_descriptorAllocator.allocate(m_descriptorLayout);
		m_probeVolumes[i].computeBuildDescriptorSet = m_descriptorAllocator.allocate(m_computeDescriptorLayout);
		m_probeVolumes[i].irradianceVisSet = m_descriptorAllocator.allocate(m_irradianceVisSetLayout);

		vkdescriptorutils::queueWriteAccelStructure(m_probeVolumes[i].rayDataDescriptorSet, 0, 1, &m_rtHelper->m_tlAccelStrucutre.accel);
		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].rayDataDescriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolumes[i].rayDataImage, VK_IMAGE_LAYOUT_GENERAL);
		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].rayDataDescriptorSet, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, skyboxImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].computeBuildDescriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolumes[i].rayDataImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].computeBuildDescriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolumes[i].irradianceImage, VK_IMAGE_LAYOUT_GENERAL);
		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].computeBuildDescriptorSet, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolumes[i].visibilityImage, VK_IMAGE_LAYOUT_GENERAL);

		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].irradianceVisSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolumes[i].irradianceImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkdescriptorutils::queueWriteImage(m_probeVolumes[i].irradianceVisSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolumes[i].visibilityImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	VkDeviceSize probeVolumeSize = sizeof(VolumeData) * m_probeVolumes.size();
	volumeDataBuffer = vkdeviceutils::createBuffer(probeVolumeSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "volume_data_ssbo");
	std::vector<VolumeData> volumeData;
	for (auto& vol : m_probeVolumes) {
		volumeData.push_back(vol.data);
	}
	vkdeviceutils::uploadToBuffer(volumeDataBuffer, probeVolumeSize, volumeData.data());

	vkdescriptorutils::flushDescriptorWrites();
}

void owDDGI::createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
{
	uint32_t handleSize = rt::s_rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = rt::s_rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = rt::s_rtProperties.shaderGroupBaseAlignment;
	uint32_t groupCount = rtPipelineInfo.groupCount;

	size_t dataSize = handleSize * groupCount;
	m_shaderHandles.resize(dataSize);
	VK_CHECK(rt::GetHandles(vkdeviceutils::device, m_rtPipeline.pipeline, 0, groupCount, dataSize, m_shaderHandles.data()));

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
	m_sbtBuffer = vkdeviceutils::createBuffer(bufferSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, "shader_binding_table");
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

void owDDGI::createRaytracePipeline(VkDescriptorSetLayout& textureSetLayout)
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

	stages[eRaygen]		= vkpipelineutils::createShader("shaders/raygen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stages[eMiss]		= vkpipelineutils::createShader("shaders/rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eProbeMiss]	= vkpipelineutils::createShader("shaders/probeMiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eClosestHit] = vkpipelineutils::createShader("shaders/rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

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

	std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorLayout, textureSetLayout };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &ddgiPCRange;
	pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipeline_layout_create_info.pSetLayouts = setLayouts.data();
	vkCreatePipelineLayout(vkdeviceutils::device, &pipeline_layout_create_info, nullptr, &m_rtPipeline.layout);

	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rtPipelineInfo.pGroups = shader_groups.data();
	rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, rt::s_rtProperties.maxRayRecursionDepth);
	rtPipelineInfo.layout = m_rtPipeline.layout;
	rt::CreatePipeline(vkdeviceutils::device, {}, {}, 1, & rtPipelineInfo, nullptr, &m_rtPipeline.pipeline);

	createShaderBindingTable(rtPipelineInfo);

	for (auto& s : stages) {
		vkDestroyShaderModule(vkdeviceutils::device, s.module, nullptr);
	}
}

void owDDGI::addVolume(glm::vec3 pos, glm::vec3 scale, uint32_t densityWidth, uint32_t densityDepth, uint32_t densityHeight, float viewBias, float normalBias) {
	DDGIVolume volume;
	volume.data.densityWidth = densityWidth;
	volume.data.densityHeight = densityHeight;
	volume.data.densityDepth = densityDepth;
	volume.data.pos = glm::vec4(pos, normalBias);

	numProbes = densityWidth * densityDepth;
	volume.totalNumProbes = numProbes * densityHeight;

	volume.transform.position = pos;
	volume.transform.rotation = glm::quat(glm::vec3(0.f)); 
	volume.transform.scale = scale;

	// evenly disperse probes
	float xSpace = volume.transform.scale.x / (densityWidth);
	float ySpace = volume.transform.scale.y / (densityHeight);
	float zSpace = volume.transform.scale.z / (densityDepth);

	glm::vec3 probeSpacing = glm::vec3(xSpace, ySpace, zSpace);
	volume.data.spacing = glm::vec4(probeSpacing, viewBias);
	volume.data.inverseSpacing = glm::vec4((1.0f / probeSpacing), 10000);

	for (uint32_t i = 0; i < densityHeight; i++) {
		std::vector<std::vector<glm::vec3>> probePlane;
		for (uint32_t j = 0; j < densityDepth; j++) {
			std::vector<glm::vec3> probeRow;
			for (uint32_t k = 0; k < densityWidth; k++) {
				probeRow.push_back(glm::vec3(xSpace * k, ySpace * i, zSpace * j));
			}
			probePlane.push_back(probeRow);
		}
		volume.probes.push_back(probePlane);
	}

	std::vector<glm::vec4> probePositions;
	for (uint32_t i = 0; i < densityHeight; i++) {
		for (uint32_t j = 0; j < densityDepth; j++) {
			for (uint32_t k = 0; k < densityWidth; k++) {
				// i controls y
				// j controls z
				probePositions.push_back(glm::vec4(volume.probes[i][j][k] + volume.transform.position, 1.0f));
			}
		}
	}
	VkDeviceSize probeBufferSize = probePositions.size() * sizeof(glm::vec4);
	volume.probePositionBuffer = vkdeviceutils::createBuffer(probeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_pos_ssbo");
	vkdeviceutils::uploadToBuffer(volume.probePositionBuffer, probeBufferSize, probePositions.data());

	VkExtent3D rayDataExtent{
		.width = static_cast<uint32_t>(volume.data.inverseSpacing.w),
		.height = densityDepth * densityWidth,
		.depth = 1
	};

	volume.rayDataImage = vkimageutils::createImageandView(rayDataExtent, densityHeight, m_irradFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_raydata_image");

	VkExtent3D irradianceExtent{
		.width = IRRADIANCE_PIXEL_COUNT * densityWidth,
		.height = IRRADIANCE_PIXEL_COUNT * densityDepth,
		.depth = 1
	};

	volume.irradianceImage = vkimageutils::createImageandView(irradianceExtent, densityHeight, m_irradFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_irradiance_image");

	VkExtent3D visibilityExtent{
		.width = VISIBILITY_PIXEL_COUNT * densityWidth,
		.height = VISIBILITY_PIXEL_COUNT * densityDepth,
		.depth = 1
	};

	volume.visibilityImage = vkimageutils::createImageandView(visibilityExtent, densityHeight, m_depthFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_visibility_image");

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
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &volume.visibilityImage.imageSampler));
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &volume.irradianceImage.imageSampler));
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &volume.rayDataImage.imageSampler));

	m_probeVolumes.push_back(volume);
}

void owDDGI::setup(rtHelper* rtHelper, VulkanImage& skyboxImage, VkDescriptorSetLayout& textureSetLayout) {
	m_rtHelper = rtHelper;

#ifdef PROBE_VOLUME_MAP_SPONZA
	glm::vec3 posA = glm::vec3(-16.044f, -1.4202f, -9.08f);
	glm::vec3 scaleA = glm::vec3(31.855, 13.78, 18.87);
	addVolume(posA, scaleA, PROBE_A_DENSITY_WIDTH, PROBE_A_DENSITY_DEPTH, PROBE_A_DENSITY_HEIGHT, 0.85f, 0.4f);
	
	glm::vec3 posB = glm::vec3(-11.144f, 3.280f, 1.650f);
	glm::vec3 scaleB = glm::vec3(23.f, 4.5f, 3.5f);
	addVolume(posB, scaleB, PROBE_B_DENSITY_WIDTH, PROBE_B_DENSITY_DEPTH, PROBE_B_DENSITY_HEIGHT, 0.4f, 0.2f);
#endif
#ifndef PROBE_VOLUME_MAP_SPONZA
	glm::vec3 posA = glm::vec3(-25.804f, -2.56f, -14.8);
	glm::vec3 scaleA = glm::vec3(50.015, 13.6, 30.25);
	addVolume(posA, scaleA, PROBE_A_DENSITY_WIDTH, PROBE_A_DENSITY_DEPTH, PROBE_A_DENSITY_HEIGHT, 0.85f, 0.4f);
#endif

	VkDeviceSize volumeBufferSize = m_probeVolumes.size() * sizeof(glm::mat4);
	m_volumeVis.volumeTransformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_volumeVis.volumeTransformBuffers[i] = vkdeviceutils::createBuffer(volumeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "volume_transform_ssbo");
	}

	// create other RT resources
	createRaytraceDescriptors(skyboxImage);
	createRaytracePipeline(textureSetLayout);
	
	VkPushConstantRange computePCRange{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(ComputePushConstant)
	};

	m_irradianceComputePipeline = vkpipelineutils::createComputePipeline(&m_computeDescriptorLayout, 1, &computePCRange, 1, "shaders/irradianceComp.spv");
	m_visibilityComputePipeline = vkpipelineutils::createComputePipeline(&m_computeDescriptorLayout, 1, &computePCRange, 1, "shaders/visibilityComp.spv");
}

void owDDGI::bakeDDGI(VkDescriptorSet& textureSet, VkDeviceAddress renderCallAddress, VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress, VkDeviceAddress materialAddress) {
	uint32_t i = 0;
	for (auto& volume : m_probeVolumes) {
		VkClearValue clearValue{};
		clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkClearValue irradianceClearValue{};
		irradianceClearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		VkImageSubresourceRange subResourceRange = {};
		subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResourceRange.baseMipLevel = 0;
		subResourceRange.levelCount = 1;
		subResourceRange.baseArrayLayer = 0;
		subResourceRange.layerCount = volume.data.densityHeight;

		vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
			VK_LABEL(cmd, "DDGI Bake");
			vkimageutils::transitionTexImage(cmd, volume.rayDataImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkCmdClearColorImage(cmd, volume.rayDataImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue.color, 1, &subResourceRange);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline.pipeline);
			
			std::array<VkDescriptorSet, 2> sets = { volume.rayDataDescriptorSet, textureSet };
			
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
			
			ddgiPushConstant ddgiPC{
				.probePositionBufferAddress = volume.probePositionBuffer.gpuAddress,
				.vertexAddress = vertexAddress,
				.indexAddress = indexAddress,
				.renderCallAddress = renderCallAddress,
				.volumeDataAddress = volumeDataBuffer.gpuAddress,
				.materialBufferAddress = materialAddress,
				.volumeIndex = i
			};
			vkCmdPushConstants(cmd, m_rtPipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(ddgiPushConstant), &ddgiPC);

			vkimageutils::transitionTexImage(cmd, volume.irradianceImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkCmdClearColorImage(cmd, volume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, &irradianceClearValue.color, 1, &subResourceRange);

			vkimageutils::transitionTexImage(cmd, volume.visibilityImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkCmdClearColorImage(cmd, volume.visibilityImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue.color, 1, &subResourceRange);

			// x should be num rays, y should be num probes per layer, z should be num probes vertically
			rt::Trace(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, static_cast<uint32_t>(volume.data.inverseSpacing.w), volume.data.densityWidth * volume.data.densityDepth, volume.data.densityHeight);
			vkimageutils::transitionTexImage(cmd, volume.rayDataImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			
			// radiance
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipeline.pipeline);
			
			ComputePushConstant compPC{
				.volumeDataAddress = volumeDataBuffer.gpuAddress,
				.volumeIndex = i
			};
			vkCmdPushConstants(cmd, m_irradianceComputePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &compPC);
			
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipeline.layout, 0, 1, &volume.computeBuildDescriptorSet, 0, nullptr);
			
			vkCmdDispatch(cmd, volume.data.densityWidth, volume.data.densityDepth, volume.data.densityHeight);
			
			VkMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				1, &barrier,
				0, nullptr,
				0, nullptr
			);
			
			// vis
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_visibilityComputePipeline.pipeline);
			
			vkCmdPushConstants(cmd, m_visibilityComputePipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &compPC);
			
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_visibilityComputePipeline.layout, 0, 1, &volume.computeBuildDescriptorSet, 0, nullptr);
			
			vkCmdDispatch(cmd, volume.data.densityWidth, volume.data.densityDepth, volume.data.densityHeight);
			
			vkimageutils::transitionTexImage(cmd, volume.visibilityImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkimageutils::transitionTexImage(cmd, volume.irradianceImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			VK_LABEL_END(cmd);
		});

		vkimageutils::destroyImage(volume.rayDataImage);
		i++;
	}
}

void owDDGI::shutdown() {
	vkdeviceutils::destroyBuffer(m_sbtBuffer);
	vkdeviceutils::destroyBuffer(volumeDataBuffer);

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkdeviceutils::destroyBuffer(m_volumeVis.volumeTransformBuffers[i]);
	}

	for(auto& volume : m_probeVolumes) {
		vkdeviceutils::destroyBuffer(volume.probePositionBuffer);
		vkimageutils::destroyImage(volume.irradianceImage);
		vkimageutils::destroyImage(volume.visibilityImage);
	}

	m_irradianceComputePipeline.destroy();
	m_visibilityComputePipeline.destroy();
	m_rtPipeline.destroy();

	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_descriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_computeDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_irradianceVisSetLayout, nullptr);

	m_probeVis.destroy();
	m_volumeVis.destroy();
}