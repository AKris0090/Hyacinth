#include "owDDGI.h"

void owDDGI::createRaytraceDescriptors() {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4.f },				// out images (2 for rayData/irradiancce, 2 for compute version)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.f },
	};

	m_descriptorAllocator.initPool(2, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);							// rayData image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // irradiance sampler
		layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // visibility sampler
		m_descriptorLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL); // rayData image
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);			// irradiance image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);			// visibility image
		m_computeDescriptorLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	m_rtDescriptorSet = m_descriptorAllocator.allocate(m_descriptorLayout);
	m_computeDescriptorSet = m_descriptorAllocator.allocate(m_computeDescriptorLayout);

	vkdescriptorutils::queueWriteAccelStructure(m_rtDescriptorSet, 0, 1, &m_rtHelper->m_tlAccelStrucutre.accel);
	vkdescriptorutils::queueWriteImage(m_rtDescriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolume.rayDataImage, VK_IMAGE_LAYOUT_GENERAL);
	vkdescriptorutils::queueWriteImage(m_rtDescriptorSet, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolume.irradianceImage, VK_IMAGE_LAYOUT_GENERAL);
	vkdescriptorutils::queueWriteImage(m_rtDescriptorSet, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolume.visibilityImage, VK_IMAGE_LAYOUT_GENERAL);

	vkdescriptorutils::queueWriteImage(m_computeDescriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolume.rayDataImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::queueWriteImage(m_computeDescriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolume.irradianceImage, VK_IMAGE_LAYOUT_GENERAL);
	vkdescriptorutils::queueWriteImage(m_computeDescriptorSet, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_probeVolume.visibilityImage, VK_IMAGE_LAYOUT_GENERAL);

	vkdescriptorutils::flushDescriptorWrites();
}

void owDDGI::createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
{
	uint32_t handleSize = m_rtHelper->m_rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = m_rtHelper->m_rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = m_rtHelper->m_rtProperties.shaderGroupBaseAlignment;
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

void owDDGI::createRaytracePipeline(VkDescriptorSetLayout& textureLayout)
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

	std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorLayout, textureLayout };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &ddgiPCRange;
	pipeline_layout_create_info.setLayoutCount = 2;
	pipeline_layout_create_info.pSetLayouts = setLayouts.data();
	vkCreatePipelineLayout(vkdeviceutils::device, &pipeline_layout_create_info, nullptr, &m_rtPipeline.layout);

	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rtPipelineInfo.pGroups = shader_groups.data();
	rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, m_rtHelper->m_rtProperties.maxRayRecursionDepth);
	rtPipelineInfo.layout = m_rtPipeline.layout;
	rt::CreatePipeline(vkdeviceutils::device, {}, {}, 1, & rtPipelineInfo, nullptr, &m_rtPipeline.pipeline);

	createShaderBindingTable(rtPipelineInfo);

	for (auto& s : stages) {
		vkDestroyShaderModule(vkdeviceutils::device, s.module, nullptr);
	}
}

void owDDGI::setup(rtHelper* rtHelper, SceneGraph& m_scene, VkDescriptorSetLayout& textureLayout) {
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
	m_probeVolume.probePositionBuffer = vkdeviceutils::createBuffer(probeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_pos_ssbo");
	vkdeviceutils::uploadToBuffer(m_probeVolume.probePositionBuffer, probeBufferSize, probePositions.data());

	VkExtent3D rayDataExtent{
		.width = RAYS_PER_PROBE,
		.height = PROBE_DENSITY_DEPTH * PROBE_DENSITY_WIDTH,
		.depth = 1
	};

	m_probeVolume.rayDataImage = vkimageutils::createImageandView(rayDataExtent, PROBE_DENSITY_HEIGHT, m_irradFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_raydata_image");

	VkExtent3D irradianceExtent{
		.width = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_WIDTH,
		.height = IRRADIANCE_PIXEL_COUNT * PROBE_DENSITY_DEPTH,
		.depth = 1
	};

	m_probeVolume.irradianceImage = vkimageutils::createImageandView(irradianceExtent, PROBE_DENSITY_HEIGHT, m_irradFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_irradiance_image");

	VkExtent3D visibilityExtent{
		.width = VISIBILITY_PIXEL_COUNT * PROBE_DENSITY_WIDTH,
		.height = VISIBILITY_PIXEL_COUNT * PROBE_DENSITY_DEPTH,
		.depth = 1
	};

	m_probeVolume.visibilityImage = vkimageutils::createImageandView(visibilityExtent, PROBE_DENSITY_HEIGHT, m_depthFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "ddgi_visibility_image");

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
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_probeVolume.visibilityImage.imageSampler));
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_probeVolume.irradianceImage.imageSampler));
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_probeVolume.rayDataImage.imageSampler));

	// create other RT resources
	createRaytraceDescriptors();
	createRaytracePipeline(textureLayout);
	
	m_irradianceComputePipeline = vkpipelineutils::createComputePipeline(&m_computeDescriptorLayout, 1, nullptr, 0, "shaders/irradianceComp.spv");
	m_visibilityComputePipeline = vkpipelineutils::createComputePipeline(&m_computeDescriptorLayout, 1, nullptr, 0, "shaders/visibilityComp.spv");

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
	closestHitVertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "chit_vertex");
	closestHitIndexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "chit_index");

	VulkanBuffer staging = vkdeviceutils::createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

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

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, closestHitVertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, closestHitIndexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(staging);
}

