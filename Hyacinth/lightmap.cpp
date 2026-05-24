#include "lightmap.h"

void LightMapper::createLightMapDescriptors() {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.f },		// accelstructure
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },					// 1 out image (lightMap)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f },			//  2 input images (world pos, normal)
	};
	m_descriptorAllocator.initPool(3, sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
		layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		m_descriptorLayout = layoutBuilder.buildLayout(nullptr, 0);
	}

	m_lightMapDescriptorSet = m_descriptorAllocator.allocate(m_descriptorLayout);

	vkdescriptorutils::queueWriteAccelStructure(m_lightMapDescriptorSet, 0, 1, &m_rtHelper->m_tlAccelStrucutre.accel);
	vkdescriptorutils::queueWriteImage(m_lightMapDescriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_lightMapImage, VK_IMAGE_LAYOUT_GENERAL);
	vkdescriptorutils::queueWriteImage(m_lightMapDescriptorSet, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_worldPosImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkdescriptorutils::queueWriteImage(m_lightMapDescriptorSet,	3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_normalImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkdescriptorutils::flushDescriptorWrites();
}

void LightMapper::createShaderBindingTable(VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo) {
	uint32_t handleSize = m_rtHelper->m_rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = m_rtHelper->m_rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = m_rtHelper->m_rtProperties.shaderGroupBaseAlignment;
	uint32_t groupCount = rtPipelineInfo.groupCount;

	size_t dataSize = handleSize * groupCount;
	m_shaderHandles.resize(dataSize);
	VK_CHECK(rt::GetHandles(vkdeviceutils::device, m_lightMapTracePipeline.pipeline, 0, groupCount, dataSize, m_shaderHandles.data()));

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

	memcpy(pData + hitOffset, m_shaderHandles.data() + 2 * handleSize, handleSize);
	m_hitRegion.deviceAddress = m_sbtBuffer.gpuAddress + hitOffset;
	m_hitRegion.stride = hitSize;
	m_hitRegion.size = hitSize;

	m_callableRegion.deviceAddress = 0;
	m_callableRegion.stride = 0;
	m_callableRegion.size = 0;
}

void LightMapper::createLightMapPipelines()
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

	stages[eRaygen] = vkpipelineutils::createShader("shaders/lightmap_shaders/lightGen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stages[eMiss] = vkpipelineutils::createShader("shaders/lightmap_shaders/lightMiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	stages[eClosestHit] = vkpipelineutils::createShader("shaders/lightmap_shaders/lightCHit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

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

	std::array<VkDescriptorSetLayout, 1> setLayouts = { m_descriptorLayout };

	VkPushConstantRange lightMapPCRange{};
	lightMapPCRange.offset = 0;
	lightMapPCRange.size = sizeof(lightPosPC);
	lightMapPCRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &lightMapPCRange;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = setLayouts.data();
	vkCreatePipelineLayout(vkdeviceutils::device, &pipeline_layout_create_info, nullptr, &m_lightMapTracePipeline.layout);

	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rtPipelineInfo.pGroups = shader_groups.data();
	rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(1U, m_rtHelper->m_rtProperties.maxRayRecursionDepth);
	rtPipelineInfo.layout = m_lightMapTracePipeline.layout;
	rt::CreatePipeline(vkdeviceutils::device, {}, {}, 1, &rtPipelineInfo, nullptr, & m_lightMapTracePipeline.pipeline);

	createShaderBindingTable(rtPipelineInfo);

	for (auto& s : stages) {
		vkDestroyShaderModule(vkdeviceutils::device, s.module, nullptr);
	}

	m_posNormalPipeline.addShader("shaders/lightmap_shaders/posNormalVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	m_posNormalPipeline.addShader("shaders/lightmap_shaders/posNormalFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_posNormalPipeline.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_posNormalPipeline.setDefaultAttributes();
	m_posNormalPipeline.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_posNormalPipeline.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_posNormalPipeline.setColorAttachmentFormat(m_posNormalFormat, 2);
	m_posNormalPipeline.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
	m_posNormalPipeline.disableBlending();
	m_posNormalPipeline.numColorAttachments = 2;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)LIGHTMAP_SIZE;
	viewport.height = (float)LIGHTMAP_SIZE;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	m_lightMapExtent.height = LIGHTMAP_SIZE;
	m_lightMapExtent.width = LIGHTMAP_SIZE;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_lightMapExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	m_posNormalPipeline.m_viewportState.pViewports = &viewport;
	m_posNormalPipeline.m_viewportState.pScissors = &scissor;

	VkPushConstantRange posNormPCRange{};
	posNormPCRange.offset = 0;
	posNormPCRange.size = sizeof(PosNormPC);
	posNormPCRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &posNormPCRange;

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &m_posNormalPipeline.m_pipeline.layout));

	m_posNormalPipeline.buildPipeline();
}

