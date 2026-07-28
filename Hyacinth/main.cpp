#include "sdlwindow.h"
#include "hyacinthengine.h"
#include "hyacinth_client.h"
#include "input.h"
#include "time.h"
#include "hyacinth_physics.h"
#include <thread>
#include <chrono>

#include "fp_arms.h"
#include "pistol.h"
#include "flashbang.h"

#define CONNECT_SERVER true

#pragma comment(lib, "Hyacinth-Physics.lib")

std::atomic<uint32_t> tickNum{ 0 };
BotBehavior b;
bool dontEnd = true;

HyacinthEngine hyacinthEngine;
PhysicsManager physicsManager;
HyacinthNetworkClient netClient;

void simulationTick() {
	auto epoch = std::chrono::steady_clock::now();
	while (dontEnd) {
		auto nextTick = epoch + (tickNum + 1) * SERVER_TIMESTEP_MS;

		hyacinthEngine.p_netEntManager->inputAccumulatorMutex.lock();
		// update physics
		ClientUpdatePacket p;
		p.id = netClient.netEntManager.self->id;
		p.tick = tickNum;
		p.movementFB = netClient.netEntManager.inputAccumulator.movementFB;
		p.movementLR = netClient.netEntManager.inputAccumulator.movementLR;
		p.jump = netClient.netEntManager.inputAccumulator.jump;
		p.lmb = netClient.netEntManager.inputAccumulator.shooting;
		p.r = netClient.netEntManager.inputAccumulator.reloading;
		p.num1 = netClient.netEntManager.inputAccumulator.num1;
		p.num2 = netClient.netEntManager.inputAccumulator.num2;

		if (b.active) {
			p.movementLR = b.update(SERVER_TIMESTEP);
			p.movementFB = 0;
			p.jump = false;
		}

		// update physics
		hyacinthEngine.p_netEntManager->selfMutex.lock();

		hyacinthEngine.m_camera.prevPitch = hyacinthEngine.m_camera.m_transform.pitch;
		hyacinthEngine.m_camera.prevYaw = hyacinthEngine.m_camera.m_transform.yaw;

		physicsManager.updatePlayerMovement(0, netClient.netEntManager.self->moveSpeed, netClient.netEntManager.self->transform, netClient.netEntManager.inputAccumulator);

		hyacinthEngine.p_netEntManager->inputAccumulatorMutex.unlock();

		p.pitch = netClient.netEntManager.self->transform.pitch;
		p.yaw = netClient.netEntManager.self->transform.yaw;

		bool shotFiredOut = false;
		EQUIPPED_WEAPON weaponOut = PISTOL;

		hyacinthEngine.p_netEntManager->self->updateWeaponState(SERVER_TIMESTEP, p.num1, p.num2, p.lmb, p.r, shotFiredOut, weaponOut);

		if (shotFiredOut && netClient.netEntManager.self->currentWeapon == PISTOL) {
#ifdef DEBUG_NETWORK
			for (const auto& e : hyacinthEngine.p_netEntManager->entities) {
				if (e.first == 1) {
					hyacinthEngine.m_netDebugRenderer.clientEntityPosition = glm::vec4(e.second->transform.position, 1.f);
				}
			}
#endif
			// if shot fired, then draw trace and draw the tracer to connect the two
			glm::vec3 hitPos = physicsManager.traceBullet(netClient.netEntManager.self->transform);
			glm::vec3 origin = netClient.netEntManager.self->transform.position + glm::vec3(0.f, 1.85f, 0.f);
			origin += netClient.netEntManager.self->transform.forward * 1.6f;
			origin += netClient.netEntManager.self->transform.right * 0.9f;
			origin -= netClient.netEntManager.self->transform.up * 0.2f;
			glm::vec3 dir = hitPos - origin;
			glm::vec3 normDir = glm::normalize(dir);
			Transform t;
			t.scale.x = glm::length(dir);
			t.yaw = glm::degrees(glm::atan2(normDir.z, normDir.x));
			t.pitch = glm::degrees(glm::asin(normDir.y));
			t.setRotationPitchYaw();
			t.position = origin;
			hyacinthEngine.m_tracerManager.addTracer(t.getMatrix(), hyacinthEngine.m_assetDrawer.getStaticMeshRef("tracer"));

			netClient.netEntManager.self->recoil.startRecoil();
		}

#ifdef CONNECT_SERVER
		netClient.updateServerTick(p, hyacinthEngine.mouseLocked);
		netClient.netEntManager.rB.addState(netClient.netEntManager.self->transform, p.movementFB, p.movementLR, tickNum);
#endif
		hyacinthEngine.p_netEntManager->selfMutex.unlock();

#ifdef CONNECT_SERVER
		hyacinthEngine.p_netEntManager->rB.pendingPacketsMutex.lock();
		hyacinthEngine.p_netEntManager->clearPendingPackets(hyacinthEngine.p_netEntManager->self, hyacinthEngine.m_assetDrawer.getStaticMeshRef("tracer"));
		hyacinthEngine.p_netEntManager->rB.pendingPacketsMutex.unlock();
#endif
  
		hyacinthEngine.p_netEntManager->inputAccumulatorMutex.lock();
		netClient.netEntManager.inputAccumulator.reset();
		hyacinthEngine.p_netEntManager->inputAccumulatorMutex.unlock();

		hyacinthEngine.p_netEntManager->selfMutex.lock();
		ServerSnapshot sP;
		sP.entities.push_back(*hyacinthEngine.p_netEntManager->self);
		hyacinthEngine.p_netEntManager->selfSimBuffer.newPacket(sP);
		hyacinthEngine.p_netEntManager->selfMutex.unlock();

		// only uncomment if need to view debug in PVD, otherwise interferes with shots
		// physicsManager->pScene->simulate(SERVER_TIMESTEP);
		// physicsManager->pScene->fetchResults(true);

		std::this_thread::sleep_until(nextTick);

		tickNum++;
	}
}

