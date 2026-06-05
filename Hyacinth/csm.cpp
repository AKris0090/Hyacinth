#include "csm.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

static float boundingBoxRadius(glm::vec3 frustumCenter, glm::vec3* AABBPoints) {
	float sphereRadius = 0.f;
	for (int j = 0; j < 8; j++) {
		float dist = glm::length(AABBPoints[j] - frustumCenter);
		sphereRadius = std::max(sphereRadius, dist);
	}
	return sphereRadius;
}

struct Triangle {
	glm::vec3 points[3];
	bool culled;
};

// lightCamOrthoMin/Max are the xy bounding boxes of the shadow map's coverage area
// pointsCamView are the corners of the scene in light camera space
// code from https://github.com/walbourn/directx-sdk-samples-reworked/blob/main/CascadedShadowMaps11/CascadedShadowsManager.cpp#L494
// deadass no clue whats going on
static void computeNearFar(float& nearPlane, float& farPlane, glm::vec3 lightCamOrthoMin, glm::vec3 lightCamOrthoMax, glm::vec3* pointsCamView) {
	nearPlane = FLT_MAX;
	farPlane = -FLT_MAX;

	Triangle triangleList[16];
	int triangleCount = 1;

	triangleList[0].points[0] = pointsCamView[0];
	triangleList[0].points[1] = pointsCamView[1];
	triangleList[0].points[2] = pointsCamView[2];
	triangleList[0].culled = false;

	static const int aabbTriangleIndexes[] = {
		0,1,2,  1,2,3,
		4,5,6,  5,6,7,
		0,2,4,  2,4,6,
		1,3,5,  3,5,7,
		0,1,4,  1,4,5,
		2,3,6,  3,6,7
	};

	int pointPassesCollision[3];

	float fLightCameraOrthographicMinX = lightCamOrthoMin.x;
	float fLightCameraOrthographicMaxX = lightCamOrthoMax.x;
	float fLightCameraOrthographicMinY = lightCamOrthoMin.y;
	float fLightCameraOrthographicMaxY = lightCamOrthoMax.y;

	for (int aabbTriIter = 0; aabbTriIter < 12; ++aabbTriIter) {
		triangleList[0].points[0] = pointsCamView[aabbTriangleIndexes[aabbTriIter * 3 + 0]];
		triangleList[0].points[1] = pointsCamView[aabbTriangleIndexes[aabbTriIter * 3 + 1]];
		triangleList[0].points[2] = pointsCamView[aabbTriangleIndexes[aabbTriIter * 3 + 2]];
		triangleCount = 1;
		triangleList[0].culled = false;

		for (int frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter) {
			float edge;
			int component;

			if (frustumPlaneIter == 0) {
				edge = fLightCameraOrthographicMinX;
				component = 0;
			}
			else if (frustumPlaneIter == 1) {
				edge = fLightCameraOrthographicMaxX;
				component = 0;
			}
			else if (frustumPlaneIter == 2) {
				edge = fLightCameraOrthographicMinY;
				component = 1;
			}
			else {
				edge = fLightCameraOrthographicMaxY;
				component = 1;
			}

			for (int triIter = 0; triIter < triangleCount; ++triIter) {
				if (!triangleList[triIter].culled) {
					int insideVertCount = 0;
					glm::vec3 tempOrder;

					if (frustumPlaneIter == 0) {
						for (int triPtIter = 0; triPtIter < 3; ++triPtIter) {
							if (triangleList[triIter].points[triPtIter].x > lightCamOrthoMin.x) {
								pointPassesCollision[triPtIter] = 1;
							}
							else {
								pointPassesCollision[triPtIter] = 0;
							}
							insideVertCount += pointPassesCollision[triPtIter];
						}
					}
					else if (frustumPlaneIter == 1) {
						for (int triPtIter = 0; triPtIter < 3; ++triPtIter) {
							if (triangleList[triIter].points[triPtIter].x < lightCamOrthoMax.x) {
								pointPassesCollision[triPtIter] = 1;
							}
							else {
								pointPassesCollision[triPtIter] = 0;
							}
							insideVertCount += pointPassesCollision[triPtIter];
						}
					}
					else if (frustumPlaneIter == 2) {
						for (int triPtIter = 0; triPtIter < 3; ++triPtIter) {
							if (triangleList[triIter].points[triPtIter].y > lightCamOrthoMin.y) {
								pointPassesCollision[triPtIter] = 1;
							}
							else {
								pointPassesCollision[triPtIter] = 0;
							}
							insideVertCount += pointPassesCollision[triPtIter];
						}
					}
					else {
						for (int triPtIter = 0; triPtIter < 3; ++triPtIter) {
							if (triangleList[triIter].points[triPtIter].y < lightCamOrthoMax.y) {
								pointPassesCollision[triPtIter] = 1;
							}
							else {
								pointPassesCollision[triPtIter] = 0;
							}
							insideVertCount += pointPassesCollision[triPtIter];
						}
					}

					if (pointPassesCollision[1] && !pointPassesCollision[0]) {
						tempOrder = triangleList[triIter].points[0];
						triangleList[triIter].points[0] = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = tempOrder;
						pointPassesCollision[0] = TRUE;
						pointPassesCollision[1] = FALSE;
					}
					if (pointPassesCollision[2] && !pointPassesCollision[1]) {
						tempOrder = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = triangleList[triIter].points[2];
						triangleList[triIter].points[2] = tempOrder;
						pointPassesCollision[1] = TRUE;
						pointPassesCollision[2] = FALSE;
					}
					if (pointPassesCollision[1] && !pointPassesCollision[0]) {
						tempOrder = triangleList[triIter].points[0];
						triangleList[triIter].points[0] = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = tempOrder;
						pointPassesCollision[0] = TRUE;
						pointPassesCollision[1] = FALSE;
					}

					if (insideVertCount == 0) {
						triangleList[triIter].culled = true;
					}
					else if (insideVertCount == 1) {
						triangleList[triIter].culled = false;

						glm::vec3 vVert0ToVert1 = triangleList[triIter].points[1] - triangleList[triIter].points[0];
						glm::vec3 vVert0ToVert2 = triangleList[triIter].points[2] - triangleList[triIter].points[0];

						float fHitPointTimeRatio = edge - triangleList[triIter].points[0][component];
						float fDistanceAlongVector01 = fHitPointTimeRatio / vVert0ToVert1[component];
						float fDistanceAlongVector02 = fHitPointTimeRatio / vVert0ToVert2[component];

						vVert0ToVert1 *= fDistanceAlongVector01;
						vVert0ToVert1 += triangleList[triIter].points[0];
						vVert0ToVert2 *= fDistanceAlongVector02;
						vVert0ToVert2 += triangleList[triIter].points[0];

						triangleList[triIter].points[1] = vVert0ToVert2;
						triangleList[triIter].points[2] = vVert0ToVert1;

					}
					else if (insideVertCount == 2) {

						triangleList[triangleCount] = triangleList[triIter + 1];

						triangleList[triIter].culled = false;
						triangleList[triIter + 1].culled = false;

						glm::vec3 vVert2ToVert0 = triangleList[triIter].points[0] - triangleList[triIter].points[2];
						glm::vec3 vVert2ToVert1 = triangleList[triIter].points[1] - triangleList[triIter].points[2];

						float fHitPointTime_2_0 = edge - triangleList[triIter].points[2][component];
						float fDistanceAlongVector_2_0 = fHitPointTime_2_0 / vVert2ToVert0[component];
						vVert2ToVert0 *= fDistanceAlongVector_2_0;
						vVert2ToVert0 += triangleList[triIter].points[2];

						triangleList[triIter + 1].points[0] = triangleList[triIter].points[0];
						triangleList[triIter + 1].points[1] = triangleList[triIter].points[1];
						triangleList[triIter + 1].points[2] = vVert2ToVert0;

						float fHitPointTime_2_1 = edge - triangleList[triIter].points[2][component];
						float fDistanceAlongVector_2_1 = fHitPointTime_2_1 / vVert2ToVert1[component];
						vVert2ToVert1 *= fDistanceAlongVector_2_1;
						vVert2ToVert1 += triangleList[triIter].points[2];
						triangleList[triIter].points[0] = triangleList[triIter + 1].points[1];
						triangleList[triIter].points[1] = triangleList[triIter + 1].points[2];
						triangleList[triIter].points[2] = vVert2ToVert1;
						++triangleCount;
						++triIter;
					}
					else {
						triangleList[triIter].culled = false;

					}
				}          
			}
		}
		for (int index = 0; index < triangleCount; ++index) {
			if (!triangleList[index].culled) {
				for (int vertind = 0; vertind < 3; ++vertind) {
					float fTriangleCoordZ = triangleList[index].points[vertind].z;
					if (nearPlane > fTriangleCoordZ)
					{
						nearPlane = fTriangleCoordZ;
					}
					if (farPlane < fTriangleCoordZ)
					{
						farPlane = fTriangleCoordZ;
					}
				}
			}
		}
	}
}

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
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = samplerInfo.addressModeU;
	samplerInfo.addressModeW = samplerInfo.addressModeU;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

	samplerInfo.compareEnable = VK_TRUE;
	samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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
	m_shadowPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
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

