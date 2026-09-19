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
	LightAnimation* prevAnim = nullptr;
	
	WEAPON_STATE currentState = NULL_STATE;
	WEAPON_STATE previousState = NULL_STATE;
	glm::quat currentSwayYaw = { 1.f, 0.f, 0.f, 0.f };
	glm::quat currentSwayPitch = { 1.f, 0.f, 0.f, 0.f };

	PLAYER_MOVEMENT_STATE currentMoveState = M_IDLE;
	PLAYER_MOVEMENT_STATE previousMoveState = M_IDLE;
	
	float previousPitch = 0.f, previousYaw = 0.f;
	float deltaPitch = 0.f, deltaYaw = 0.f;
	bool reloadTrigger = false;
	bool shootTrigger = true;

	float currentTime = 0.f;
	float previousTime = 0.f;

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;

	FirstPersonAnimationController() {};
	FirstPersonAnimationController(LightMesh* meshRef);
	void updateAnimParams(WEAPON_STATE newState, PLAYER_MOVEMENT_STATE moveState, float deltaPitch, float deltaYaw);
};

class FirstPersonAnimationStateMachine {
private:
	static void flushQueuedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void transitionAnimationState(FirstPersonAnimationController& c, ANIMATION_TYPE newAnim);
	static void lerpPreviousCurrentAnimations(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap);
public:
	static void updateAnimationState(FirstPersonAnimationController& c, float deltaTime);
	static void updateAnimatedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap, float deltaTime);
};

class HFPArms : public HAnimatedGameObject {
public:
	FirstPersonAnimationController controller;

	HFPArms(LightMesh* meshRef);
	void updateAnimation(float deltaTime, bool updateMatrices) override;
};