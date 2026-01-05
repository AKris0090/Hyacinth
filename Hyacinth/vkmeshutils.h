#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <vector>
#include <array>

struct Vertex {
	glm::vec4 pos;		// uvX is in w component
	glm::vec4 normal;	// uvY is in w component
	glm::vec4 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
};

struct GPUMeshBuffers {
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount;
};

struct GPUDrawPushConstants {
	VkDeviceAddress transformAddress;
};

namespace vkmeshutils {
	GPUMeshBuffers uploadMesh(DeviceContext& ctx, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);
}