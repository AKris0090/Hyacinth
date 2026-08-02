#include "pch.h"

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

void PhysicsManager::initPhysics(bool debug) {
	std::cout << "[PHYSICS] Initiating Physics engine" << std::endl;

	pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
	if (!pFoundation) {
		PHYSX_ERROR("PxCreateFoundation failed!");
	}

	if (debug) {
		pVirtDebug = PxCreatePvd(*pFoundation);
		physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
		bool connected = pVirtDebug->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
		std::cout << "[PHYSICS] Debug Connection Status: " << connected << std::endl;
	}

	pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pFoundation, physx::PxTolerancesScale(), recordMemoryAllocations, pVirtDebug);
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
	pMaterial = pPhysics->createMaterial(0.85f, 0.75f, 0.f);
	pFrictionMaterial = pPhysics->createMaterial(0.9f, 0.9f, 0.5f);

	pCManager = PxCreateControllerManager(*pScene);
	controllerDesc.radius = 0.5f;
	controllerDesc.height = 1.f;
	controllerDesc.position = physx::PxExtendedVec3(0.0, 0.2, 0.0);
	controllerDesc.material = pMaterial;
	controllerDesc.stepOffset = 0.f;
	controllerDesc.contactOffset = 0.1f;
	controllerDesc.scaleCoeff = 1.f;

	capGeom = physx::PxCapsuleGeometry(0.5f, 1.f);
	sphereGeom = physx::PxSphereGeometry(0.15f);

	jointGeom = physx::PxSphereGeometry(0.05f);

	std::cout << "[PHYSICS] Physics created!" << std::endl << std::endl;
}

// Client function for adding capsules to properly predict player-player collision
void PhysicsManager::addNetworkEntityCapsuleCollider(uint32_t cId) {
	physx::PxController* entityController = pCManager->createController(controllerDesc);
	clientControllers[cId] = entityController;
}

void PhysicsManager::setNetworkEntityCapColliderPosition(ServerSnapshot* s, uint32_t selfId) {
	charLock.lock();
	for (const auto& e : s->entities) {
		if (e.id == selfId) continue;
		if (clientControllers.find(e.id) == clientControllers.end()) {
			addNetworkEntityCapsuleCollider(e.id);
		}
		clientControllers[e.id]->setFootPosition(physxEVec(e.transform.position));
	}
	charLock.unlock();
}

physx::PxTransform matToPxTransform(const glm::mat4& m)
{
	glm::vec3 pos = glm::vec3(m[3]);
	glm::quat q = glm::quat_cast(m); // assumes orthonormal rotation part

	physx::PxVec3 pxPos(pos.x, pos.y, pos.z);
	physx::PxQuat pxQuat(q.x, q.y, q.z, q.w); // note: PxQuat is (x,y,z,w), matches glm order

	return physx::PxTransform(pxPos, pxQuat);
}

void PhysicsManager::addRagdoll(uint32_t id, LightMesh* meshRef) {
	// add all major joints
	std::vector<std::string> jointNames = {
		"spine.006",
		"upper_arm.L",
		"upper_arm.R",
		"forearm.L",
		"forearm.R",
		"hand.L",
		"hand.R",
		"thigh.L",
		"thigh.R",
		"shin.L",
		"shin.R",
		"toe.L",
		"toe.R"
	};

	Ragdoll rag;
	for (const auto& jN : jointNames) {
		RagdollJoint joint;

		// get joint
		LightNode* n = meshRef->getNodeByName(jN);
		joint.nodeIndex = n->nodeIndex;

		physx::PxShape* sphereShape = pPhysics->createShape(jointGeom, *pFrictionMaterial);
		physx::PxRigidDynamic* dyn = pPhysics->createRigidDynamic(matToPxTransform(n->getMatrix()));
		dyn->attachShape(*sphereShape);
		sphereShape->release();
		pScene->addActor(*dyn);
		dyn->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);

		joint.rdJoint = dyn;
		rag.joints.push_back(joint);
	}
	ragdolls[id] = rag;
}

void PhysicsManager::addCharacterController(uint32_t cId, LightMesh* meshRef) {
	if (auto search = clientControllers.find(cId); search != clientControllers.end()) {
		std::cout << "[PHYSICS] That client already has a character controller?" << std::endl;
	}
	physx::PxController* playerController = pCManager->createController(controllerDesc);
	playerController->getActor()->userData = new controllerUserData{ cId };
	clientControllers[cId] = playerController;
	clientPhysicsObjects[cId] = PhysicsEnt{};
	
	// addRagdoll(cId, meshRef);
}

