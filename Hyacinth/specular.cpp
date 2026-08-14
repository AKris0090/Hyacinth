#include "specular.h"

void SpecularTraceHelper::createRayTraceDescriptors(VulkanImage& skyboxImage) {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f }, // accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },				// out images (2 for rayData/irradiancce, 2 for compute version)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f }, // skybox image for miss shader
	};

	m_descriptorAllocator.initPool(6, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL); // specular write image
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR);
		writeSpecularLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		specularSamplerLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	specularSamplerSets.resize(MAX_FRAMES_IN_FLIGHT);
	writeSpecularSets.resize(MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		specularSamplerSets[i] = m_descriptorAllocator.allocate(specularSamplerLayout);
		writeSpecularSets[i] = m_descriptorAllocator.allocate(writeSpecularLayout);

		vkdescriptorutils::queueWriteAccelStructure(writeSpecularSets[i], 0, 1, &m_rtHelper->m_tlAccelStrucutre.accel);
		vkdescriptorutils::queueWriteImage(writeSpecularSets[i], 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, specularImages[i], VK_IMAGE_LAYOUT_GENERAL);
		vkdescriptorutils::queueWriteImage(writeSpecularSets[i], 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, skyboxImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkdescriptorutils::queueWriteImage(specularSamplerSets[i], 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specularImages[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	vkdescriptorutils::flushDescriptorWrites();
}

void SpecularTraceHelper::createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
{
	uint32_t handleSize = rt::s_rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = rt::s_rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = rt::s_rtProperties.shaderGroupBaseAlignment;
	uint32_t groupCount = rtPipelineInfo.groupCount;

	size_t dataSize = handleSize * groupCount;
	m_shaderHandles.resize(dataSize);
	VK_CHECK(rt::GetHandles(vkdeviceutils::device, rtPipeline.pipeline, 0, groupCount, dataSize, m_shaderHandles.data()));

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
	raygenRegion.deviceAddress = m_sbtBuffer.gpuAddress + raygenOffset;
	raygenRegion.stride = raygenSize;
	raygenRegion.size = raygenSize;

	memcpy(pData + missOffset, m_shaderHandles.data() + 1 * handleSize, handleSize);
	memcpy(pData + missOffset + handleSize, m_shaderHandles.data() + 2 * handleSize, handleSize); // 2 miss shaders
	missRegion.deviceAddress = m_sbtBuffer.gpuAddress + missOffset;
	missRegion.stride = handleSize;
	missRegion.size = missSize;

	memcpy(pData + hitOffset, m_shaderHandles.data() + 3 * handleSize, handleSize);
	hitRegion.deviceAddress = m_sbtBuffer.gpuAddress + hitOffset;
	hitRegion.stride = hitSize;
	hitRegion.size = hitSize;

	callableRegion.deviceAddress = 0;
	callableRegion.stride = 0;
	callableRegion.size = 0;
}

void SpecularTraceHelper::createSpecularTracePipeline(VkDescriptorSetLayout& gBufferSetLayout, VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& textureSetLayout)
{
	enum StageIndices
	{
		eRaygen,
		eMiss,
		eBounceMiss,
		eClosestHit,
		eShaderGroupCount
	};
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	for (auto& s : stages) {
		s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}

	stages[eRaygen] = vkpipelineutils::createShader("shaders/specularRgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stages[eMiss] = vkpipelineutils::createShader("shaders/specularMiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eBounceMiss] = vkpipelineutils::createShader("shaders/specularBounceMiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eClosestHit] = vkpipelineutils::createShader("shaders/specularChit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

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
	group.generalShader = eBounceMiss;
	shader_groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	shader_groups.push_back(group);

	VkPushConstantRange ddgiPCRange{
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		.offset = 0,
		.size = sizeof(SpecularPushConstant)
	};

	std::array<VkDescriptorSetLayout, 4> setLayouts = { writeSpecularLayout, uniformSetLayout, gBufferSetLayout, textureSetLayout };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &ddgiPCRange;
	pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipeline_layout_create_info.pSetLayouts = setLayouts.data();
	vkCreatePipelineLayout(vkdeviceutils::device, &pipeline_layout_create_info, nullptr, &rtPipeline.layout);

	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rtPipelineInfo.pGroups = shader_groups.data();
	rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, rt::s_rtProperties.maxRayRecursionDepth);
	rtPipelineInfo.layout = rtPipeline.layout;
	rt::CreatePipeline(vkdeviceutils::device, {}, {}, 1, &rtPipelineInfo, nullptr, &rtPipeline.pipeline);

	createShaderBindingTable(rtPipelineInfo);

	for (auto& s : stages) {
		vkDestroyShaderModule(vkdeviceutils::device, s.module, nullptr);
	}
}

void SpecularTraceHelper::setup(rtHelper* rtHelper, VkDescriptorSetLayout& gBufferSetLayout, VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& textureSetLayout, VulkanImage& skyboxImage, VkExtent2D swapExt) {
	m_rtHelper = rtHelper;

	VkExtent3D ext{
		.width = swapExt.width,
		.height = swapExt.height,
		.depth = 1
	};

	// create specular images
	specularImages.resize(MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		specularImages[i] = vkimageutils::createImageandView(ext, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, false, "specular_image");
		vkimageutils::createImageSampler(specularImages[i]);
	}

	createRayTraceDescriptors(skyboxImage);
	createSpecularTracePipeline(gBufferSetLayout, uniformSetLayout, textureSetLayout);
}

void SpecularTraceHelper::resizeScreen(VkExtent2D swapExt) {
	VkExtent3D ext{
		.width = swapExt.width,
		.height = swapExt.height,
		.depth = 1
	};

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkimageutils::destroyImage(specularImages[i]);
	}

	specularImages.clear();
	specularImages.shrink_to_fit();
	specularImages.resize(MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		specularImages[i] = vkimageutils::createImageandView(ext, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, false, "specular_image");
		vkimageutils::createImageSampler(specularImages[i]);

		vkdescriptorutils::queueWriteImage(writeSpecularSets[i], 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, specularImages[i], VK_IMAGE_LAYOUT_GENERAL);

		vkdescriptorutils::queueWriteImage(specularSamplerSets[i], 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specularImages[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	} // flush happens on outside function call
}

void SpecularTraceHelper::traceScene(VkCommandBuffer& cmd, uint32_t swImageIndex, VkDeviceAddress& vbuff, VkDeviceAddress& iBuff, VkDeviceAddress& renderBuff, VkDeviceAddress& matBuff, VkDescriptorSet& gbuffSet, VkDescriptorSet& uniformSet, VkDescriptorSet& textureSet, glm::vec3 camPos, VkExtent2D swapChainExtent) {
	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = 1;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = 1;

	vkimageutils::transitionTexImage(cmd, specularImages[swImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, specularImages[swImageIndex].image, VK_IMAGE_LAYOUT_GENERAL, &specularClearVal.color, 1, &subResourceRange);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.pipeline);

	std::array<VkDescriptorSet, 4> sets = { writeSpecularSets[swImageIndex], uniformSet, gbuffSet, textureSet};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

	SpecularPushConstant specPC{
		.vertexBuffer = vbuff,
		.indexBuffer = iBuff,
		.renderCallBuffer = renderBuff,
		.materialBuffer = matBuff,
		.cameraPos = camPos
	};
	vkCmdPushConstants(cmd, rtPipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(SpecularPushConstant), &specPC);

	// x should be num rays, y should be num probes per layer, z should be num probes vertically
	rt::Trace(cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion, swapChainExtent.width, swapChainExtent.height, 1);
	vkimageutils::transitionTexImage(cmd, specularImages[swImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
}

void SpecularTraceHelper::shutdown() {
	vkdeviceutils::destroyBuffer(m_sbtBuffer);
	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, specularSamplerLayout, nullptr);
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, writeSpecularLayout, nullptr);
	rtPipeline.destroy();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkimageutils::destroyImage(specularImages[i]);
	}
}