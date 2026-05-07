#include "csm.h"

void shadowHelper::setup(int maxFramesInFlight, VkDescriptorSetLayout& cullLayout) {
	transform.position = glm::vec3(-2.f, 12.f, -6.f);

	m_uniformBuffers.resize(maxFramesInFlight);
	m_uniformDescriptorSets.resize(maxFramesInFlight);

	extent.width = cascadeImageSize;
	extent.height = cascadeImageSize;

	VkExtent3D extent3D{
		.width = cascadeImageSize,
		.height = cascadeImageSize,
		.depth = 1
	};

	m_shadowImage = vkimageutils::createImageandView(extent3D, SHADOW_MAP_CASCADE_COUNT, shadowFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT, false, "csm_image");

	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		m_cascades[i].cascadeImageView = vkimageutils::createImageView(m_shadowImage, i, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	VkSamplerCreateInfo samplerInfo { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
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
	VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &m_shadowImage.imageSampler));

	// descriptor
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (float)SHADOW_MAP_CASCADE_COUNT }, // one per cascade
	};
	m_descriptorAllocator.initPool(maxFramesInFlight + (maxFramesInFlight * SHADOW_MAP_CASCADE_COUNT), sizes);

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		m_descriptorSetLayout = layoutBuilder.buildLayout(nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}

	for (int i = 0; i < maxFramesInFlight; i++) {
		// create uniform buffers
		m_uniformBuffers[i] = vkdeviceutils::createBuffer(sizeof(shadowUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "csm_uniform");
		m_uniformDescriptorSets[i] = m_descriptorAllocator.allocate(m_descriptorSetLayout);
		vkdescriptorutils::queueWriteBuffer(m_uniformDescriptorSets[i], 0, sizeof(shadowUniform), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_uniformBuffers[i]);

		for (int j = 0; j < SHADOW_MAP_CASCADE_COUNT; j++) {
			m_cascades[j].cullUniformBuffers[i] = vkdeviceutils::createBuffer(sizeof(CameraFrustumPlanes), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "csm_uniform");
			m_cascades[j].cascadeCullDescriptorSets[i] = m_descriptorAllocator.allocate(cullLayout);
			vkdescriptorutils::queueWriteBuffer(m_cascades[j].cascadeCullDescriptorSets[i], 0, sizeof(CameraFrustumPlanes), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_cascades[j].cullUniformBuffers[i]);
		}
	}

	vkdescriptorutils::flushDescriptorWrites();

	// pipeline
	m_shadowPipelineUtil.addShader("shaders/shadow.spv", VK_SHADER_STAGE_VERTEX_BIT);

	m_shadowPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_shadowPipelineUtil.setPositionAttribute();
	m_shadowPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_shadowPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_shadowPipelineUtil.setMultisamplingNone();
	m_shadowPipelineUtil.disableBlending();

	m_shadowPipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
	m_shadowPipelineUtil.setDepthAttachmentFormat(shadowFormat);

	m_shadowPipelineUtil.m_rasterizer.depthClampEnable = VK_TRUE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) extent.width;
	viewport.height = (float) extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	m_shadowPipelineUtil.m_viewportState.pViewports = &viewport;
	m_shadowPipelineUtil.m_viewportState.pScissors = &scissor;

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(shadowGPUPushConstant);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCInfo.pushConstantRangeCount = 1;
	pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = &m_descriptorSetLayout;

	VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &m_shadowPipelineUtil.m_pipeline.layout));

	m_shadowPipelineUtil.buildPipeline();
}

void shadowHelper::setupImGui() {
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		m_imGuiSets[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(m_shadowImage.imageSampler, m_cascades[i].cascadeImageView, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
	}
}

void shadowHelper::update(Camera& cam, int currentFrame) {
	updateFrustumCorners(cam.m_zNear, cam.m_zFar, cam.m_proj, cam.m_view);

	std::vector<glm::mat4> cascadeViewProjMatrices(SHADOW_MAP_CASCADE_COUNT);
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		cascadeViewProjMatrices[i] = m_cascades[i].viewProj;
		
		// update cascade frustum planes for culling
		Camera::GetFrustumPlanes(m_cascades[i].frustumPlanes, m_cascades[i].viewProj);
		memcpy(m_cascades[i].cullUniformBuffers[currentFrame].info.pMappedData, m_cascades[i].frustumPlanes, sizeof(CameraFrustumPlanes));
	}
	memcpy(m_uniformBuffers[currentFrame].info.pMappedData, cascadeViewProjMatrices.data(), sizeof(glm::mat4) * SHADOW_MAP_CASCADE_COUNT);
}