HStaticGameObject* worldObject;

HFPArms* armsObject;
HPistol* pistolObject;
HFlashBang* flashObject;

void addGameObjects(HyacinthEngine& engine, HyacinthNetworkClient& netClient) {
	worldObject = new HStaticGameObject(engine.m_assetDrawer.getStaticMeshRef("world"));
	engine.addStaticGameObject(worldObject);  

	armsObject = new HFPArms(engine.m_assetDrawer.getAnimatedMeshRef("fp_arms"));
	engine.addAnimatedGameObject(armsObject);

	pistolObject = new HPistol(engine.m_assetDrawer.getAnimatedMeshRef("pistol"));
	engine.addAnimatedGameObject(pistolObject);
	pistolObject->setParentObject(armsObject, pistolObject->controller.baseNode, armsObject->controller.gunBone);
	
	flashObject = new HFlashBang(engine.m_assetDrawer.getAnimatedMeshRef("flashbang"));
	engine.addAnimatedGameObject(flashObject);
	flashObject->setParentObject(armsObject, flashObject->controller.baseNode, armsObject->controller.gunBone);
}

void updateGameObjects(HyacinthEngine& engine, HyacinthNetworkClient& netClient) {
	armsObject->controller.updateAnimParams(netClient.netEntManager.self->currentState, engine.m_camera.m_transform.pitch - engine.m_camera.prevPitch, engine.m_camera.m_transform.yaw - engine.m_camera.prevYaw);
	armsObject->transform.position = engine.m_camera.m_transform.position;
	armsObject->transform.rotation = engine.m_camera.m_transform.rotation;

	flashObject->transform.position = engine.m_camera.m_transform.position;
	flashObject->transform.rotation = engine.m_camera.m_transform.rotation;
	pistolObject->transform.position = engine.m_camera.m_transform.position;
	pistolObject->transform.rotation = engine.m_camera.m_transform.rotation;
	
	if (netClient.netEntManager.self->currentWeapon == PISTOL) {
		pistolObject->active = true;
		pistolObject->controller.updateAnimParams(armsObject->controller.shootTrigger, armsObject->controller.reloadTrigger);
		flashObject->active = false;
	}
	else if (netClient.netEntManager.self->currentWeapon == GRENADE && netClient.netEntManager.self->currentState != GRENADE_THROW) {
		flashObject->active = true;
		pistolObject->active = false;
	}
	else {
		flashObject->active = false;
		pistolObject->active = false;
	}

	for (const auto& ao : engine.m_animatedObjects) {
		ao.gameObject->updateAnimation(Time::getDeltaTime());
		memcpy(ao.jointMatrixBuffer.pMappedData, ao.gameObject->jointMatrixData, ao.gameObject->mesh->jointMatrixSize);
	}
}

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	physicsManager.initPhysics(false); // initialize PVD?
	auto path = vkdebugutils::getExeDir() / "objects" / "sponza" / "sponza_physics.glb";
	// auto path = vkdebugutils::getExeDir() / "objects" / "test_scene.glb";
	LightLoaderOptions op{};
	physicsManager.addStaticPhysicsObject(LightLoader::loadFromFile(path.string(), op));
	physicsManager.addCharacterController(0);

	hyacinthEngine.p_netEntManager = &netClient.netEntManager;
	netClient.netEntManager.inputAccumulator.id = 0;
	netClient.netEntManager.tracerManager = &hyacinthEngine.m_tracerManager;
	Entity* thisEnt = nullptr;

	addGameObjects(hyacinthEngine, netClient);
	hyacinthEngine.bakeDDGI();