void PhysicsManager::removeCharacterController(uint32_t cId) {
	clientControllers[cId]->release();
	clientPhysicsObjects.erase(cId);
	clientControllers.erase(cId);
}

void PhysicsManager::addDynamicNetworkSphere(uint32_t id, glm::vec3 spawnPos, glm::vec3 initialVel) {
	physx::PxShape* sphereShape = pPhysics->createShape(sphereGeom, *pFrictionMaterial);
	physx::PxRigidDynamic* dyn = pPhysics->createRigidDynamic(physx::PxTransform(physxVec(spawnPos)));
	dyn->setLinearVelocity(physxVec(initialVel));
	dyn->attachShape(*sphereShape);
	sphereShape->release();
	pScene->addActor(*dyn);

	worldObjects[id] = dyn;
}

void PhysicsManager::loadShape(std::vector<physx::PxShape*>& shapes, LightMesh* mesh, LightNode* node) {
	glm::mat4 trueModel = node->getMatrix();
	for (auto& prim : node->primitives) {
		std::vector<physx::PxVec3> pxVertices;
		std::vector<uint32_t> pxIndices;

		for (uint32_t i = 0; i < prim.vertexCount; i++) {
			glm::vec3 vert = mesh->vertices[i + prim.firstVertex].pos;
			glm::vec4 p = trueModel * glm::vec4(vert.x, vert.y, vert.z, 1.0f);
			pxVertices.push_back(physx::PxVec3(p.x, p.y, p.z));   
		}

		for (uint32_t i = 0; i < prim.indexCount; i++) {
			pxIndices.push_back(mesh->indices[i + prim.firstIndex]);
		}

		physx::PxTriangleMeshDesc meshDescription;
		meshDescription.points.count = static_cast<uint32_t>(pxVertices.size());
		meshDescription.points.data = pxVertices.data();
		meshDescription.points.stride = sizeof(physx::PxVec3);

		meshDescription.triangles.count = static_cast<uint32_t>(pxIndices.size() / 3);
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
		worldGeom.push_back(geo);

		physx::PxShapeFlags shapeFlags(physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE);
		physx::PxShape* shape = pPhysics->createShape(geo, *pMaterial, shapeFlags);
		shapes.push_back(shape);
	}

	for (auto& child : node->children) {
		loadShape(shapes, mesh, child);
	}
}

std::vector<physx::PxShape*> PhysicsManager::createPhysicsFromMesh(LightMesh* object) {
	std::vector<physx::PxShape*> shapes;

	for (auto& node : object->parentNodes) {
		loadShape(shapes, object, node);
	}

	return shapes;
}

