#pragma once

#include "animatedgameobject.h"

struct PistolAnimationController : public AnimControllerBase {
	LightNode* baseNode = nullptr;
	float currentTime = 0.f;
	bool done = false;
	bool shoot = false;
	bool reload = false;
	LightAnimation* currentAnim = nullptr;

	PistolAnimationController() {};
	PistolAnimationController(LightMesh* meshRef);
	void updateAnimParams(bool queueShoot, bool queueReload);
};

class PistolAnimationStateMachine {
public:
	static void updateAnimationState(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HPistol : public HAnimatedGameObject {
public:
	PistolAnimationController controller;

	HPistol(LightMesh* meshRef);
	void updateAnimation(float deltaTime, bool updateMatrices) override;
};