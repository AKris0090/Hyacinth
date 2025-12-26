#include "sdlwindow.h"

int main() {
	SDLWindow sdlwindow;
	sdlwindow.init("Hyacinth Engine", 1280, 720);

	while(sdlwindow.running) {
		sdlwindow.pollEvents();
	}

	return 0;
} 