void owDDGI::bakeDDGI(VkDescriptorSet& textureSet) {
	VkClearValue clearValues[1]{};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = 1;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = PROBE_DENSITY_HEIGHT;

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkimageutils::transitionImage(cmd, m_probeVolume.rayDataImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.rayDataImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline.pipeline);

		std::array<VkDescriptorSet, 2> sets = { m_rtDescriptorSet, textureSet };

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline.layout, 0, 2, sets.data(), 0, nullptr);

		ddgiPushConstant ddgiPC{
			.probePositionBufferAddress = m_probeVolume.probePositionBuffer.gpuAddress,
			.vertexAddress = closestHitVertexBuffer.gpuAddress,
			.indexAddress = closestHitIndexBuffer.gpuAddress,
			.materialIndexAddress = m_probeVolume.materialIndexAddress
		};
		vkCmdPushConstants(cmd, m_rtPipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(ddgiPushConstant), &ddgiPC);

		vkimageutils::transitionImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);

		vkimageutils::transitionImage(cmd, m_probeVolume.visibilityImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_probeVolume.visibilityImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValues[0].color, 1, &subResourceRange);

		// x should be num rays, y should be num probes per layer, z should be num probes vertically
		rt::Trace(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, RAYS_PER_PROBE, PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH, PROBE_DENSITY_HEIGHT);
		vkimageutils::transitionImage(cmd, m_probeVolume.rayDataImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// radiance
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradianceComputePipeline.layout, 0, 1, &m_computeDescriptorSet, 0, nullptr);

		vkCmdDispatch(cmd, PROBE_DENSITY_WIDTH, PROBE_DENSITY_DEPTH, PROBE_DENSITY_HEIGHT);

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
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_visibilityComputePipeline.layout, 0, 1, &m_computeDescriptorSet, 0, nullptr);

		vkCmdDispatch(cmd, PROBE_DENSITY_WIDTH, PROBE_DENSITY_DEPTH, PROBE_DENSITY_HEIGHT);

		vkimageutils::transitionImage(cmd, m_probeVolume.visibilityImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkimageutils::transitionImage(cmd, m_probeVolume.irradianceImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	});

	vkimageutils::destroyImage(m_probeVolume.rayDataImage);
}

// PROBE VISUALIZATION STUFF

void owDDGI::createProbeVisualizationStructures(VkDescriptorSetLayout& descSetLayout, VkFormat depthFormat, SWChainImageFormat SWImageFormat, VkSampleCountFlagBits msaaSamples) {
	auto spherePath = vkdebugutils::getExeDir() / "objects" / "sphere.glb";
	m_probeVis.sphereObject = gltfutils::loadFromFile(spherePath.string());
	gltfNode* node = m_probeVis.sphereObject.nodes[0].get();
	for (const auto& p : node->primitives) {
		for (const auto& v : p.get()->vertices) {
			node->vertices.push_back(v);
		}
		for (const auto& index : p.get()->indices) {
			node->indices.push_back(index);
		}
	}
	m_probeVis.indexCount = static_cast<uint32_t>(node->indices.size());
	
	VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(Vertex);
	VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
	m_probeVis.vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_vis_vertex");
	m_probeVis.indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "probe_vis_index");

	VulkanBuffer staging = vkdeviceutils::createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

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

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, m_probeVis.vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, m_probeVis.indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(staging);


	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f }
	};
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_probeVis.visSetLayout = layoutBuilder.buildLayout(nullptr, 0);
	}
	m_probeVis.visDescriptorAllocator.initPool(1, sizes);
	m_probeVis.visSet = m_probeVis.visDescriptorAllocator.allocate(m_probeVis.visSetLayout);

	vkdescriptorutils::queueWriteImage(m_probeVis.visSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolume.irradianceImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::queueWriteImage(m_probeVis.visSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_probeVolume.visibilityImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::flushDescriptorWrites();

	// create probe vis pipeline
	m_probeVis.pipelineUtil.addShader("shaders/probeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	m_probeVis.pipelineUtil.addShader("shaders/probeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &m_probeVis.pipelineUtil.m_pipeline.layout));

	m_probeVis.pipelineUtil.buildPipeline();
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

void owDDGI::shutdown() {
	m_rtHelper->shutdown();

	vkdeviceutils::destroyBuffer(closestHitVertexBuffer);
	vkdeviceutils::destroyBuffer(closestHitIndexBuffer);
	vkdeviceutils::destroyBuffer(m_probeVis.vertexBuffer);
	vkdeviceutils::destroyBuffer(m_probeVis.indexBuffer);
	vkdeviceutils::destroyBuffer(m_sbtBuffer);
	vkdeviceutils::destroyBuffer(m_probeVolume.probePositionBuffer);

	vkimageutils::destroyImage(m_probeVolume.irradianceImage);
	vkimageutils::destroyImage(m_probeVolume.visibilityImage);

	m_irradianceComputePipeline.destroy();
	m_visibilityComputePipeline.destroy();
	m_rtPipeline.destroy();

	m_probeVis.pipelineUtil.destroyPipeline();

	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_descriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_computeDescriptorLayout, nullptr);

	m_probeVis.visDescriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_probeVis.visSetLayout, nullptr);
}