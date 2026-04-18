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
	using PhysicsStepFn = std::function<void(Transform& t, float fb, float lr)>;
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

	void addState(Transform t, float fb, float lr, uint32_t tickNum) {
		rBMutex.lock();
		StateStorage sS;
		sS.tickNum = tickNum;
		sS.state = t;
		sS.fb = fb;
		sS.lr = lr;
		ringBuffer.push_back(sS);
		rBMutex.unlock();
	}

	std::pair<Transform, Transform> rewindState(Transform newTransform, uint32_t tickNum) {
		rBMutex.lock();
		physicsPosition(newTransform.position);

		Transform prev;

		bool skip = true;
		std::cout << "rewinded ticks: ";
		for (int i = 0; i < ringBuffer.size(); i++) {
			StateStorage& sS = ringBuffer[i];
			if (sS.tickNum == tickNum) { skip = false; sS.state = newTransform; continue; }
		 	if (skip) continue;
			std::cout << sS.tickNum << " ";
			newTransform.pitch = sS.state.pitch;
			newTransform.yaw = sS.state.yaw;
			newTransform.setRotationPitchYaw();
		 	physicsStep(newTransform, sS.fb, sS.lr);
		 	sS.state = newTransform;
			if (i == ringBuffer.size() - 2) {
				prev = newTransform;
			}
		}
		std::cout << std::endl;

		rBMutex.unlock();
		return std::pair<Transform, Transform>(prev, newTransform);
	}

	// sp.processedTickNum is translated to client local tick
	bool checkPacketNeedsRewind(Entity* self, std::pair<Transform, Transform>& outTransform, Transform serverTransform, uint32_t processedTickNum) {
		while (!ringBuffer.empty() && ringBuffer.front().tickNum != processedTickNum) {
			ringBuffer.pop_front();
		}

		if (ringBuffer.empty()) return false;

		int ind = -1;
		int index = 0;
		for (auto& pack : ringBuffer) {
			if (pack.tickNum == processedTickNum) {
				ind = index;
				break;
			}
			index++;
		}
		
		float diffPitch = serverTransform.pitch - ringBuffer[ind].state.pitch;
		float diffYaw = serverTransform.yaw - ringBuffer[ind].state.yaw;
		// std::cout << "tick: " << processedTickNum << " | " << "serv: " << serverTransform.pitch << ", " << serverTransform.yaw << " | " << "client: " << ringBuffer[ind].state.pitch << ", " << ringBuffer[ind].state.yaw << " | " << "diff: " << diffPitch << ", " << diffYaw << std::endl;

		if (glm::length(ringBuffer[ind].state.position - serverTransform.position) > DIFF_THRESHOLD) {
			// std::cout << "rewinded!" << std::endl;
			// 
			std::cout << "tick: " << processedTickNum << " | ";
			for (auto& pack : ringBuffer) {
				std::cout << "|" << pack.tickNum << ":" << (glm::length(pack.state.position - serverTransform.position) <= DIFF_THRESHOLD);
			}
			std::cout << "|" << std::endl;
			std::cout << "diff: " << glm::length(ringBuffer[ind].state.position - serverTransform.position) << std::endl;
			outTransform = rewindState(serverTransform, processedTickNum);
			return true;
		}
		else {
			// std::cout << "length: " << glm::length(ringBuffer[ind].state.position - t.position) << std::endl;
		}
		
		// std::cout << "passed!" << std::endl;
		
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
	std::mutex inputAccumulatorMutex;
	PacketBuffer selfSimBuffer;
	std::mutex selfMutex;
	std::vector<uint32_t> ids;
	std::unordered_map<uint32_t, Entity*> entities;
	std::unordered_map<uint32_t, std::unique_ptr<std::mutex>> entityMutexes;
	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;
	PacketBuffer packetBuffer;
	RewindBuffer rB;
	int tickOffset;


	void setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID);
	void update();
	void drawEntities(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void drawFPCharacter(VkCommandBuffer& cmd, VkDescriptorSet& uniformSet);
	void shutdown();

	void clearPendingPackets(Entity* self) {
		while (!rB.pendingPackets.empty()) {
			auto& pending = rB.pendingPackets.front();

			if (pending.processedTickNum < rB.ringBuffer.front().tickNum) {
				rB.pendingPackets.pop();
				continue;
			}

			bool found = false;
			StateStorage sS;
			for (auto& s : rB.ringBuffer)
				if (s.tickNum == pending.processedTickNum) { found = true; sS = s; break; }
			if (!found) break;

			Transform serverTransform;
			for (auto& e : pending.entities) {
				if (e.id == self->id) {
					serverTransform = e.transform;
				}
			}

			std::pair<Transform, Transform> t;
			if (rB.checkPacketNeedsRewind(self, t, serverTransform, sS.tickNum)) {
				selfSimBuffer.fixServerRecon(self, t);
			}
			rB.pendingPackets.pop();
		}
	}
};