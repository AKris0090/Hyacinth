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

static void addAccelStructure(HMeshNode* node, VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress) {
	if (node->primtives.size() > 0) {
		std::vector<glm::vec3> nodeVertices;
		std::vector<uint32_t> nodeIndices;

		AccelerationStructure blAccel;

		rt::nodeAccelBuildPacket packet;
		packet.vertexAddress = vertexAddress;
		packet.indexAddress = indexAddress;
		for (const auto& p : node->primtives) {
			packet.prims.push_back(rt::nodeAccelBuildPacket::primAccel{
				.vertexOffset = p.firstVertex,
				.firstIndex = p.firstIndex,
				.numVertices = p.vertexCount,
				.numIndices = p.indexCount,
			});
		}

		rtHelper::createBottomLevelAS(blAccel, packet);
		blAccel.instanceMatrix = node->getMatrix();
		rt::bottomLevelStructures.push_back(blAccel);
	}
	else {
		std::cout << "[RAYTRACING] Skipping BLAS generation for node: " << node->nodeName << " at index: " << node->nodeIndex   << std::endl;
	}

	for (auto& n : node->children) {
		addAccelStructure(n, vertexAddress, indexAddress);
	}
}

void HMesh::generateBLAccelStructures(VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress) {
	for (auto& n : parentNodes) {
		addAccelStructure(n, vertexAddress, indexAddress);
	}
}