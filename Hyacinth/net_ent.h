#pragma once

#include "hyacinth_network.h"
#include "vkpipelineutils.h"
// #include "gltfutils.h"
#include <unordered_map>

struct gltfObject;

class NetworkEntityManager {
	std::vector<uint32_t> ids;
	std::unordered_map<uint32_t, Entity*> entities;

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	VulkanPipelineBuilder pipelineUtil;
	uint32_t indexCount;
	std::unique_ptr<gltfObject> sphereObject;

	struct entityBufferPC {
		VkDeviceAddress entityPosBufferAddress;
	};

	VulkanBuffer entityPositionBuffer;

	void setupRenderingUtils();

public:
	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;

	void setupFromServerPacket(ServerPacket& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerPacket& p, uint32_t currentClientID);
	void update();
	void drawEntities(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void shutdown();
};