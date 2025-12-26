#include "sdlwindow.h"

SDLWindow::~SDLWindow()
{
	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pWindow);
}

void SDLWindow::init(std::string title, int width, int height)
{
	SDL_Init(SDL_INIT_VIDEO);
	pWindow = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_SetWindowPosition(pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	pRenderer = SDL_CreateRenderer(pWindow, NULL);
	SDL_SetWindowRelativeMouseMode(pWindow, true);
}

void SDLWindow::pollEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			running = false;
		}
	}
}
