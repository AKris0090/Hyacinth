#include "sdlwindow.h"
#include "vkengine.h"
#include "input.h"
#include "time.h"

int main() {
	SDLWindow sdlwindow;
	bool mouseLocked = true;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	Time::setInitialTime();

	while(sdlwindow.running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);

			Input::handleSDLInput(event);
			if (event.type == SDL_EVENT_QUIT) {
				sdlwindow.running = false;
			}
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
				mouseLocked = !mouseLocked;
				SDL_SetWindowRelativeMouseMode(hyacinthEngine.m_window, mouseLocked);
			}
		}
		hyacinthEngine.draw();

		Time::updateTime();
	}

	return 0;
} 