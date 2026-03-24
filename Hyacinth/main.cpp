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

void simulationTick(HyacinthEngine* engine) {
	using namespace std::chrono_literals;

	while (true) {
		engine->m_camera.update(0.0071825f, engine->mouseLocked);
		std::this_thread::sleep_for(7.1825ms);
	}
}

int main() {
	hyacinthPhysicsTest();

	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	HyacinthNetworkClient netClient;
	netClient.netEntManager.p_cam = &hyacinthEngine.m_camera;
	std::string ip;
	std::cout << "Enter server IP: ";
	std::getline(std::cin, ip);
	if (CONNECT_SERVER) std::cout << (netClient.setup(ip, hyacinthEngine.m_swImageFormat, hyacinthEngine.m_descriptorSetLayout) ? "CONNECTION FAILED" : "CONNECTION SUCCESSFUL") << std::endl;
	hyacinthEngine.p_netEntManager = &netClient.netEntManager;

	Time::setInitialTime();

	std::thread tickThread = std::thread(simulationTick, &hyacinthEngine);
	tickThread.detach();

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
		if (hyacinthEngine.mouseLocked) {
			netClient.updateServerTick();
		}

		Time::updateTime();
	}

	netClient.shutdownNet();

	return 0;
} 