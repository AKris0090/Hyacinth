#pragma once

#include <SDL3/SDL.h>
#include "glm/glm.hpp"
#include <utility>
#include <array>

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
	bool spaceKeyDown();
	bool botKeyDown();

	void resetMouseMotion();
	std::pair<float, float> getMouseMotion();
	std::pair<float, float> getTickMouseMotion();
	std::array<int8_t, 3> getMovement();
	bool getSpaceButton();
};