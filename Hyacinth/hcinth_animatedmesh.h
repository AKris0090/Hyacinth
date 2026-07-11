#pragma once

#include <string>
#include <stdexcept>
#include "mesh_common.h"
#include "glm/gtc/quaternion.hpp"
#include "transform.h"

class HSkinnedMeshNode {
public:
	bool							hasSkinnedMesh = false;

	uint32_t						nodeIndex;
	std::string						nodeName;
	Transform						transform;
	HSkinnedMeshNode*				parent;
	std::vector<HSkinnedMeshNode*>	children;
	std::vector<HMeshPrim>			primtives;

	glm::quat						queuedQuatRotation;
	std::vector<float>				queuedYawShifts;
	std::vector<float>				queuedPitchShifts;

	bool upperBody = false;
	bool lowerBody = false;

	// refers to indices in the global buffer
	uint32_t firstIndex;
	uint32_t indexCount;

	glm::mat4 getMatrix();
};

struct HAnimSampler {
	std::string            interpolation;
	std::vector<float>     inputs;
	std::vector<glm::vec4> outputsVec4;
};

struct HAnimChannel {
	std::string	path;
	HSkinnedMeshNode*	node;
	uint32_t    samplerIndex;
};

struct HAnimation {
	std::vector<HAnimSampler>		samplers;
	std::vector<HAnimChannel>		channels;
	float							start = (std::numeric_limits<float>::max)();
	float							end = (std::numeric_limits<float>::min)();
};

struct HSkin {
	std::vector<glm::mat4>				inverseBindMatrices;
	std::vector<HSkinnedMeshNode*>		joints;
};

static bool isParentOf(HSkinnedMeshNode* search, HSkinnedMeshNode* target);

class HSkinnedMesh {
public:
	std::string meshName;
	uint32_t numNodes = 0;
	uint32_t numMeshedNodes = 0;

	// refer to index in the global buffer
	uint32_t firstIndex;
	uint32_t vertexOffset;
	uint32_t indexCount;

	uint32_t meshID;

	std::vector<HSkinnedMeshNode*> parentNodes;
	std::vector<HSkinnedMeshNode*> meshedNodes;

	VkDeviceSize jointMatrixSize;
	std::vector<HAnimation> animations;
	HSkin skin;
	HSkinnedMeshNode* meshOwnerNode;

	HSkinnedMeshNode* getNodeByIndex(uint32_t index);
	HSkinnedMeshNode* getNodeByName(std::string nodeName);
};