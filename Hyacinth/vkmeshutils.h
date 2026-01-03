#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
	glm::vec3 pos;
	float uvX;
	glm::vec3 normal;
	float uvY;
	glm::vec4 color;
};

struct GPUMeshBuffers {
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	VkDeviceAddress vertexBufferAddress;
	uint32_t indexCount;
};

struct GPUDrawPushConstants {
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBuffer;
};

namespace vkmeshutils {
	GPUMeshBuffers uploadMesh(VkDevice& dev, VmaAllocator& alloc, VkCommandBuffer& cmd, VkQueue& gQueue, VkFence& uploadFence, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);
}