#pragma once

#include "animatedgameobject.h"

struct PistolAnimationController : public AnimControllerBase {
	HSkinnedMeshNode* baseNode = nullptr;
	float currentTime = 0.f;
	bool done = false;
	bool shoot = false;
	bool reload = false;
	HAnimation* currentAnim = nullptr;

	PistolAnimationController() {};
	PistolAnimationController(HSkinnedMesh* meshRef);
	void updateAnimParams(bool queueShoot, bool queueReload);
};

class PistolAnimationStateMachine {
public:
	static void updateAnimationState(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HPistol : public HAnimatedGameObject {
public:
	PistolAnimationController controller;

	HPistol(HSkinnedMesh* meshRef);
	void updateAnimation(float deltaTime) override;
};