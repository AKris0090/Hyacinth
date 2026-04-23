#include "pch.h"
#include "framework.h"

#define GLM_ENABLE_EXPERIMENTAL

#include "hyacinth_physics.h"

#define PVD_HOST "127.0.0.1"

#pragma comment(lib, "PhysX_64.lib")
#pragma comment(lib, "PhysXFoundation_64.lib")
#pragma comment(lib, "PhysXCooking_64.lib")
#pragma comment(lib, "PhysXCommon_64.lib")
#pragma comment(lib, "PhysXExtensions_static_64.lib")
#pragma comment(lib, "PhysXPvdSDK_static_64.lib") 
#pragma comment(lib, "PhysXCharacterKinematic_static_64.lib")

#define PHYSX_ERROR(err) {				\
	std::cout << err << std::endl;		\
	exit(1);							\
}										\

void PhysicsManager::initPhysics() {
	std::cout << "[PHYSICS] Initiating Physics engine" << std::endl;

	pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
	if (!pFoundation) {
		PHYSX_ERROR("PxCreateFoundation failed!");
	}

#ifndef NDEBUG
	pVirtDebug = PxCreatePvd(*pFoundation);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
	pVirtDebug->connect(*transport, PxPvdInstrumentationFlag::eALL);
#endif

	pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pFoundation, PxTolerancesScale(), recordMemoryAllocations, pVirtDebug);
	if (!pPhysics) {
		PHYSX_ERROR("PxCreatePhysics failed!");
	}

	physx::PxSceneDesc sceneDescription(pPhysics->getTolerancesScale());
	sceneDescription.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	pDispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	sceneDescription.cpuDispatcher = pDispatcher;
	sceneDescription.filterShader = physx::PxDefaultSimulationFilterShader;
	pScene = pPhysics->createScene(sceneDescription);

	physx::PxPvdSceneClient* pvdSceneClient = pScene->getScenePvdClient();
	if (pvdSceneClient) {
		pvdSceneClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdSceneClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdSceneClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}
	pMaterial = pPhysics->createMaterial(0.f, 0.f, 0.f);

	pCManager = PxCreateControllerManager(*pScene);
	controllerDesc.radius = 0.5f;
	controllerDesc.height = 1.f;
	controllerDesc.position = physx::PxExtendedVec3(0.0, 0.2, 0.0);
	controllerDesc.material = pMaterial;
	controllerDesc.stepOffset = 0.f;
	controllerDesc.contactOffset = 0.1;
	controllerDesc.scaleCoeff = 1.f;

	std::cout << "[PHYSICS] Physics created!" << std::endl << std::endl;
}

// TODO: add capsule colliders on client for physics preds
// void PhysicsManager::addNetworkEntityCapsuleCollider(uint32_t cId) {

// }

void PhysicsManager::addCharacterController(uint32_t cId) {
	if (auto search = clientControllers.find(cId); search != clientControllers.end()) {
		std::cout << "[PHYSICS] That client already has a character controller?" << std::endl;
	}
	physx::PxController* playerController = pCManager->createController(controllerDesc);
	clientControllers[cId] = playerController;
	clientPhysicsObjects[cId] = PhysicsEnt{};
}

void PhysicsManager::removeCharacterController(uint32_t cId) {
	clientControllers[cId]->release();
	clientPhysicsObjects.erase(cId);
	clientControllers.erase(cId);
}

