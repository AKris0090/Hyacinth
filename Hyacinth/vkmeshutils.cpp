#include "vkmeshutils.h"

GPUMeshBuffers vkmeshutils::uploadMesh(VkDevice& dev, VmaAllocator& alloc, VkCommandBuffer& cmd, VkQueue& gQueue, VkFence& uploadFence, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices) {
	GPUMeshBuffers meshBuffers{};
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

	GPUMeshBuffers gpuMesh{};

	gpuMesh.vertexBuffer = vkdeviceutils::createBuffer(alloc, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	deviceAddressInfo.buffer = gpuMesh.vertexBuffer.buffer;
	gpuMesh.vertexBufferAddress = vkGetBufferDeviceAddress(dev, &deviceAddressInfo);

	gpuMesh.indexBuffer = vkdeviceutils::createBuffer(alloc, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VulkanBuffer staging = vkdeviceutils::createBuffer(alloc, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data;
	vmaMapMemory(alloc, staging.allocation, &data);	

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

	vkdeviceutils::beginCommandBuffer(cmd);
	vkCmdCopyBuffer(cmd, staging.buffer, gpuMesh.vertexBuffer.buffer, 1, &vertexCopyRegion);
	vkCmdCopyBuffer(cmd, staging.buffer, gpuMesh.indexBuffer.buffer, 1, &indexCopyRegion);
	vkdeviceutils::endSubmitCommandBuffer(cmd, dev, gQueue, uploadFence);

	vmaUnmapMemory(alloc, staging.allocation);
	vkdeviceutils::destroyBuffer(alloc, staging);

	return gpuMesh;
}