void LightMapper::setup(rtHelper* rtHelper) {
	this->m_rtHelper = rtHelper;

	VkExtent3D lightMapExtent3D {
		.width = LIGHTMAP_SIZE,
		.height = LIGHTMAP_SIZE,
		.depth = 1
	};

	VkExtent3D worldPosNormExtent3D{
		.width = LIGHTMAP_SIZE,
		.height = LIGHTMAP_SIZE,
		.depth = 1
	};

	m_lightMapImage = vkimageutils::createImageandView(lightMapExtent3D, 1, m_lightMapFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "lightmap_image");
	m_worldPosImage = vkimageutils::createImageandView(worldPosNormExtent3D, 1, m_posNormalFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT, false, "lightmap_world_pos_image");
	m_normalImage = vkimageutils::createImageandView(worldPosNormExtent3D, 1, m_posNormalFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT, false, "lightmap_normal_image");
	VkSamplerCreateInfo samplerInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = samplerInfo.addressModeU;
	samplerInfo.addressModeW = samplerInfo.addressModeU;
	samplerInfo.maxAnisotropy = 1.0f;
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_worldPosImage.imageSampler));
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_normalImage.imageSampler));

	// samplerInfo.magFilter = VK_FILTER_LINEAR;
	// samplerInfo.minFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_lightMapImage.imageSampler));

	createLightMapDescriptors();
	createLightMapPipelines();
}

void LightMapper::bakeLightMap(VkBuffer& staticDrawBuffer, uint32_t numStaticDraws, VkDeviceAddress& drawDataAddress, VkDeviceAddress& transformAddress, VkBuffer& vertexBuffer, VkBuffer& indexBuffer) {
	VkClearValue posClear{};
	posClear.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	VkClearValue normClear{};
	normClear.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	VkClearValue lightMapClear{};
	lightMapClear.color = { { 0.0f } };

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.layerCount = 1;
	subResourceRange.levelCount = 1;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_lightMapExtent.width);
	viewport.height = static_cast<float>(m_lightMapExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_lightMapExtent;

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkimageutils::transitionImage(cmd, m_worldPosImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkimageutils::transitionImage(cmd, m_normalImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		PosNormPC pc{};
		pc.drawDataAddress = drawDataAddress;
		pc.transformAddress = transformAddress;

		lightPosPC lPc{};
		lPc.lightPos = glm::vec4(-2.f, 12.f, -6.f, 1.f);

		{
			VK_LABEL(cmd, "WorldPosNormal Pass");

			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
			vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_posNormalPipeline.m_pipeline.pipeline);
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			vkCmdPushConstants(cmd, m_posNormalPipeline.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PosNormPC), &pc);

			VkRenderingAttachmentInfo posAttachment = vkimageutils::createColorAttachmentInfo(m_worldPosImage.imageView, posClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo normalAttachment = vkimageutils::createColorAttachmentInfo(m_normalImage.imageView, normClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			std::array<VkRenderingAttachmentInfo, 2> colorAttachments = { posAttachment, normalAttachment };
			VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_lightMapExtent, 2, colorAttachments.data(), nullptr);
			vkCmdBeginRendering(cmd, &renderingInfo);

			vkCmdDrawIndexedIndirect(cmd, staticDrawBuffer, 0, numStaticDraws, sizeof(VkDrawIndexedIndirectCommand));
			vkCmdEndRendering(cmd);
			VK_LABEL_END(cmd);
		}

		vkimageutils::transitionImage(cmd, m_worldPosImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkimageutils::transitionImage(cmd, m_normalImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		vkimageutils::transitionImage(cmd, m_lightMapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, m_lightMapImage.image, VK_IMAGE_LAYOUT_GENERAL, &lightMapClear.color, 1, &subResourceRange);
		
		{
			VK_LABEL(cmd, "Lightmap Trace Pass");
			std::array<VkDescriptorSet, 1> sets = { m_lightMapDescriptorSet };
		
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_lightMapTracePipeline.pipeline);
			vkCmdPushConstants(cmd, m_lightMapTracePipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(lightPosPC), &lPc);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_lightMapTracePipeline.layout, 0, 1, sets.data(), 0, nullptr);
			rt::Trace(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, LIGHTMAP_SIZE, LIGHTMAP_SIZE, 1);
			VK_LABEL_END(cmd);
		}
		
		vkimageutils::transitionImage(cmd, m_lightMapImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	});
}