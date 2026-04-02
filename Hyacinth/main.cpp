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

std::filesystem::path getExeDir()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
}

void simulationTick(HyacinthEngine* engine) {
	while (true) {
		engine->p_netEntManager->selfMutex.lock();
		engine->camMutex.lock();
		engine->m_camera.m_transform = engine->p_netEntManager->self->transform;
		engine->m_camera.m_transform.position.y += 1.85f;
		engine->camMutex.unlock();
		engine->p_netEntManager->selfMutex.unlock();
		std::this_thread::sleep_for(SERVER_TIMESTEP_MS);
	}
}

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	PhysicsManager physicsManager;
	physicsManager.initPhysics();
	LightLoader loader;
	auto path = getExeDir() / "objects" / "sponza" / "sponza.gltf";
	physicsManager.addStaticPhysicsObject(loader.loadFromFile(path.string(), true));
	physicsManager.addCharacterController(0);

	HyacinthNetworkClient netClient;
	Entity* thisEnt;
	std::string ip;
	std::cout << "Enter server IP: ";
	// std::getline(std::cin, ip);
	if (CONNECT_SERVER) std::cout << (netClient.setup("", hyacinthEngine.m_swImageFormat, hyacinthEngine.m_descriptorSetLayout) ? "CONNECTION FAILED" : "CONNECTION SUCCESSFUL") << std::endl;
	hyacinthEngine.p_netEntManager = &netClient.netEntManager;
	thisEnt = netClient.netEntManager.self;


	Time::setInitialTime();

	// std::thread tickThread = std::thread(simulationTick, &hyacinthEngine);
	// tickThread.detach();

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
		hyacinthEngine.draw();
		netClient.updateServerTick(hyacinthEngine.mouseLocked);

		// update physics
		SimulateStruct s;
		s.id = 0;
		std::array<int8_t, 3> m = InputManager::getMovement();
		s.movementFB = m[0];
		s.movementLR = m[1];
		s.movementUD = m[2];
		std::pair<float, float> mo = InputManager::getMouseMotion();
		s.xRelMouse = mo.first;
		s.yRelMouse = mo.second;

		ServerPacket interp = netClient.netEntManager.packetBuffer.getInterpolatedSimPacket(Time::getDeltaTime());
		netClient.netEntManager.updateEntitiesFromPacket(interp, netClient.netEntManager.self->id);
		netClient.netEntManager.selfMutex.lock();
		hyacinthEngine.camMutex.lock();
		// physicsManager.updateSingleEntity(0, thisEnt->camSpeed, thisEnt->transform, s, Time::getDeltaTime(), Time::getDeltaTime());
		hyacinthEngine.m_camera.m_transform = thisEnt->transform;
		hyacinthEngine.m_camera.m_transform.position.y += 1.85f;
		hyacinthEngine.m_camera.update();
		hyacinthEngine.camMutex.unlock();
		netClient.netEntManager.selfMutex.unlock();

		Time::updateTime();
		InputManager::resetMouseMotion();
	}

	netClient.shutdownNet();

	return 0;
} 