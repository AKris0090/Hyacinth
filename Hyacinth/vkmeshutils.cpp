#include "vkmeshutils.h"

GPUMeshBuffers vkmeshutils::uploadMesh(DeviceContext& ctx, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices) {
	GPUMeshBuffers meshBuffers{};
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

	GPUMeshBuffers gpuMesh{};

	gpuMesh.vertexBuffer = vkdeviceutils::createBuffer(*ctx.allocator, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	gpuMesh.indexBuffer = vkdeviceutils::createBuffer(*ctx.allocator, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VulkanBuffer staging = vkdeviceutils::createBuffer(*ctx.allocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data;
	vmaMapMemory(*ctx.allocator, staging.allocation, &data);

	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	VkBufferCopy vertexCopyRegion{};
	vertexCopyRegion.srcOffset = 0;
	vertexCopyRegion.dstOffset = 0;
	vertexCopyRegion.size = vertexBufferSize;

	VkBufferCopy indexCopyRegion{};
	indexCopyRegion.srcOffset = vertexBufferSize;
	indexCopyRegion.dstOffset = 0;
	indexCopyRegion.size = indexBufferSize;

	vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
		vkCmdCopyBuffer(cmd, staging.buffer, gpuMesh.vertexBuffer.buffer, 1, &vertexCopyRegion);
		vkCmdCopyBuffer(cmd, staging.buffer, gpuMesh.indexBuffer.buffer, 1, &indexCopyRegion);
		});

	vmaUnmapMemory(*ctx.allocator, staging.allocation);
	vkdeviceutils::destroyBuffer(*ctx.allocator, staging);

	return gpuMesh;
}