#ifdef CONNECT_SERVER
	std::string ip;
	std::cout << "Enter server IP: ";
	// std::getline(std::cin, ip);
	if (CONNECT_SERVER) {
		int res = netClient.setup("", hyacinthEngine.m_swImageFormat, hyacinthEngine.m_descriptorSetLayout, hyacinthEngine.m_assetDrawer.getAnimatedMeshRef("tp_character"), hyacinthEngine.m_assetDrawer.getAnimatedMeshRef("flashbang"));
		std::cout << (res ? "CONNECTION FAILED" : "CONNECTION SUCCESSFUL") << std::endl;
		if (res > 0) {
			exit(EXIT_FAILURE);
		}
	}
	thisEnt = netClient.netEntManager.self;
	ServerSnapshot sP;
	sP.entities.push_back(*thisEnt);
	netClient.netEntManager.selfSimBuffer.newPacket(sP);
	netClient.netEntManager.selfSimBuffer.newPacket(sP);
	netClient.netEntManager.rB.setPhysicsPosition([](glm::vec3 p) {
		physicsManager.clientControllers[0]->setFootPosition(physx::PxExtendedVec3(p.x, p.y, p.z));
	});
	netClient.netEntManager.rB.setPhysicsStep([](Transform& t, int8_t fb, int8_t lr) {
		SimulateStruct s;
		s.id = 0;
		s.movementFB = fb;
		s.movementLR = lr;
		physicsManager.updatePlayerMovement(0, netClient.netEntManager.self->moveSpeed, t, s);
	});
#endif
#ifndef CONNECT_SERVER
	netClient.netEntManager.self = new Entity();
	thisEnt = netClient.netEntManager.self;

	ServerSnapshot s{};
	s.entities.push_back(*thisEnt);
	netClient.netEntManager.selfSimBuffer.newPacket(s);
	netClient.netEntManager.selfSimBuffer.newPacket(s);
	netClient.netEntManager.setupFromServerPacket(s, hyacinthEngine.m_assetDrawer.getAnimatedMeshRef("tp_character"), hyacinthEngine.m_assetDrawer.getAnimatedMeshRef("flashbang"), 0);
