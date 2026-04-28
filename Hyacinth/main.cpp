#include "sdlwindow.h"
#include "hyacinthengine.h"
#include "hyacinth_client.h"
#include "input.h"
#include "time.h"
#include "hyacinth_physics.h"
#include <thread>
#include <chrono>

#define CONNECT_SERVER true

#pragma comment(lib, "Hyacinth-Physics.lib")

std::atomic<uint32_t> tickNum{ 0 };
BotBehavior b;
bool dontEnd = true;

std::filesystem::path getExeDir()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
}

void simulationTick(HyacinthEngine* engine, HyacinthNetworkClient* netClient, PhysicsManager* physicsManager) {
	auto epoch = std::chrono::steady_clock::now();
	while (dontEnd) {
		auto nextTick = epoch + (tickNum + 1) * SERVER_TIMESTEP_MS;

		engine->p_netEntManager->inputAccumulatorMutex.lock();
		// update physics
		ClientUpdatePacket p;
		p.id = netClient->netEntManager.self->id;
		p.tick = tickNum;
		p.movementFB = netClient->netEntManager.inputAccumulator.movementFB;
		p.movementLR = netClient->netEntManager.inputAccumulator.movementLR;
		p.jump = netClient->netEntManager.inputAccumulator.jump;
		p.lmb = netClient->netEntManager.inputAccumulator.shooting;

		if (b.active) {
			p.movementLR = b.update(SERVER_TIMESTEP);
			p.movementFB = 0;
			p.jump = false;
		}

		// update physics
		engine->p_netEntManager->selfMutex.lock();
		if (engine->mouseLocked) {
			physicsManager->updatePlayerMovement(0, netClient->netEntManager.self->moveSpeed, netClient->netEntManager.self->transform, netClient->netEntManager.inputAccumulator);
		}
		engine->p_netEntManager->inputAccumulatorMutex.unlock();

		p.pitch = netClient->netEntManager.self->transform.pitch;
		p.yaw = netClient->netEntManager.self->transform.yaw;
		netClient->updateServerTick(p, engine->mouseLocked);
		netClient->netEntManager.rB.addState(netClient->netEntManager.self->transform, p.movementFB, p.movementLR, tickNum);
		engine->p_netEntManager->selfMutex.unlock();

		engine->p_netEntManager->rB.pendingPacketsMutex.lock();
		engine->p_netEntManager->clearPendingPackets(engine->p_netEntManager->self);
		engine->p_netEntManager->rB.pendingPacketsMutex.unlock();
  
		engine->p_netEntManager->inputAccumulatorMutex.lock();
		netClient->netEntManager.inputAccumulator.reset();
		engine->p_netEntManager->inputAccumulatorMutex.unlock();

		engine->p_netEntManager->selfMutex.lock();
		ServerSnapshot sP;
		sP.entities.push_back(*engine->p_netEntManager->self);
		engine->p_netEntManager->selfSimBuffer.newPacket(sP);
		engine->p_netEntManager->selfMutex.unlock();

		std::this_thread::sleep_until(nextTick);

		tickNum++;
	}
}

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	PhysicsManager physicsManager;
	physicsManager.initPhysics(false);
	LightLoader loader;
	auto path = getExeDir() / "objects" / "sponza" / "sponza.gltf";
	physicsManager.addStaticPhysicsObject(loader.loadFromFile(path.string(), true));
	physicsManager.addCharacterController(0);

	HyacinthNetworkClient netClient;
	netClient.netEntManager.characterObject = &hyacinthEngine.m_scene.dynamicObjects[0];
	Entity* thisEnt;
	std::string ip; 
	std::cout << "Enter server IP: ";
	// std::getline(std::cin, ip);
	if (CONNECT_SERVER) {
		int res = netClient.setup("", hyacinthEngine.m_swImageFormat, hyacinthEngine.m_descriptorSetLayout);
		std::cout << (res ? "CONNECTION FAILED" : "CONNECTION SUCCESSFUL") << std::endl;
		if (res > 0) {
			exit(EXIT_FAILURE);
		}
	}

	hyacinthEngine.p_netEntManager = &netClient.netEntManager;
	thisEnt = netClient.netEntManager.self;
	netClient.netEntManager.inputAccumulator.id = 0;
	ServerSnapshot sP;
	sP.entities.push_back(*thisEnt);
	netClient.netEntManager.selfSimBuffer.newPacket(sP);
	netClient.netEntManager.selfSimBuffer.newPacket(sP);
	netClient.netEntManager.rB.setPhysicsPosition([&physicsManager](glm::vec3 p) {
		physicsManager.clientControllers[0]->setFootPosition(physx::PxExtendedVec3(p.x, p.y, p.z));
	});
	netClient.netEntManager.rB.setPhysicsStep([&physicsManager, &netClient](Transform& t, float fb, float lr) {
		SimulateStruct s;
		s.id = 0;
		s.movementFB = fb;
		s.movementLR = lr;
		physicsManager.updatePlayerMovement(0, netClient.netEntManager.self->moveSpeed, t, s);
	});

	Time::setInitialTime();

	std::thread tickThread = std::thread(simulationTick, &hyacinthEngine, &netClient, &physicsManager);

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

		hyacinthEngine.draw();

		// sample/accumulate input 
		std::array<int8_t, 3> m = InputManager::getMovement();
		std::pair<float, float> mo = InputManager::getMouseMotion();

		ClientUpdatePacket p;
		p.id = netClient.netEntManager.self->id;
		p.movementFB = m[0];
		p.movementLR = m[1];
		p.jump = InputManager::getSpaceButton();
		p.pitch = mo.first;
		p.yaw = mo.second;
		p.lmb = InputManager::mouseDown();

		netClient.netEntManager.inputAccumulatorMutex.lock();
		netClient.netEntManager.inputAccumulator.addPacket(p);
		netClient.netEntManager.inputAccumulatorMutex.unlock();

		if (hyacinthEngine.mouseLocked) {
			hyacinthEngine.camMutex.lock();
			SimulateStruct sS;
			sS.pitch = p.pitch;
			sS.yaw = p.yaw;
			physicsManager.updateCamera(0, netClient.netEntManager.self->camSpeed, sS, hyacinthEngine.m_camera.m_transform, false, Time::getDeltaTime());

			hyacinthEngine.p_netEntManager->selfMutex.lock();
			netClient.netEntManager.self->transform.forward = hyacinthEngine.m_camera.m_transform.forward;
			netClient.netEntManager.self->transform.right = hyacinthEngine.m_camera.m_transform.right;
			netClient.netEntManager.self->transform.pitch = hyacinthEngine.m_camera.m_transform.pitch;
			netClient.netEntManager.self->transform.yaw = hyacinthEngine.m_camera.m_transform.yaw;
			hyacinthEngine.p_netEntManager->selfMutex.unlock();

			ServerSnapshot selfInterp = netClient.netEntManager.selfSimBuffer.getInterpolatedSimPacket(Time::getDeltaTime());
			hyacinthEngine.m_camera.m_transform.position = selfInterp.entities[0].transform.position;
			hyacinthEngine.m_camera.m_transform.position.y += 1.85f;

			hyacinthEngine.m_camera.update();
			hyacinthEngine.camMutex.unlock();
		}

		// read packets from the server
		ServerSnapshot interp = netClient.netEntManager.packetBuffer.getInterpolatedSimPacket(Time::getDeltaTime());
		// use packet to determine object transforms
		netClient.netEntManager.updateEntitiesFromPacket(interp, netClient.netEntManager.self->id, Time::getDeltaTime());
		physicsManager.setNetworkEntityCapColliderPosition(&interp, netClient.netEntManager.self->id);

		Time::updateTime();
		InputManager::resetMouseMotion();
	}

	dontEnd = false;
	tickThread.join();

	netClient.shutdownNet();

	return 0;
} 