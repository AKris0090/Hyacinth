#include "sdlwindow.h"
#include "vkengine.h"
#include "input.h"
#include "time.h"

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	Time::setInitialTime();

	while(sdlwindow.running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			Input::handleSDLInput(event);
			if (event.type == SDL_EVENT_QUIT) {
				sdlwindow.running = false;
			}
		}
		hyacinthEngine.m_camera.update(Time::getDeltaTime());
		hyacinthEngine.draw();

		Time::updateTime();
	}

	return 0;
} 