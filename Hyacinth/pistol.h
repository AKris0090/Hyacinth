#pragma once

#include "animatedgameobject.h"

struct PistolAnimationController {
	Animation* idleAnimation;
	Animation* shootAnimation;
	Animation* reloadAnimation;
	float currentTime = 0.f;
	bool done = false;
	bool queueShoot = false;
	bool queueReload = false;

	Animation* currentAnim;

	PistolAnimationController() {
		currentAnim = idleAnimation = shootAnimation = nullptr;
	};
};

class PistolAnimationStateMachine {
private:
	void updateAnimation(PistolAnimationController& c, float deltaTime);
public:
	void updateAnimationState(PistolAnimationController& c, float deltaTime);
};