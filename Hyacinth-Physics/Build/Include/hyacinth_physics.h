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
#include "common/PxTolerancesScale.h"
#include "cooking/PxTriangleMeshDesc.h"
#include "cooking/PxCooking.h"
#include "characterkinematic/PxController.h"
#include "characterkinematic/PxControllerManager.h"
#include "characterkinematic/PxCapsuleController.h"

#include <unordered_map>
#include "light_loader.h"
#include "hyacinth_network.h"

using namespace physx;

class PhysicsManager {
private:
	bool recordMemoryAllocations = true;

	PxDefaultErrorCallback defaultErrorCallback;
	PxDefaultAllocator defaultAllocatorCallback;
	physx::PxPvd* pVirtDebug = NULL;

	physx::PxFoundation* pFoundation = NULL;
	physx::PxPhysics* pPhysics = NULL;

	physx::PxDefaultCpuDispatcher* pDispatcher;
	physx::PxTolerancesScale pTolerancesScale;
	physx::PxMaterial* pMaterial = NULL;
	physx::PxScene* pScene = NULL;

	physx::PxControllerManager* pCManager = NULL;
	physx::PxCapsuleControllerDesc controllerDesc;

public:
	std::unordered_map<uint32_t, physx::PxController*> clientControllers;
	std::vector<physx::PxShape*> createPhysicsFromMesh(LightObject* object);
	std::mutex controllerArrayMutex;

	void initPhysics();
	void addCharacterController(uint32_t cId);
	void removeCharacterController(uint32_t cId);
	void addStaticPhysicsObject(LightObject* object);
	void updatePhysicsServer(EntityManager* entityManager);
	void updateCamera(uint32_t eId, float camSpeed, SimulateStruct& p, Transform& t, bool serverSide, float deltaTime);
	void updatePlayerMovement(uint32_t eId, Transform& t, SimulateStruct& s);
};