void shadowHelper::update(Camera& cam, AABB worldSpaceSceneBouunds, int currentFrame) {
	updateFrustumCorners(cam.m_zNear, cam.m_zFar, cam.m_proj, cam.m_view, worldSpaceSceneBouunds);

	std::vector<glm::mat4> cascadeViewProjMatrices(SHADOW_MAP_CASCADE_COUNT);
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		cascadeViewProjMatrices[i] = m_cascades[i].viewProj;
		
		// update cascade frustum planes for culling
		Camera::GetFrustumPlanes(m_cascades[i].frustumPlanes, m_cascades[i].viewProj);
		memcpy(m_cascades[i].cullUniformBuffers[currentFrame].info.pMappedData, m_cascades[i].frustumPlanes, sizeof(CameraFrustumPlanes));
	}
	memcpy(m_uniformBuffers[currentFrame].info.pMappedData, cascadeViewProjMatrices.data(), sizeof(glm::mat4) * SHADOW_MAP_CASCADE_COUNT);
}

static glm::vec3 vec3mat4mul(glm::vec3 v, glm::mat4 m) {
	glm::vec4 unresolved = m * glm::vec4(v, 1.f);
	return (glm::vec3(unresolved) / unresolved.w);
}

static glm::mat4 getGlobalShadowMatrix(glm::vec3 lightDir, glm::mat4 camProj, glm::mat4 camView) {
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

	glm::mat4 invViewProj = glm::inverse(camProj * camView);

	glm::vec3 frustumCenter = glm::vec3(0.f);
	for (int i = 0; i < 8; i++) {
		frustumCorners[i] = vec3mat4mul(frustumCorners[i], invViewProj);
		frustumCenter += frustumCorners[i];
	}
	frustumCenter /= 8.0f;

	glm::vec3 shadowCamPos = frustumCenter + lightDir * -0.5f;
	glm::mat4 shadowCamProj = glm::orthoRH_ZO(-0.5f, 0.5f, -0.5f, 0.5f, 0.f, 1.f);
	glm::mat4 shadowCamView = glm::lookAt(shadowCamPos, frustumCenter, glm::vec3(0.f, 1.f, 0.f));

	glm::mat4 texScaleBias = glm::mat4(1.0f);
	texScaleBias[0][0] = 0.5f;
	texScaleBias[1][1] = 0.5f;
	texScaleBias[2][2] = 1.0f;
	texScaleBias[3][0] = 0.5f;
	texScaleBias[3][1] = 0.5f;

	return texScaleBias * (shadowCamProj * shadowCamView);
}

