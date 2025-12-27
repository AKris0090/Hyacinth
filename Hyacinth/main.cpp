#include "sdlwindow.h"
#include "vkengine.h"

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	HyacinthEngine hyacinthEngine;
	hyacinthEngine.m_window = sdlwindow.m_window;
	hyacinthEngine.init();

	while(sdlwindow.isRunning()) {
		sdlwindow.pollEvents();

		hyacinthEngine.draw();
	}

	return 0;
} 