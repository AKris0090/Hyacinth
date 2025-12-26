#pragma once

#include <SDL3/SDL.h>
#include <iostream>

class SDLWindow {
public:
	SDLWindow() {};
	~SDLWindow();

	void init(std::string title, int width, int height);
	void pollEvents();

	SDL_Window* pWindow = nullptr;
	SDL_Renderer* pRenderer = nullptr;
	bool running = true;
};