void shadowHelper::updateFrustumCorners(float camNear, float camFar, glm::mat4 proj, glm::mat4 view, AABB sceneWorldBounds) {
	float nearClip = camNear;
	float farClip = camFar;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	float currentSplits[SHADOW_MAP_CASCADE_COUNT];
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		currentSplits[i] = (d - nearClip) / clipRange;
	}

	glm::vec3 lightDirection = glm::normalize(transform.position);

	glm::mat4 globalShadowMatrix = getGlobalShadowMatrix(lightDirection, proj, view);
	shaderShadowMatrix = globalShadowMatrix;

	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float lastSplitDist = i == 0 ? 0.f : currentSplits[i - 1];

		float splitDist = currentSplits[i];

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
			frustumCorners[j] = vec3mat4mul(frustumCorners[j], invCam);
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

		float sphereRadius = 0.f;
		for (int j = 0; j < 8; j++) {
			float dist = glm::length(frustumCorners[j] - frustumCenter);
			sphereRadius = std::max(sphereRadius, dist);
		}

		glm::vec3 aabbMin = sceneWorldBounds.min;
		glm::vec3 aabbMax = sceneWorldBounds.max;

		glm::vec3 sceneAABBCornersLightView[8] = {
			glm::vec3(aabbMin.x, aabbMin.y, aabbMin.z),
			glm::vec3(aabbMax.x, aabbMin.y, aabbMin.z),
			glm::vec3(aabbMin.x, aabbMax.y, aabbMin.z),
			glm::vec3(aabbMax.x, aabbMax.y, aabbMin.z),
			glm::vec3(aabbMin.x, aabbMin.y, aabbMax.z),
			glm::vec3(aabbMax.x, aabbMin.y, aabbMax.z),
			glm::vec3(aabbMin.x, aabbMax.y, aabbMax.z),
			glm::vec3(aabbMax.x, aabbMax.y, aabbMax.z),
		};

		sphereRadius = std::min(sphereRadius, boundingBoxRadius(frustumCenter, sceneAABBCornersLightView));

		sphereRadius = 30.f;

		sphereRadius = std::ceil(sphereRadius * 16.f) / 16.f;

		glm::vec3 maxExtents = glm::vec3(sphereRadius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 shadowCamPos = frustumCenter + lightDirection * -minExtents;

		glm::mat4 lightViewMatrix = glm::lookAt(lightDirection * -minExtents, glm::vec3(0.f), glm::vec3(0.0f, 1.0f, 0.0f));

		// calculate tight near and far bounds ///////////////////

		for (int j = 0; j < 8; j++) {
			glm::vec4 lv = lightViewMatrix * glm::vec4(sceneAABBCornersLightView[j], 1.0f);
			sceneAABBCornersLightView[j] = glm::vec3(lv);
		}

		float tightNear, tightFar;
		computeNearFar(tightNear, tightFar, minExtents, maxExtents, sceneAABBCornersLightView);

		//////////////////////////////////////////////////////////
		glm::mat4 lightOrthoMatrix = glm::orthoRH_ZO(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -tightFar, -tightNear);

		// Store split distance and matrix in cascade
		m_cascades[i].viewProj = lightOrthoMatrix * lightViewMatrix;

		// stabilization ///////////////////
		glm::vec4 shadowOrigin = glm::vec4(0.f, 0.f, 0.f, 1.f);
		shadowOrigin = m_cascades[i].viewProj * shadowOrigin;
		shadowOrigin = shadowOrigin * (cascadeImageSize / 2.f);

		glm::vec4 roundedOrigin = glm::round(shadowOrigin);
		glm::vec4 roundedOffset = roundedOrigin - shadowOrigin;
		roundedOffset = roundedOffset * (2.f / cascadeImageSize);
		roundedOffset = glm::vec4(roundedOffset.x, roundedOffset.y, 0.f, 0.f);

		glm::mat4 shadowProj = lightOrthoMatrix;
		shadowProj[3] += roundedOffset;
		m_cascades[i].viewProj = shadowProj * lightViewMatrix;
		///////////////////////////////////

		glm::mat4 texScaleBias = glm::mat4(1.0f);
		texScaleBias[0][0] = 0.5f;
		texScaleBias[1][1] = 0.5f;
		texScaleBias[2][2] = 1.0f;
		texScaleBias[3][0] = 0.5f;
		texScaleBias[3][1] = 0.5f;

		glm::mat4 cascadeMatrix = texScaleBias * m_cascades[i].viewProj;

		const float clipDist = camFar - camNear;
		shaderSplits[i] = camNear + splitDist * clipDist;

		glm::mat4 invCascadeMat = glm::inverse(cascadeMatrix);
		glm::vec3 cascadeCorner = vec3mat4mul(glm::vec3(0.f), invCascadeMat);
		cascadeCorner = vec3mat4mul(cascadeCorner, globalShadowMatrix);

		glm::vec3 otherCorner = vec3mat4mul(glm::vec3(1.f), invCascadeMat); // project the furthest corner into the frustum local space
		otherCorner = vec3mat4mul(otherCorner, globalShadowMatrix); // transform the local frustum space into the light's local space

		glm::vec3 cascadeScale = glm::vec3(1.f) / (otherCorner - cascadeCorner);
		cascadeOffsets[i] = glm::vec4(-cascadeCorner, 0.f);
		cascadeScales[i] = glm::vec4(cascadeScale, 1.f);
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