#endif
	Time::setInitialTime();

	std::thread tickThread = std::thread(simulationTick);

	while(sdlwindow.running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);

			InputManager::handleSDLInput(event);
			if (event.type == SDL_EVENT_QUIT) {
				sdlwindow.running = false;
			}
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
				hyacinthEngine.mouseLocked = !hyacinthEngine.mouseLocked;
				SDL_SetWindowRelativeMouseMode(hyacinthEngine.m_window, hyacinthEngine.mouseLocked);
			}
		}

		if (InputManager::botKeyDown()) {
			b.active = true;
		}

		// sample/accumulate input 
		std::array<int8_t, 3> m = InputManager::getMovement();
		std::pair<float, float> mo = InputManager::getMouseMotion();

		ClientUpdatePacket p;
		p.id = netClient.netEntManager.self->id;
		p.movementFB = m[0];
		p.movementLR = m[1];
		p.movementUD = m[2];
		p.jump = InputManager::getSpaceButton();
		p.pitch = mo.first;
		p.yaw = mo.second;
		p.lmb = InputManager::mouseDown();
		p.r = InputManager::reloadKeyDown();
		p.num1 = InputManager::num1KeyDown();
		p.num2 = InputManager::num2KeyDown();

		if (hyacinthEngine.mouseLocked) {
			if (!InputManager::getf1()) {
				netClient.netEntManager.inputAccumulatorMutex.lock();
				netClient.netEntManager.inputAccumulator.addPacket(p);
				netClient.netEntManager.inputAccumulatorMutex.unlock();

				hyacinthEngine.camMutex.lock();
				SimulateStruct sS;
				sS.pitch = p.pitch;
				sS.yaw = p.yaw;
				physicsManager.updateCamera(0, netClient.netEntManager.self->camSpeed, sS, hyacinthEngine.m_camera.m_transform, false, Time::getDeltaTime(), &netClient.netEntManager.self->recoil);

				hyacinthEngine.p_netEntManager->selfMutex.lock();
				netClient.netEntManager.self->transform.forward = hyacinthEngine.m_camera.m_transform.forward;
				netClient.netEntManager.self->transform.right = hyacinthEngine.m_camera.m_transform.right;
				netClient.netEntManager.self->transform.pitch = hyacinthEngine.m_camera.m_transform.pitch;
				netClient.netEntManager.self->transform.yaw = hyacinthEngine.m_camera.m_transform.yaw;
				hyacinthEngine.p_netEntManager->selfMutex.unlock();

				ServerSnapshot selfInterp = netClient.netEntManager.selfSimBuffer.getInterpolatedSimPacket(Time::getDeltaTime());
				hyacinthEngine.m_camera.m_transform.position = selfInterp.entities[0].transform.position;
				hyacinthEngine.m_camera.m_transform.position.y += 1.85f;

				hyacinthEngine.m_camera.update(false); // not flycam
				hyacinthEngine.camMutex.unlock();
			}
			else {
				hyacinthEngine.camMutex.lock();
				hyacinthEngine.m_camera.updateFlyCamera(p, Time::getDeltaTime(), netClient.netEntManager.self->camSpeed, netClient.netEntManager.self->moveSpeed);
				hyacinthEngine.m_camera.update(true); // flycam
				hyacinthEngine.camMutex.unlock();
			}
		}

#ifdef CONNECT_SERVER

		// read packets from the server
		ServerSnapshot interp = netClient.netEntManager.packetBuffer.getInterpolatedSimPacket(Time::getDeltaTime());
		// use packet to determine object transforms
		netClient.netEntManager.updateEntitiesFromPacket(interp, netClient.netEntManager.self->id, Time::getDeltaTime());
		physicsManager.setNetworkEntityCapColliderPosition(&interp, netClient.netEntManager.self->id);

		hyacinthEngine.m_worldHealthManager.update(interp.entities, netClient.netEntManager.self->id, hyacinthEngine.m_camera.m_transform);

#ifdef DEBUG_NETWORK
		hyacinthEngine.m_netDebugRenderer.serverEntityPosition = glm::vec4(netClient.netEntManager.shotAckPosition, 1.f);
#endif

#endif
		updateGameObjects(hyacinthEngine, netClient);

		hyacinthEngine.draw();

		Time::updateTime();
		InputManager::resetMouseMotion();
	}

	dontEnd = false;
	tickThread.join();

	netClient.shutdownNet();
	hyacinthEngine.shutdown();

	return 0;
} 