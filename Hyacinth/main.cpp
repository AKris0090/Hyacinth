#include "sdlwindow.h"
#include "hyacinthengine.h"
#include "hyacinth-client.h"
#include "input.h"
#include "time.h"

#define CONNECT_SERVER true

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1920, 1080);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	HyacinthNetworkClient netClient;
	std::string ip;
	std::cout << "Enter server IP: ";
	std::getline(std::cin, ip);
	if (CONNECT_SERVER) std::cout << netClient.setup(ip) << std::endl;

	Time::setInitialTime();

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
		netClient.sendPositionString(hyacinthEngine.m_camera.m_transform);

		Time::updateTime();
	}

	return 0;
} 