#pragma once

#include "hyacinth_network.h"
#include "vkpipelineutils.h"
#include "gltfutils.h"
#include <unordered_map>
#include "fpcam.h"
#include <mutex>
#include <utility>

constexpr float DIFF_THRESHOLD = 0.01f;

class PacketBuffer {
public:
	float timeAggregate;
	std::pair<ServerSnapshot, ServerSnapshot> packetBuffer;
	std::mutex packetBufferMutex;

	void newPacket(ServerSnapshot p, bool resetTime = true) {
		packetBufferMutex.lock();
		packetBuffer.first = packetBuffer.second;
		packetBuffer.second = p;
		if (resetTime) timeAggregate = 0.f;
		packetBufferMutex.unlock();
	}

	void fixServerRecon(Entity* e, std::pair<Transform, Transform> newPacketBuffer) {
		packetBuffer.first.entities[0].transform = newPacketBuffer.first;
		packetBuffer.second.entities[0].transform = newPacketBuffer.second;
	}

	ServerSnapshot getInterpolatedSimPacket(float timeDelta) {
		timeAggregate += timeDelta;
		if (timeAggregate > SERVER_TIMESTEP) {
			timeAggregate = SERVER_TIMESTEP;
		}
		float t = timeAggregate / SERVER_TIMESTEP;

		packetBufferMutex.lock();
		ServerSnapshot p;
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
			nE.isMoving = e.isMoving || secondEnt.isMoving;
			p.entities.push_back(nE);
		}
		packetBufferMutex.unlock();
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
	using PhysicsStepFn = std::function<void(Transform& t, int8_t fb, int8_t lr)>;
	std::deque<StateStorage> ringBuffer;
	std::mutex rBMutex;
	std::queue<ServerSnapshot> pendingPackets;
	std::mutex pendingPacketsMutex;

	void setPhysicsPosition(PhysicsPosFn fn) {
		physicsPosition = fn;
	}

	void setPhysicsStep(PhysicsStepFn fn) {
		physicsStep = fn;
	}

	void addState(Transform t, int8_t fb, int8_t lr, uint32_t tickNum) {
		rBMutex.lock();
		StateStorage sS;
		sS.tickNum = tickNum;
		sS.state = t;
		sS.fb = fb;
		sS.lr = lr;
		ringBuffer.push_back(sS);
		rBMutex.unlock();
	}

	std::pair<Transform, Transform> rewindState(Transform newTransform, uint32_t tickNum);
	bool checkPacketNeedsRewind(Entity* self, std::pair<Transform, Transform>& outTransform, Transform serverTransform, uint32_t processedTickNum); // sp.processedTickNum is translated to client local tick

private:
	PhysicsStepFn physicsStep;
	PhysicsPosFn physicsPosition;
};

struct gltfObject;
class AnimationStateMachine;

class NetworkEntityManager {
public:
	Entity* self;
	SimulateStruct inputAccumulator;
	std::mutex inputAccumulatorMutex;
	PacketBuffer selfSimBuffer;
	std::mutex selfMutex;
	std::vector<uint32_t> ids;
	std::unordered_map<uint32_t, Entity*> entities;
	std::unordered_map<uint32_t, AnimationController> entityAnimationControllers;
	std::unordered_map<uint32_t, VulkanBuffer> entityJointBuffers;
	gltfObject* characterObject;

	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;
	PacketBuffer packetBuffer;
	RewindBuffer rB;
	int tickOffset;

	void setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID, float deltaTime);
	void drawEntities(VkCommandBuffer& cmd, VulkanPipelineBuilder& pipelineUtil, uint32_t numDrawCommands, VulkanBuffer& dynamicIndirectBuffer, GPUDrawPushConstants& pc);
	void shutdown();
	void clearPendingPackets(Entity* self);
};