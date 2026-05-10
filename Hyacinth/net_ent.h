#pragma once

#include "hyacinth_network.h"
#include "vkpipelineutils.h"
#include "gltfutils.h"
#include <unordered_map>
#include "fpcam.h"
#include <mutex>
#include <utility>
#include <shared_mutex>

constexpr float DIFF_THRESHOLD = 0.01f;

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
	std::unordered_map<uint32_t, ThirdPersonAnimationController> entityAnimationControllers;
	std::unordered_map<uint32_t, VulkanBuffer> entityJointBuffers;
	gltfObject* characterObject;

	gltfObject* firstPersonObject;
	VulkanBuffer firstPersonJointBuffer;
	FirstPersonAnimationController firstPersonAnimationController;

	gltfObject* pistolObject;
	VulkanBuffer pistolJointBuffer;
	PistolAnimationController pistolAnimationController;

	SWChainImageFormat imageFormat;
	VkDescriptorSetLayout* uniformSetLayout;
	PacketBuffer packetBuffer;
	RewindBuffer rB;
	int tickOffset;

	glm::vec3 shotAckPosition;
	
	void setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID);
	void updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID, float deltaTime);
	void drawEntities(VkCommandBuffer& cmd, VulkanPipelineBuilder& pipelineUtil, uint32_t numDrawCommands, VulkanBuffer& dynamicIndirectBuffer, GPUDrawPushConstants& pc);
	void shutdown();
	void clearPendingPackets(Entity* self);
};