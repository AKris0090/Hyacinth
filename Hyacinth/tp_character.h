#pragma once

#include "entity.h"
#include "animatedgameobject.h"

class ThirdPersonAnimationController : public AnimControllerBase {
public:
	LightNode* upperArmL = nullptr;    // left arm (pitch) controller
	LightNode* upperArmR = nullptr;    // right arm (pitch) controller
	LightNode* spine005 = nullptr;     // head neck (pitch) controller
	LightNode* spine007 = nullptr;     // lower body yaw controller
	LightNode* spine003 = nullptr;     // upper body yaw controller
	LightNode* spine = nullptr;		// full body yaw controller

	float currentLowerTime = 0.f;
	float currentUpperTime = 0.f;
	float previousTime = 0.f;

	LightAnimation* previousAnimation = nullptr;
	std::unordered_map<uint32_t, Transform> previousAnimationTransforms;

	LightAnimation* currentLowerBodyAnim = nullptr;
	LightAnimation* currentUpperBodyAnim = nullptr;

	glm::quat prevBasisRotation{ 1.f, 0.f, 0.f, 0.f };
	glm::quat basisRotation{ 1.f, 0.f, 0.f, 0.f };

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;


	std::vector<bool> isUpperFlag;
	std::vector<bool> isLowerFlag;

	CURRENT_PLAYER_MOTION_STATE motionState = STILL;
	TURN_ANIM_STATE turnState = IDLE;

	float pitch = 0, yaw = 0, alpha = 0;
	bool isMoving = false;

	ThirdPersonAnimationController() {};
	ThirdPersonAnimationController(LightMesh* mesh);

	void updateAnimParams(Entity* entity);
};

class ThirdPersonAnimationStateMachine {
private:
	static void flushQueuedNodeTransforms(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void updateUpperAnimation(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void updateLowerAnimation(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void updatePreviousWholeBodyAnimation(ThirdPersonAnimationController& c);
	static void lerpPreviousCurrentAnimations(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void updateFromPlayerState(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap);
	static void transitionToNewAnimation(ThirdPersonAnimationController& c, LightAnimation* current, LightAnimation* next);

public:
	static void updateAnimationState(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HTPCharacter : public HAnimatedGameObject {
public:
	ThirdPersonAnimationController controller;
	ThirdPersonAnimationStateMachine stateMachine;

	HTPCharacter(LightMesh* meshRef);
	void updateAnimation(float deltaTime) override;
};