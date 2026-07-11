#include "hcinth_mesh.h"

glm::mat4 HMeshNode::getMatrix() {
	glm::mat4 nodeMatrix = transform.getMatrix();
	HMeshNode* currentParent = parent;
	while (currentParent)
	{
		nodeMatrix = currentParent->transform.getMatrix() * nodeMatrix;
		currentParent = currentParent->parent;
	}
	return nodeMatrix;
}

static void generateVectors(HMeshNode* node, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices) {
	for (const auto& p : node->primtives) {
		for (int i = 0; i < p.vertexCount; i++) {
			vertices.push_back(assetVertices[p.firstVertex + i].pos);
		}

		for (int i = 0; i < p.indexCount; i++) {
			indices.push_back(assetIndices[p.firstIndex + i]);
		}
	}
}

static void addAccelStructure(HMeshNode* node, std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices) {
	std::vector<glm::vec3> nodeVertices;
	std::vector<uint32_t> nodeIndices;

	AccelerationStructure blAccel;
	generateVectors(node, nodeVertices, nodeIndices, assetVertices, assetIndices); // populate node vertices and indices
	rtHelper::createBottomLevelAS(blAccel, nodeVertices, nodeIndices);
	blAccel.instanceMatrix = node->getMatrix();
	rt::bottomLevelStructures.push_back(blAccel);

	for (auto& n : node->children) {
		addAccelStructure(n, assetVertices, assetIndices);
	}
}

void HMesh::generateBLAccelStructures(std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices) {
	for (auto& n : parentNodes) {
		addAccelStructure(n, assetVertices, assetIndices);
	}
}