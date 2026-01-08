#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <glm/gtx/hash.hpp>

struct Vertex {
	glm::vec4 pos;		// uvX is in w component
	glm::vec4 normal;	// uvY is in w component
	glm::vec4 tangent;

	// for hashing for tangent generation
	bool operator==(const Vertex& other) const {
		return pos == other.pos &&
			normal == other.normal &&
			tangent == other.tangent;
	}

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
		attributeDescriptions[2].offset = offsetof(Vertex, tangent);
		return attributeDescriptions;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return (hash<glm::vec4>()(vertex.pos) ^ hash<glm::vec4>()(vertex.normal) ^ hash<glm::vec4>()(vertex.tangent));
		}
	};
}

struct GPUMeshBuffers {
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount;
};

struct GPUDrawPushConstants {
	VkDeviceAddress transformAddress;
	VkDeviceAddress materialAddress;
	VkDeviceAddress drawDataAddress;
};

namespace vkmeshutils {
	GPUMeshBuffers uploadMesh(DeviceContext& ctx, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);
}

struct MaterialPipeline {
	VkPipelineLayout    layout;
	VkPipeline          pipeline;
};

struct MaterialInstance {
	uint32_t baseColorIndex;
	uint32_t normalIndex;
	uint32_t metallicRoughnessIndex;

	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
};

struct GPUMaterialIndices {
	uint32_t baseColorIndex;
	uint32_t normalIndex;
	uint32_t metallicRoughnessIndex;
};