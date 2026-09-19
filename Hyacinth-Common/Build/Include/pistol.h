#pragma once

#include "animatedgameobject.h"

struct PistolAnimationController : public AnimControllerBase {
	LightNode* baseNode = nullptr;

	float currentTime = 0.f;
	float previousTime = 0.f;

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;

	bool done = false;
	bool shoot = false;
	bool reload = false;
	bool walking = false;
	LightAnimation* currentAnim = nullptr;
	LightAnimation* prevAnim = nullptr;

	PLAYER_MOVEMENT_STATE currentMoveState = M_IDLE;
	PLAYER_MOVEMENT_STATE previousMoveState = M_IDLE;

	PistolAnimationController() {};
	PistolAnimationController(LightMesh* meshRef);
	void updateAnimParams(PLAYER_MOVEMENT_STATE moveState, bool queueShoot, bool queueReload);
};

class PistolAnimationStateMachine {
private:
	static void transitionAnimationState(PistolAnimationController& c, ANIMATION_TYPE newAnim);
	static void lerpPreviousCurrentAnimations(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap);
public:
	static void updateAnimationState(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap, float deltaTime);
};

class HPistol : public HAnimatedGameObject {
public:
	PistolAnimationController controller;

	HPistol(LightMesh* meshRef);
	void updateAnimation(float deltaTime, bool updateMatrices) override;
};