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
	uint32_t primIndexOffset = 0;
	for (const auto& p : node->primtives) {
		for (int i = 0; i < p.vertexCount; i++) {   
			vertices.push_back(assetVertices[p.firstVertex + i].pos);
		}

		for (int i = 0; i < p.indexCount; i++) {
			indices.push_back(assetIndices[p.firstIndex + i] + primIndexOffset);
		}
		primIndexOffset += p.vertexCount;
	}
}

static void addAccelStructure(HMeshNode* node, std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices) {
	if (node->primtives.size() > 0) {
		std::vector<glm::vec3> nodeVertices;
		std::vector<uint32_t> nodeIndices;

		AccelerationStructure blAccel;
		generateVectors(node, nodeVertices, nodeIndices, assetVertices, assetIndices); // populate node vertices and indices

		rt::nodeAccelBuildPacket packet;
		packet.numVertices = static_cast<uint32_t>(nodeVertices.size());
		packet.numIndices = static_cast<uint32_t>(nodeIndices.size());

		// create buffers for gpu building
		VkDeviceSize vertexBufferSize = packet.numVertices * sizeof(glm::vec3);
		VkDeviceSize indexBufferSize = packet.numIndices * sizeof(uint32_t);

		VulkanBuffer vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0, "node_accel_build_vertex");
		VulkanBuffer indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0, "node_accel_build_vertex");

		vkdeviceutils::uploadToBuffer(vertexBuffer, vertexBufferSize, nodeVertices.data(), 0);
		vkdeviceutils::uploadToBuffer(indexBuffer, indexBufferSize, nodeIndices.data(), 0);

		packet.vertexAddress = vertexBuffer.gpuAddress;
		packet.indexAddress = indexBuffer.gpuAddress;

		rtHelper::createBottomLevelAS(blAccel, packet);
		blAccel.instanceMatrix = node->getMatrix();
		rt::bottomLevelStructures.push_back(blAccel);

		vkdeviceutils::destroyBuffer(vertexBuffer);
		vkdeviceutils::destroyBuffer(indexBuffer);
	}

	for (auto& n : node->children) {
		addAccelStructure(n, assetVertices, assetIndices);
	}
}

void HMesh::generateBLAccelStructures(std::vector<Vertex>& assetVertices, std::vector<uint32_t>& assetIndices) {
	for (auto& n : parentNodes) {
		addAccelStructure(n, assetVertices, assetIndices);
	}
}