void PhysicsManager::addStaticPhysicsObject(LightMesh* object) {
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
void PhysicsManager::updateCamera(uint32_t eId, float camSpeed, SimulateStruct& p, Transform& t, bool serverSide, float deltaTime, CamRecoil* r) {
	if (serverSide) {
		t.pitch = p.pitch;
		t.yaw = p.yaw;
	} else {
		// simulate struct pitch and yaw are DELTA VALUES
		float mouseX = p.pitch * camSpeed * deltaTime;
		float mouseY = p.yaw * camSpeed * deltaTime;

		t.yaw += mouseX;
		t.pitch -= mouseY;

		if (t.yaw > 360.f)  t.yaw -= 360.f;
		if (t.yaw < -360.f) t.yaw += 360.f;

		float pitchAddition = r->updateRecoil(deltaTime);
		t.pitchAdditional = pitchAddition;

		t.pitch = glm::clamp(t.pitch, -89.9f, 89.9f);
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
		phys.yVel -= 9.81f * 3.65f * SERVER_TIMESTEP;
	}

	float yDisplacement = phys.yVel * SERVER_TIMESTEP;

	charLock.lock();
	const physx::PxControllerCollisionFlags flags = clientControllers[eId]->move(physx::PxVec3(localDisplacement.x, yDisplacement, localDisplacement.z), 0.001f, SERVER_TIMESTEP, nullptr);
	charLock.unlock();
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

	// update character animations
	// for (const auto& [id, object] : entityManager->characterGameObjects) {
	// 	object->controller.updateAnimParams(&entityManager->clients[id]->entity);
	// 	object->updateAnimation(SERVER_TIMESTEP, false);
	// 
	// 	for (auto& j : ragdolls[id].joints) {
	// 		glm::mat4 worldMat = entityManager->clients[id]->entity.transform.getMatrix() * object->nodeTransforms[j.nodeIndex].getMatrix();
	// 		physx::PxTransform transform = matToPxTransform(worldMat);
	// 		j.rdJoint->setKinematicTarget(matToPxTransform(worldMat));
	// 	}
	// }

	// flush physics events
	Event e;
	while (physicsEventQueue.pop(e)) {
		if (e.eventType == SERVER_EVENT::CLIENT_DISCONNECT) {
			removeCharacterController(e.clientID);
		}
	}
}

void PhysicsManager::updateAllWorldObjects(std::unordered_map<uint32_t, Ordnance*>& ord, std::unordered_map<uint32_t, ServersideClient*>& clients) {
	std::vector<uint32_t> idsToErase;
	for (const auto& [id, ordn] : ord) {
		ordn->entity.transform.position = glmPhysxVec(worldObjects[id]->getGlobalPose().p);

		if (ordn->updateFlashState(SERVER_TIMESTEP) == POP) {
			// loop clients to flash them
			for (auto& [clientID, client] : clients) {
				hitReg h = hitReg{ false, INT_MAX };

				// return hitreg struct that reports if the shooter hit anything, or if hit nothing
				physx::PxVec3 origin = physxVec(ordn->entity.transform.position);
				physx::PxVec3 dir = physxVec(glm::normalize((client->entity.transform.position + glm::vec3(0.f, 1.85f, 0.f) - ordn->entity.transform.position)));
				physx::PxReal maxDist = 100.f;
				physx::PxRaycastBuffer rayHit;

				float cDistance = FLT_MAX;

				physx::PxTransform pose(physxVec(client->entity.transform.position + glm::vec3(0, 1.f, 0)), physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
				physx::PxRaycastHit hit;

				bool didHit = physx::PxGeometryQuery::raycast(origin, dir, capGeom, pose, maxDist, physx::PxHitFlag::eDEFAULT, 1, &hit);

				if (didHit) {
					h.hit = true;
					cDistance = hit.distance;
				}

				for (const auto& g : worldGeom) {
					physx::PxTransform pose(physxVec(glm::vec3(0, 0, 0)));
					physx::PxRaycastHit hit;

					bool didHit = physx::PxGeometryQuery::raycast(origin, dir, g, pose, maxDist, physx::PxHitFlag::eDEFAULT, 1, &hit);

					if (didHit && hit.distance < cDistance) {
						h.hit = false;
						cDistance = hit.distance;
					}
				}

				if (h.hit) {
					float dot = glm::dot(glm::normalize(client->entity.transform.forward), glmPhysxVec(dir));

					if (dot < 0.f) {
						glm::mat4 proj = glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.f, 1000.f);
						glm::vec3 pos = client->entity.transform.position + glm::vec3(0.f, 1.85f, 0.f);
						glm::mat4 view = glm::lookAt(pos, pos + client->entity.transform.forward, client->entity.transform.up);

						glm::vec4 ndcpos = proj * view * glm::vec4(ordn->entity.transform.position, 1.f);
						ndcpos /= ndcpos.w;
						ndcpos.y *= -1.f;

						bool xinRange = (ndcpos.x < 1.f) && (ndcpos.x > -1.f);
						bool yinRange = (ndcpos.y < 1.f) && (ndcpos.y > -1.f);
						if (xinRange && yinRange) {
							ndcpos.x = glm::clamp(ndcpos.x, -1.f, 1.f);
							ndcpos.y = glm::clamp(ndcpos.y, -1.f, 1.f);

							client->entity.startFlash(ndcpos.x, ndcpos.y);
						}
					}
					else {

					}
				}
			}

			idsToErase.push_back(id);
		}
	}

	for (auto& id : idsToErase) {
		ord.erase(id);
	}
}

hitReg PhysicsManager::playerShooting(uint32_t shooterId, Transform& currentEntityTransform, rewindSnapshot* snapshotToTrace) {
	hitReg h = hitReg{ false, INT_MAX };

	// return hitreg struct that reports if the shooter hit anything, or if hit nothing
	physx::PxVec3 origin = physxVec(currentEntityTransform.position + glm::vec3(0.f, 1.85f, 0.f));
	currentEntityTransform.setRotationPitchYaw();
	physx::PxVec3 dir = physxVec(glm::normalize(currentEntityTransform.forward));
	physx::PxReal maxDist = 100.f;
	physx::PxRaycastBuffer rayHit;

	float cDistance = FLT_MAX;

	for (const auto& e : snapshotToTrace->entityPositions) {
		if (e.id == shooterId) continue; // if the current entity is the shooter, skip

		physx::PxTransform pose(physxVec(e.pos + glm::vec3(0, 1.f, 0)), physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
		physx::PxRaycastHit hit;

		bool didHit = physx::PxGeometryQuery::raycast(origin, dir, capGeom, pose, maxDist, physx::PxHitFlag::eDEFAULT, 1, &hit);

		if (didHit && hit.distance < cDistance) {
			h.hit = true;
			h.entityHitId = e.id;
			h.footPosHit = e.pos;
			h.hitPos = glmPhysxVec(hit.position);
			cDistance = hit.distance;
			break;
		}
	}

	for (const auto& g : worldGeom) {
		physx::PxTransform pose(physxVec(glm::vec3(0, 0, 0)));
		physx::PxRaycastHit hit;

		bool didHit = physx::PxGeometryQuery::raycast(origin, dir, g, pose, maxDist, physx::PxHitFlag::eDEFAULT, 1, &hit);

		if (didHit && hit.distance < cDistance) {
			cDistance = hit.distance;
			h.entityHitId = INT_MAX;
			h.footPosHit = glm::vec3(0, -100.f, 0);
			h.hitPos = glmPhysxVec(hit.position);
		}
	}

	if (cDistance == FLT_MAX) {
		h.hitPos = glmPhysxVec((origin + (dir * 35.f)));
	}

	return h;
}

void DrawRaycastPVD(physx::PxPvdSceneClient* pvdClient, const physx::PxVec3& origin, const physx::PxVec3& direction, float distance, bool hit = false, const physx::PxVec3& hitPos = physx::PxVec3(0.f)) {
	if (!pvdClient)
		return;

	physx::PxVec3 normalizedDir = direction.getNormalized();
	physx::PxVec3 endpoint = origin + normalizedDir * distance;

	if (hit) {
		physx::PxDebugLine rayToHit[] = {
			physx::PxDebugLine(origin, hitPos, physx::PxDebugColor::eARGB_YELLOW)
		};
		pvdClient->drawLines(rayToHit, 1);

		physx::PxDebugPoint hitMarker[] = {
			physx::PxDebugPoint(hitPos, physx::PxDebugColor::eARGB_RED)
		};
		pvdClient->drawPoints(hitMarker, 1);
	}
	else {
		physx::PxDebugLine fullRay[] = {
			physx::PxDebugLine(origin, endpoint, physx::PxDebugColor::eARGB_GREEN)
		};
		pvdClient->drawLines(fullRay, 1);
	}

	physx::PxDebugPoint originMarker[] = {
		physx::PxDebugPoint(origin, physx::PxDebugColor::eARGB_WHITE)
	};
	pvdClient->drawPoints(originMarker, 1);
}

glm::vec3 PhysicsManager::traceBullet(Transform& entTransform) {
	physx::PxVec3 origin = physxVec(entTransform.position + glm::vec3(0.f, 1.85f, 0.f));
	physx::PxVec3 dir = physxVec(glm::normalize(entTransform.forward));
	physx::PxReal maxDist = 100.f;
	physx::PxRaycastBuffer rayHit;

	glm::vec3 hitPos = glm::vec3(0.f);
	float cDistance = FLT_MAX;

	for(const auto& g : worldGeom) {
		physx::PxTransform pose(physxVec(glm::vec3(0, 0, 0)));
		physx::PxRaycastHit hit;

		bool didHit = physx::PxGeometryQuery::raycast(origin, dir, g, pose, maxDist, physx::PxHitFlag::eDEFAULT, 1, &hit);

		if (didHit && hit.distance < cDistance) {
			cDistance = hit.distance;
			hitPos = glmPhysxVec(hit.position);
		}
	}

	if (cDistance == FLT_MAX) {
		hitPos = glmPhysxVec((origin + (dir * 35.f)));
	}

	DrawRaycastPVD(pScene->getScenePvdClient(), origin, dir, 100.f, cDistance != FLT_MAX, cDistance != FLT_MAX ? physxVec(hitPos) : physx::PxVec3(0.f));

	return hitPos;
}