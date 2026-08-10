#include "input.h"

namespace InputManager {
	bool forward, backward;
	bool left, right;
	bool up, down;
	float xrel, yrel;
	float xTickRel, yTickRel;
	bool mouseLeft;
	bool tabKey, space, b, r, one, two;
	bool shiftKey, ctrlKey;
	bool f1 = false;

	void handleSDLInput(SDL_Event& e) {
		if (e.type == SDL_EVENT_KEY_DOWN) {
			if (e.key.scancode == SDL_SCANCODE_W) { forward = true; }

			if (e.key.scancode == SDL_SCANCODE_S) { backward = true; }

			if (e.key.scancode == SDL_SCANCODE_A) { left = true; }

			if (e.key.scancode == SDL_SCANCODE_D) { right = true; }

			if (e.key.scancode == SDL_SCANCODE_E) { up = true; }

			if (e.key.scancode == SDL_SCANCODE_Q) { down = true; }

			if (e.key.scancode == SDL_SCANCODE_SPACE) { space = true; }

			if (e.key.scancode == SDL_SCANCODE_B) { b = true; }

			if (e.key.scancode == SDL_SCANCODE_R) { r = true; }

			if (e.key.scancode == SDL_SCANCODE_1) { one = true; }

			if (e.key.scancode == SDL_SCANCODE_2) { two = true; }

			if (e.key.scancode == SDL_SCANCODE_LSHIFT) { shiftKey = true; }

			if (e.key.scancode == SDL_SCANCODE_LCTRL) { ctrlKey = true; }
		}

		if (e.type == SDL_EVENT_KEY_UP) {
			if (e.key.scancode == SDL_SCANCODE_W) { forward = false; }

			if (e.key.scancode == SDL_SCANCODE_S) { backward = false; }

			if (e.key.scancode == SDL_SCANCODE_A) { left = false; }

			if (e.key.scancode == SDL_SCANCODE_D) { right = false; }

			if (e.key.scancode == SDL_SCANCODE_E) { up = false; }

			if (e.key.scancode == SDL_SCANCODE_Q) { down = false; }

			if (e.key.scancode == SDL_SCANCODE_TAB) { tabKey = false; }

			if (e.key.scancode == SDL_SCANCODE_SPACE) { space = false; }

			if (e.key.scancode == SDL_SCANCODE_B) { b = false; }

			if (e.key.scancode == SDL_SCANCODE_R) { r = false; }

			if (e.key.scancode == SDL_SCANCODE_1) { one = false; }

			if (e.key.scancode == SDL_SCANCODE_2) { two = false; }

			if (e.key.scancode == SDL_SCANCODE_F1) { f1 = !f1; }

			if (e.key.scancode == SDL_SCANCODE_LSHIFT) { shiftKey = false; }

			if (e.key.scancode == SDL_SCANCODE_LCTRL) { ctrlKey = false; }
		}

		if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
		{
			if (e.key.scancode == SDL_SCANCODE_TAB) { tabKey = true; }
		}

		if (e.type == SDL_EVENT_MOUSE_MOTION) {
			xrel = static_cast<float>(e.motion.xrel);
			yrel = static_cast<float>(e.motion.yrel);
			xTickRel = xrel;
			yTickRel = yrel;
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

	bool tabKeyDown() {
		return tabKey;
	}

	bool spaceKeyDown() {
		return space;
	}

	bool botKeyDown() {
		return b;
	}

	bool reloadKeyDown() {
		return r;
	}

	void resetMouseMotion() {
		xrel = 0.0;
		yrel = 0.0;
	}

	bool shiftKeyDown() {
		return shiftKey;
	}

	bool ctrlKeyDown() {
		return ctrlKey;
	}

	std::pair<float, float> getMouseMotion() {
		std::pair<float, float> motion(xrel, yrel);
		return motion;
	}

	std::pair<float, float> getTickMouseMotion() {
		std::pair<float, float> motion(xTickRel, yTickRel);
		xTickRel = 0.0;
		yTickRel = 0.0;
		return motion;
	}

	std::array<int8_t, 3> getMovement() {
		int8_t fb = 0;
		int8_t lr = 0;
		int8_t ud = 0;

		if (forwardKeyDown()) fb += 1;
		if (backwardKeyDown()) fb -= 1;
		if (rightKeyDown()) lr += 1;
		if (leftKeyDown()) lr -= 1;
		if (upKeyDown()) ud += 1;
		if (downKeyDown()) ud -= 1;

		return { fb, lr, ud };
	}

	bool getSpaceButton() {
		return spaceKeyDown();
	}

	bool num1KeyDown() {
		return one;
	}

	bool num2KeyDown() {
		return two;
	}

	bool getf1() {
		return f1;
	}
}