void shadowHelper::updateFrustumCorners(float camNear, float camFar, glm::mat4 proj, glm::mat4 view) {
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = camNear;
	float farClip = camFar;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		glm::mat4 invCam = glm::inverse(proj * view);
		for (uint32_t j = 0; j < 8; j++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++) {
			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
			frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
			frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++) {
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++) {
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		float texelsPerUnit = cascadeImageSize / (radius * 2.0f);
		glm::mat4 scalarMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit));
		glm::vec3 zero = glm::vec3(0.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 lightDir = glm::normalize(-transform.position);

		glm::mat4 lookAt = glm::lookAt(lightDir, zero, up);
		lookAt = scalarMatrix * lookAt;
		glm::mat4 invLookAt = glm::inverse(lookAt);
		glm::vec4 trueFrustumCenter = lookAt * glm::vec4(frustumCenter, 1.0f);
		trueFrustumCenter.x = (float)glm::floor(trueFrustumCenter.x);
		trueFrustumCenter.y = (float)glm::floor(trueFrustumCenter.y);
		trueFrustumCenter = invLookAt * trueFrustumCenter;

		frustumCenter.x = trueFrustumCenter.x;
		frustumCenter.y = trueFrustumCenter.y;
		frustumCenter.z = trueFrustumCenter.z;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::orthoZO(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		// Store split distance and matrix in cascade
		m_cascades[i].splitDepth = (camNear + splitDist * clipRange) * -1.f;
		m_cascades[i].viewProj = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}

void shadowHelper::drawShadowMaps(VkCommandBuffer& cmd, uint32_t numDraws, uint32_t frameIndex, VkDeviceAddress& matrixBufferAddress, VkDeviceAddress& drawDataBufferAddress) {
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineUtil.m_pipeline.pipeline);

	vkimageutils::transitionImage(cmd, m_shadowImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, extent };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 0;
	renderingInfo.pStencilAttachment = nullptr;

	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		VkRenderingAttachmentInfo shadowAttachment = vkimageutils::createShadowAttachmentInfo(m_cascades[i].cascadeImageView);
		renderingInfo.pDepthAttachment = &shadowAttachment;

		VK_LABEL(cmd, "Shadow Pass");
		vkCmdBeginRendering(cmd, &renderingInfo);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_shadowPipelineUtil.m_pipeline.layout,
			0,
			1,
			&m_uniformDescriptorSets[frameIndex],
			0,
			nullptr
		);

		shadowGPUPushConstant pushConstants{};
		pushConstants.cascadeIndex = i;
		pushConstants.transformAddress = matrixBufferAddress;
		pushConstants.drawDataAddress = drawDataBufferAddress;
		vkCmdPushConstants(cmd, m_shadowPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadowGPUPushConstant), &pushConstants);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdDrawIndexedIndirect(cmd, m_cascades[i].cascadeDrawBuffer.buffer, 0, numDraws, sizeof(VkDrawIndexedIndirectCommand));
		vkCmdEndRendering(cmd);

		VK_LABEL_END(cmd);
	}

	vkimageutils::transitionImage(cmd, m_shadowImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void shadowHelper::shutdown() {
	// shadow stuff
	vkDestroySampler(vkdeviceutils::device, m_shadowImage.imageSampler, nullptr);
	vkDestroyImageView(vkdeviceutils::device, m_shadowImage.imageView, nullptr);
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		vkDestroyImageView(vkdeviceutils::device, m_cascades[i].cascadeImageView, nullptr);
		vkdeviceutils::destroyBuffer(m_cascades[i].cascadeDrawBuffer);
		for (int j = 0; j < m_uniformBuffers.size(); j++) {
			vkdeviceutils::destroyBuffer(m_cascades[i].cullUniformBuffers[j]);
		}
	}
	vmaDestroyImage(vkdeviceutils::allocator, m_shadowImage.image, m_shadowImage.imageAllocation);

	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_descriptorSetLayout, nullptr);

	m_shadowPipelineUtil.destroyPipeline();
}