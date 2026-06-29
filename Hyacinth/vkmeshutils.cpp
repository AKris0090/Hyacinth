#include "vkmeshutils.h"

void vkmeshutils::uploadMesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices, std::vector<AABB>& boundingBoxes, VulkanBuffer& vertexBuffer, VulkanBuffer& indexBuffer, VulkanBuffer& aabbBuffer) {
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

	vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "final_vertex");
	indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "final_index");

	VulkanBuffer staging = vkdeviceutils::createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(staging.info.pMappedData, vertices.data(), vertexBufferSize);
	memcpy((char*)staging.info.pMappedData + vertexBufferSize, indices.data(), indexBufferSize);

	VkBufferCopy vertexCopyRegion{};
	vertexCopyRegion.srcOffset = 0;
	vertexCopyRegion.dstOffset = 0;
	vertexCopyRegion.size = vertexBufferSize;

	VkBufferCopy indexCopyRegion{};
	indexCopyRegion.srcOffset = vertexBufferSize;
	indexCopyRegion.dstOffset = 0;
	indexCopyRegion.size = indexBufferSize;

	vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vkdeviceutils::destroyBuffer(staging);

	aabbBuffer = vkdeviceutils::createBuffer(boundingBoxes.size() * sizeof(AABB), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "aabb_ssbo");
	vkdeviceutils::uploadToBuffer(aabbBuffer, boundingBoxes.size() * sizeof(AABB), boundingBoxes.data());
}