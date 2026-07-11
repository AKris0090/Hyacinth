#pragma once

#include <vector>
#include "vkdeviceutils.h"
#include "mesh_common.h"
#include "transform.h"
#include "raytracing.h"

class HMeshNode {
public:
	HMeshNode* parent;
	std::vector<HMeshNode*> children;
	std::vector<HMeshPrim> primtives;
	Transform transform;

	std::string nodeName = "";
	uint32_t nodeIndex;

	// refers to indices in the global buffer
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexCount;

	glm::mat4 getMatrix();
};

class HMesh {
public:
	std::string meshName;
	std::vector<HMeshNode*> parentNodes;
	std::vector<HMeshNode*> meshedNodes;
	uint32_t numNodes = 0;
	uint32_t numMeshedNodes = 0;

	uint32_t meshID;

	HMeshNode* meshOwnerNode; // ignore

	// refer to index in the global buffer
	uint32_t firstIndex;
	uint32_t vertexOffset;
	uint32_t indexCount;

	void generateBLAccelStructures(std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices); // would pass whole asset drawer, but then I would have to deal with backward declarations
};