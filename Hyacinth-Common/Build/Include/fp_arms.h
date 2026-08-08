#pragma once

#include "animatedgameobject.h"

constexpr float HORIZONTAL_GUN_SWAY = 0.13f;
constexpr float VERTICAL_GUN_SWAY = 0.25f;

class FirstPersonAnimationController : public AnimControllerBase {
public:
	LightNode* gunBone = nullptr;
	LightNode* leftWrist = nullptr;
	LightNode* rightWrist = nullptr;
	LightAnimation* currentAnim = nullptr;

	WEAPON_STATE currentState = NULL_STATE;
	WEAPON_STATE previousState = NULL_STATE;
	glm::quat currentSwayYaw = { 1.f, 0.f, 0.f, 0.f };
	glm::quat currentSwayPitch = { 1.f, 0.f, 0.f, 0.f };

	float previousPitch = 0.f, previousYaw = 0.f;
	float deltaPitch = 0.f, deltaYaw = 0.f;
	bool reloadTrigger = false;
	bool shootTrigger = true;

	float currentTime = 0.f;

	FirstPersonAnimationController() {};
	FirstPersonAnimationController(LightMesh* meshRef);
	void updateAnimParams(WEAPON_STATE newState, float deltaPitch, float deltaYaw);
};

class FirstPersonAnimationStateMachine {
private:
	static void flushQueuedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
public:
	static void updateAnimationState(FirstPersonAnimationController& c, float deltaTime);
	static void updateAnimatedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HFPArms : public HAnimatedGameObject {
public:
	FirstPersonAnimationController controller;

	HFPArms(LightMesh* meshRef);
	void updateAnimation(float deltaTime, bool updateMatrices) override;
};