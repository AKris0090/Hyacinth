#pragma once

#include "animatedgameobject.h"

constexpr float HORIZONTAL_GUN_SWAY = 0.13f;
constexpr float VERTICAL_GUN_SWAY = 0.25f;

class FirstPersonAnimationController : public AnimControllerBase {
public:
	HSkinnedMeshNode* gunBone = nullptr;
	HSkinnedMeshNode* leftWrist = nullptr;
	HSkinnedMeshNode* rightWrist = nullptr;
	HAnimation* currentAnim = nullptr;

	WEAPON_STATE currentState = NULL_STATE;
	WEAPON_STATE previousState = NULL_STATE;
	glm::quat currentSwayYaw = { 1.f, 0.f, 0.f, 0.f };
	glm::quat currentSwayPitch = { 1.f, 0.f, 0.f, 0.f };

	float deltaPitch = 0.f, deltaYaw = 0.f;

	float currentTime = 0.f;

	FirstPersonAnimationController() {};
	FirstPersonAnimationController(HSkinnedMesh* mesh);
	void updateAnimParams(WEAPON_STATE newState, float deltaPitch, float deltaYaw);
};

class FirstPersonAnimationStateMachine {
private:
	static void flushQueuedNodeTransforms(FirstPersonAnimationController& c);
public:
	static void updateAnimationState(FirstPersonAnimationController& c, float deltaTime);
	static void updateAnimatedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HFPArms : public HAnimatedGameObject {
public:
	FirstPersonAnimationController controller;
	FirstPersonAnimationStateMachine stateMachine;

	HFPArms(HSkinnedMesh* meshRef);
	void updateAnimation(float deltaTime) override;
};