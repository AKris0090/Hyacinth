#pragma once

#include <SDL3/SDL.h>
#include <iostream>

class SDLWindow {
public:
	SDLWindow() {};
	~SDLWindow();

	void init(std::string title, int width, int height);
	void pollEvents();
	bool isRunning() const { return running; }

	SDL_Window* m_window = nullptr;

private:
	bool running = true;
};