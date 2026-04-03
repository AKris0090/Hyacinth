#pragma once

#include "hyacinth_network.h"
#include "vkpipelineutils.h"
// #include "gltfutils.h"
#include <unordered_map>
#include "fpcam.h"
#include <mutex>
#include <utility>

class PacketBuffer {
public:
	float timeAggregate;
	std::pair<ServerPacket, ServerPacket> packetBuffer;

	void newPacket(ServerPacket p) {
		packetBuffer.first = packetBuffer.second;
		packetBuffer.second = p;
		timeAggregate = 0.f;
	}

	ServerPacket getInterpolatedSimPacket(float timeDelta) {
		timeAggregate += timeDelta;
		if (timeAggregate > SERVER_TIMESTEP) {
			timeAggregate = SERVER_TIMESTEP;
		}
		float t = timeAggregate / SERVER_TIMESTEP;

		ServerPacket p;
		for (auto& e : packetBuffer.first.entities) {
			Entity secondEnt;
			secondEnt.id = 150;
			for (auto& e2 : packetBuffer.second.entities) {
				if (e2.id == e.id) {
					secondEnt = e2;
				}
			}
			if (secondEnt.id == 150) continue;
			Entity nE;
			nE.camSpeed = e.camSpeed;
			nE.moveSpeed = e.moveSpeed;
			nE.id = e.id;
			nE.transform = e.transform.lerpTo(secondEnt.transform, t);
			p.entities.push_back(nE);
		}

		return p;
	}
};

struct gltfObject;

class NetworkEntityManager {
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
	Entity* self;
	std::mutex selfMutex;
	std::vector<uint32_t> ids;
	std::unordered_map<uint32_t, Entity*> entities;
	std::unordered_map<uint32_t, std::unique_ptr<std::mutex>> entityMutexes;
	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;
	PacketBuffer packetBuffer;

	void setupFromServerPacket(ServerPacket& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerPacket& p, uint32_t currentClientID);
	void update();
	void drawEntities(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void drawFPCharacter(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void shutdown();
};