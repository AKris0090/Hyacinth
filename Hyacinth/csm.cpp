#include "csm.h"

void shadowHelper::setup(DeviceContext& ctx, int maxFramesInFlight) {
	transform.position = glm::vec3(-2.f, 12.f, -6.f);

	m_cascades.resize(SHADOW_MAP_CASCADE_COUNT);
	m_uniformBuffers.resize(maxFramesInFlight);
	m_mappedUniformBuffers.resize(maxFramesInFlight);

	extent.width = cascadeImageSize;
	extent.height = cascadeImageSize;

	VkExtent3D extent3D{
		.width = cascadeImageSize,
		.height = cascadeImageSize,
		.depth = 1
	};

	VkImageCreateInfo imgInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.format = shadowFormat;
	imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.extent = extent3D;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.arrayLayers = SHADOW_MAP_CASCADE_COUNT;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.mipLevels = 1;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(*ctx.allocator, &imgInfo, &allocInfo, &m_shadowImage.image, &m_shadowImage.imageAllocation, nullptr));
	vmaSetAllocationName(*ctx.allocator, m_shadowImage.imageAllocation, "csm_shadow_image");

	VkImageViewCreateInfo completeViewInfo{};
	completeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	completeViewInfo.image = m_shadowImage.image;
	completeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	completeViewInfo.format = shadowFormat;
	completeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	completeViewInfo.subresourceRange.baseMipLevel = 0;
	completeViewInfo.subresourceRange.levelCount = 1;
	completeViewInfo.subresourceRange.baseArrayLayer = 0;
	completeViewInfo.subresourceRange.layerCount = SHADOW_MAP_CASCADE_COUNT;

	VK_CHECK(vkCreateImageView(*ctx.device, &completeViewInfo, nullptr, &m_shadowImage.imageView));

	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_shadowImage.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		viewInfo.format = shadowFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = i;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(*ctx.device, &viewInfo, nullptr, &m_cascades[i].cascadeImageView));
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
	VK_CHECK(vkCreateSampler(*ctx.device, &samplerInfo, nullptr, &m_shadowImage.imageSampler));

	// descriptor
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	};

	m_descriptorAllocator.initPool(*ctx.device, maxFramesInFlight, sizes);
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		m_descriptorSetLayout = layoutBuilder.buildLayout(*ctx.device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}
	for (int i = 0; i < maxFramesInFlight; i++) {
		// create uniform buffers
		m_uniformBuffers[i] = vkdeviceutils::createBuffer(ctx, sizeof(shadowUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "csm_uniform");
		m_mappedUniformBuffers[i] = m_uniformBuffers[i].info.pMappedData;

		m_cascades[i].uniformDescriptorSet = m_descriptorAllocator.allocate(*ctx.device, m_descriptorSetLayout);

		vkdescriptorutils::queueWriteBuffer(m_cascades[i].uniformDescriptorSet, 0, sizeof(shadowUniform), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_uniformBuffers[i]);
	}
	vkdescriptorutils::flushDescriptorWrites(*ctx.device);

	// pipeline
	m_shadowPipelineUtil.addShader(*ctx.device, "shaders/shadow.spv", VK_SHADER_STAGE_VERTEX_BIT);

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

	VK_CHECK(vkCreatePipelineLayout(*ctx.device, &pipelineLayoutCInfo, nullptr, &m_shadowPipelineUtil.m_pipeline.layout));

	m_shadowPipelineUtil.buildPipeline(*ctx.device);
}

void shadowHelper::update(FPSCam::CameraProps& cam, int currentFrame) {
	updateFrustumCorners(cam.nearClip, cam.farClip, cam.proj, cam.view);

	std::vector<glm::mat4> cascadeViewProjMatrices(SHADOW_MAP_CASCADE_COUNT);
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		cascadeViewProjMatrices[i] = m_cascades[i].viewProj;
	}
	memcpy(m_mappedUniformBuffers[currentFrame], cascadeViewProjMatrices.data(), sizeof(glm::mat4) * SHADOW_MAP_CASCADE_COUNT);
}

VkRenderingAttachmentInfo shadowHelper::getAttachmentInfo(int cascadeIndex) const
{
	VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	attachmentInfo.imageView = m_cascades[cascadeIndex].cascadeImageView;
	attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentInfo.clearValue.depthStencil.depth = 1.f;
	return attachmentInfo;
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

void shadowHelper::destroy(DeviceContext& ctx) {
	// shadow stuff
	vkDestroySampler(*ctx.device, m_shadowImage.imageSampler, nullptr);
	vkDestroyImageView(*ctx.device, m_shadowImage.imageView, nullptr);
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		vkDestroyImageView(*ctx.device, m_cascades[i].cascadeImageView, nullptr);
	}
	vmaDestroyImage(*ctx.allocator, m_shadowImage.image, m_shadowImage.imageAllocation);

	m_descriptorAllocator.destroyPool(*ctx.device);
	vkDestroyDescriptorSetLayout(*ctx.device, m_descriptorSetLayout, nullptr);

	m_shadowPipelineUtil.destroyPipeline(ctx);
}