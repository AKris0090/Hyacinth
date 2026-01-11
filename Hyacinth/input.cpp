#include "input.h"

namespace Input {
	bool forward, backward;
	bool left, right;
	bool up, down;
	float xrel, yrel;
	bool mouseLeft;

	void handleSDLInput(SDL_Event& e) {
		if (e.type == SDL_EVENT_KEY_DOWN) {
			if (e.key.scancode == SDL_SCANCODE_W) { forward = true; }

			if (e.key.scancode == SDL_SCANCODE_S) { backward = true; }

			if (e.key.scancode == SDL_SCANCODE_A) { left = true; }

			if (e.key.scancode == SDL_SCANCODE_D) { right = true; }

			if (e.key.scancode == SDL_SCANCODE_E) { up = true; }

			if (e.key.scancode == SDL_SCANCODE_Q) { down = true; }
		}

		if (e.type == SDL_EVENT_KEY_UP) {
			if (e.key.scancode == SDL_SCANCODE_W) { forward = false; }

			if (e.key.scancode == SDL_SCANCODE_S) { backward = false; }

			if (e.key.scancode == SDL_SCANCODE_A) { left = false; }

			if (e.key.scancode == SDL_SCANCODE_D) { right = false; }

			if (e.key.scancode == SDL_SCANCODE_E) { up = false; }

			if (e.key.scancode == SDL_SCANCODE_Q) { down = false; }
		}

		if (e.type == SDL_EVENT_MOUSE_MOTION) {
			xrel = static_cast<float>(e.motion.xrel);
			yrel = static_cast<float>(e.motion.yrel);
		}

		if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mouseLeft = true;
		}

		if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			mouseLeft = false;
		}
	}

	bool forwardKeyDown() {
		return forward;
	}

	bool backwardKeyDown() {
		return backward;
	}

	bool rightKeyDown() {
		return right;
	}

	bool leftKeyDown() {
		return left;
	}

	bool upKeyDown() {
		return up;
	}

	bool downKeyDown() {
		return down;
	}

	bool mouseDown() {
		return mouseLeft;
	}

	std::pair<float, float> getMouseMotion() {
		std::pair<float, float> motion(xrel, yrel);
		xrel = 0.0f;
		yrel = 0.0f;
		return motion;
	}
}