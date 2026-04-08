#pragma once

#include "hyacinth_network.h"
#include "vkpipelineutils.h"
// #include "gltfutils.h"
#include <unordered_map>
#include "fpcam.h"
#include <mutex>
#include <utility>

constexpr float DIFF_THRESHOLD = 0.01f;

class PacketBuffer {
public:
	float timeAggregate;
	std::pair<ServerPacket, ServerPacket> packetBuffer;

	void newPacket(ServerPacket p) {
		packetBuffer.first = packetBuffer.second;
		packetBuffer.second = p;
		timeAggregate = 0.f;
	}

	void fixServerRecon(Entity* e, glm::vec3 newPos) {
		ServerPacket sP;
		sP.entities.push_back(*e);
		sP.entities[0].transform = e->transform;
		sP.entities[0].transform.position = newPos;
		newPacket(sP);
		newPacket(sP);

		std::cout << "reset" << std::endl;
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

struct StateStorage {
	uint32_t tickNum;
	Transform state;
	float fb, lr;
};

class RewindBuffer {
public:
	using PhysicsPosFn = std::function<void(glm::vec3 p)>;
	using PhysicsStepFn = std::function<void(Transform& t, float fb, float lr)>;
	std::deque<StateStorage> ringBuffer;

	void setPhysicsPosition(PhysicsPosFn fn) {
		physicsPosition = fn;
	}

	void setPhysicsStep(PhysicsStepFn fn) {
		physicsStep = fn;
	}

	void addState(Transform t, float fb, float lr, uint32_t tickNum) {
		StateStorage sS;
		sS.tickNum = tickNum;
		sS.state = t;
		sS.fb = fb;
		sS.lr = lr;
		ringBuffer.push_back(sS);

		if (ringBuffer.size() > 15) {
			ringBuffer.pop_front();
		}
	}

	Transform rewindState(Transform newTransform, uint32_t tickNum) {
		physicsPosition(newTransform.position);


		bool skip = true;
		for (const auto& sS : ringBuffer) {
			if (sS.tickNum == tickNum) { skip = false; continue; }
			if (skip) continue;
			physicsStep(newTransform, sS.fb, sS.lr);
		}

		return newTransform;
	}

	// sp.processedTickNum is translated to client local tick
	bool checkPacketNeedsRewind(Entity* self, Transform& outTransform, ServerPacket& sp, uint32_t selfId) {
		while (!ringBuffer.empty() && (ringBuffer.front().tickNum < sp.processedTickNum)) ringBuffer.pop_front();
		if (ringBuffer.empty()) return false;

		Transform t;
		for (auto& e : sp.entities) {
			if (e.id == selfId) {
				t = e.transform;
			}
		}

		// std::cout << "f: " << ringBuffer.front().state.position.x << "," << ringBuffer.front().state.position.y << "," << ringBuffer.front().state.position.z;
		// std::cout << "s: " << t.position.x << "," << t.position.y << "," << t.position.z << std::endl;
		if (glm::length(ringBuffer.front().state.position - t.position) > DIFF_THRESHOLD) {
			std::cout << "off by: " << glm::length(ringBuffer.front().state.position - t.position) << std::endl;
			outTransform.position = rewindState(t, sp.processedTickNum).position;
			return true;
		}
		
		return false;
	}

private:
	PhysicsStepFn physicsStep;
	PhysicsPosFn physicsPosition;
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
	SimulateStruct inputAccumulator;
	PacketBuffer selfSimBuffer;
	std::mutex selfMutex;
	std::vector<uint32_t> ids;
	std::unordered_map<uint32_t, Entity*> entities;
	std::unordered_map<uint32_t, std::unique_ptr<std::mutex>> entityMutexes;
	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;
	PacketBuffer packetBuffer;
	RewindBuffer rB;

	void setupFromServerPacket(ServerPacket& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerPacket& p, uint32_t currentClientID);
	void update();
	void drawEntities(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void drawFPCharacter(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void shutdown();
};