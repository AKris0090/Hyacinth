#pragma once

#include <SDL3/SDL.h>
#include <utility>

namespace InputManager {
	// setter method
	void handleSDLInput(SDL_Event& e);

	// getter methods;
	bool forwardKeyDown();
	bool backwardKeyDown();
	bool rightKeyDown();
	bool leftKeyDown();
	bool upKeyDown();
	bool downKeyDown();
	bool mouseDown();
	bool tabKeyDown();

	std::pair<float, float> getMouseMotion();
};