std::vector<physx::PxShape*> PhysicsManager::createPhysicsFromMesh(LightObject* object) {
	std::vector<physx::PxShape*> shapes;

	for (auto& node : object->nodes) {
		glm::mat4 trueModel = node.get()->worldTransform;
		for (auto& prim : node.get()->primitives) {
			std::vector<physx::PxVec3> pxVertices;
			std::vector<uint32_t> pxIndices;

			for (int i = 0; i < prim.get()->vertices.size(); i++) {
				glm::vec3 vert = prim.get()->vertices[i];
				glm::vec4 p = glm::vec4(vert.x, vert.y, vert.z, 1.0f) * trueModel;
				pxVertices.push_back(physx::PxVec3(p.x, p.y, p.z));
			}

			for (int i = 0; i < prim.get()->indices.size(); i++) {
				pxIndices.push_back(prim.get()->indices[i]);
			}

			physx::PxTriangleMeshDesc meshDescription;
			meshDescription.points.count = pxVertices.size();
			meshDescription.points.data = pxVertices.data();
			meshDescription.points.stride = sizeof(physx::PxVec3);

			meshDescription.triangles.count = pxIndices.size() / 3;
			meshDescription.triangles.data = pxIndices.data();
			meshDescription.triangles.stride = 3 * sizeof(physx::PxU32);

			assert(meshDescription.isValid());

			physx::PxTolerancesScale toleranceScale;
			physx::PxCookingParams params(toleranceScale);

			params.midphaseDesc = physx::PxMeshMidPhase::eBVH33;

			bool skipMeshCleanup = false;
			bool skipEdgeData = false;
			bool cookingPerformance = false;
			bool meshSizePerfTradeoff = true;

			params.suppressTriangleMeshRemapTable = true;
			params.meshPreprocessParams |= static_cast<physx::PxMeshPreprocessingFlags>(physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
			params.meshPreprocessParams &= ~static_cast<physx::PxMeshPreprocessingFlags>(physx::PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE);
			params.midphaseDesc.mBVH33Desc.meshCookingHint = physx::PxMeshCookingHint::eSIM_PERFORMANCE;
			params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff = 0.0f;

			physx::PxTriangleMesh* triMesh = PxCreateTriangleMesh(params, meshDescription, pPhysics->getPhysicsInsertionCallback());

			physx::PxMeshGeometryFlags flags(~physx::PxMeshGeometryFlag::eDOUBLE_SIDED);
			physx::PxTriangleMeshGeometry geo(triMesh, physx::PxMeshScale(physx::PxVec3(1, 1, 1)), flags);

			physx::PxShapeFlags shapeFlags(physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE);
			physx::PxShape* shape = pPhysics->createShape(geo, *pMaterial, shapeFlags);
			shapes.push_back(shape);
		}
	}

	return shapes;
}

void PhysicsManager::addStaticPhysicsObject(LightObject* object) {
	std::vector<physx::PxShape*> shapes = createPhysicsFromMesh(object);
	physx::PxRigidStatic* body = pPhysics->createRigidStatic(physx::PxTransform(physx::PxVec3(0, 0, 0)));
	for (auto& shape : shapes) {
		body->attachShape(*shape);
		shape->release();
	}
	pScene->addActor(*body);
}

// if serverSide is true, then simulateStruct has absolute pitch and yaw values
// if false, then simulateStruct has delta xRel and yRel mouse raw input values
void PhysicsManager::updateCamera(uint32_t eId, float camSpeed, SimulateStruct& p, Transform& t, bool serverSide, float deltaTime) {
	if (serverSide) {
		t.pitch = p.pitch;
		t.yaw = p.yaw;
	} else {
		if (glm::abs(p.pitch) > 0.f || glm::abs(p.yaw) > 0.f) { // simulate struct pitch and yaw are DELTA VALUES
			float mouseX = p.pitch * camSpeed * deltaTime;
			float mouseY = p.yaw * camSpeed * deltaTime;

			t.yaw += mouseX;
			t.pitch -= mouseY;

			if (t.yaw > 360.f)  t.yaw -= 360.f;
			if (t.yaw < -360.f) t.yaw += 360.f;
			t.pitch = glm::clamp(t.pitch, -89.9f, 89.9f);
		}
	}

	t.setRotationPitchYaw();
}

void PhysicsManager::updatePlayerMovement(uint32_t eId, float moveSpeed, Transform& t, SimulateStruct& s) {
	if (clientControllers.find(eId) == clientControllers.end()) {
		return;
	}

	glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };
	glm::vec3 flatForward = glm::normalize(glm::vec3(t.forward.x, 0.0f, t.forward.z));
	glm::vec3 flatRight = glm::normalize(glm::vec3(t.right.x, 0.0f, t.right.z));

	if (s.movementFB > 0)       localDisplacement += flatForward;
	if (s.movementFB < 0)       localDisplacement -= flatForward;
	if (s.movementLR > 0)       localDisplacement += flatRight;
	if (s.movementLR < 0)       localDisplacement -= flatRight;

	glm::vec3 pos{ 0.f, 0.f, 0.f };
	if (glm::length(localDisplacement) > 0) {
		localDisplacement = glm::normalize(localDisplacement);
		localDisplacement *= moveSpeed;
	}

	PhysicsEnt& phys = clientPhysicsObjects[eId];

	if (phys.isGrounded) {
		if (s.jump) {
			phys.yVel = JUMP_VELOCITY;
		}
		else {
			phys.yVel = 0.f;
		}
	}
	else {
		phys.yVel -= 9.81f * SERVER_TIMESTEP;
	}

	float yDisplacement = phys.yVel * SERVER_TIMESTEP;

	const physx::PxControllerCollisionFlags flags = clientControllers[eId]->move(physx::PxVec3(localDisplacement.x, yDisplacement, localDisplacement.z), 0.001f, SERVER_TIMESTEP, nullptr);
	phys.isGrounded = flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_DOWN);
	physx::PxExtendedVec3 p = clientControllers[eId]->getFootPosition();
	t.position = glm::vec3(p.x, p.y, p.z);
}

void PhysicsManager::updatePhysicsServer(EntityManager* entityManager) {
	for (const auto& [id, sSClient] : entityManager->clients) {
		if (clientControllers[id] == NULL) continue;
		updateCamera(id, sSClient->entity.camSpeed, sSClient->bufferedPacket, sSClient->entity.transform, true, -FLT_MAX);
		updatePlayerMovement(id, sSClient->entity.moveSpeed, sSClient->entity.transform, sSClient->bufferedPacket);
	}
	pScene->simulate(SERVER_TIMESTEP);
	pScene->fetchResults(true);

	// flush physics events
	Event e;
	while (physicsEventQueue.pop(e)) {
		if (e.eventType == SERVER_EVENT::CLIENT_DISCONNECT) {
			removeCharacterController(e.clientID);
		}
	}
}