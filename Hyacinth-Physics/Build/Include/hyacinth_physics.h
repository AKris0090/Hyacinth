#pragma once

#include <iostream>
#include "PxPhysics.h"
#include "PxScene.h"
#include "PxSceneDesc.h"
#include "PxRigidStatic.h"
#include "foundation/PxFoundation.h"
#include "foundation/PxPhysicsVersion.h"
#include "extensions/PxDefaultErrorCallback.h"
#include "extensions/PxDefaultAllocator.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"
#include "pvd/PxPvdSceneClient.h"
#include "PxPhysicsAPI.h"
#include "common/PxTolerancesScale.h"
#include "cooking/PxTriangleMeshDesc.h"
#include "cooking/PxCooking.h"
#include "characterkinematic/PxController.h"
#include "characterkinematic/PxControllerManager.h"
#include "characterkinematic/PxCapsuleController.h"
#include "geometry/PxGeometryQuery.h"
#include "geometry/PxGeometryHit.h"
#include "PxRigidDynamic.h"

#include <unordered_map>
#include "light_loader.h"
#include "hyacinth_network.h"

using namespace physx;

constexpr float JUMP_VELOCITY = 10.5f;

struct controllerUserData {
	uint32_t id;
};

struct hitReg {
	bool hit;
	uint32_t entityHitId;
	glm::vec3 footPosHit;
};

static physx::PxVec3 physxVec(glm::vec3 v) {
	return physx::PxVec3(v.x, v.y, v.z);
}

static physx::PxExtendedVec3 physxEVec(glm::vec3 v) {
	return physx::PxExtendedVec3(v.x, v.y, v.z);
}

static glm::vec3 glmPhysxEVec(physx::PxExtendedVec3 v) {
	return glm::vec3(v.x, v.y, v.z);
}

static glm::vec3 glmPhysxVec(physx::PxVec3 v) {
	return glm::vec3(v.x, v.y, v.z);
}

class PhysicsManager {
private:
	bool recordMemoryAllocations = true;

	std::mutex charLock;

	PxDefaultErrorCallback defaultErrorCallback;
	PxDefaultAllocator defaultAllocatorCallback;
	physx::PxPvd* pVirtDebug = NULL;

	physx::PxFoundation* pFoundation = NULL;
	physx::PxPhysics* pPhysics = NULL;

	physx::PxDefaultCpuDispatcher* pDispatcher;
	physx::PxTolerancesScale pTolerancesScale;
	physx::PxMaterial* pMaterial = NULL;

	physx::PxControllerManager* pCManager = NULL;
	physx::PxCapsuleControllerDesc controllerDesc;

	void loadShape(std::vector<physx::PxShape*>& shapes, LightNode* node);

public:
	physx::PxScene* pScene = NULL;
	physx::PxCapsuleGeometry capGeom;
	std::vector<physx::PxTriangleMeshGeometry> worldGeom;
	std::unordered_map<uint32_t, physx::PxController*> clientControllers;
	std::unordered_map<uint32_t, PhysicsEnt> clientPhysicsObjects;
	std::vector<physx::PxShape*> createPhysicsFromMesh(LightObject* object);
	ThreadSafeQueue<Event> physicsEventQueue;

	void initPhysics(bool debug);
	void addCharacterController(uint32_t cId);
	void removeCharacterController(uint32_t cId);
	void addStaticPhysicsObject(LightObject* object);
	void updatePhysicsServer(EntityManager* entityManager);
	void updateCamera(uint32_t eId, float camSpeed, SimulateStruct& p, Transform& t, bool serverSide, float deltaTime);
	void updatePlayerMovement(uint32_t eId, float moveSpeed, Transform& t, SimulateStruct& s);
	void addNetworkEntityCapsuleCollider(uint32_t cId);
	void setNetworkEntityCapColliderPosition(ServerSnapshot* s, uint32_t selfId);
	glm::vec3 traceBullet(Transform& camTransform);

	hitReg playerShooting(uint32_t eId, Transform& t, rewindSnapshot* snapshotToTrace);
};