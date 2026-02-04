#pragma once

#include <SDL3/SDL.h>
#include <iostream>
#include <windows.h>
#include <dwmapi.h>

class SDLWindow {
public:
	SDLWindow() {};
	~SDLWindow();

	void init(std::string title, int width, int height);
	void pollEvents(SDL_Event& event);

	SDL_Window* m_window = nullptr;
	bool running = true;
};