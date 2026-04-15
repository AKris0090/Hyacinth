#pragma once

#include "gltfcommon.h"

static glm::mat4 getAnimatedMatrix(Transform& matP, glm::mat4& matrix) {
	return glm::translate(glm::mat4(1.0f), matP.position) * glm::mat4(matP.rotation) * glm::scale(glm::mat4(1.0f), matP.scale);// *matrix;
}

struct Skin
{
	std::string					name;
	gltfNode* skeletonRoot =	nullptr;
	std::vector<glm::mat4>		inverseBindMatrices;
	std::vector<gltfNode*>		joints;
	VulkanBuffer				jointMatrixBuffer;

	static void loadSkins(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Skin>& skinsOut);
};

struct AnimationSampler
{
	std::string            interpolation;
	std::vector<float>     inputs;
	std::vector<glm::vec4> outputsVec4;
};

struct AnimationChannel
{
	std::string path;
	gltfNode*	node;
	uint32_t    samplerIndex;
};

struct Animation
{
	std::string                   name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float                         start = std::numeric_limits<float>::max();
	float                         end = std::numeric_limits<float>::min();
	float                         currentTime = 0.0f;

	static void loadAnimations(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Animation>& animsOut);
};