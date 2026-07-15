#pragma once

#include "entity.h"
#include "animatedgameobject.h"

class ThirdPersonAnimationController : public AnimControllerBase {
public:
	HSkinnedMeshNode* upperArmL = nullptr;    // left arm (pitch) controller
	HSkinnedMeshNode* upperArmR = nullptr;    // right arm (pitch) controller
	HSkinnedMeshNode* spine005 = nullptr;     // head neck (pitch) controller
	HSkinnedMeshNode* spine007 = nullptr;     // lower body yaw controller
	HSkinnedMeshNode* spine003 = nullptr;     // upper body yaw controller
	HSkinnedMeshNode* spine = nullptr;		// full body yaw controller

	float currentLowerTime = 0.f;
	float currentUpperTime = 0.f;
	float previousTime = 0.f;

	HAnimation* previousAnimation = nullptr;
	std::vector<Transform> previousAnimationTransforms;

	HAnimation* currentLowerBodyAnim = nullptr;
	HAnimation* currentUpperBodyAnim = nullptr;

	glm::quat prevBasisRotation{ 1.f, 0.f, 0.f, 0.f };
	glm::quat basisRotation{ 1.f, 0.f, 0.f, 0.f };

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;

	CURRENT_PLAYER_MOTION_STATE motionState = STILL;
	TURN_ANIM_STATE turnState = IDLE;

	float pitch, yaw, alpha;
	bool isMoving;

	ThirdPersonAnimationController() {};
	ThirdPersonAnimationController(HSkinnedMesh* mesh);

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
	static void transitionToNewAnimation(ThirdPersonAnimationController& c, HAnimation* current, HAnimation* next);

public:
	static void updateAnimationState(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime);
};

class HTPCharacter : public HAnimatedGameObject {
public:
	ThirdPersonAnimationController controller;
	ThirdPersonAnimationStateMachine stateMachine;

	HTPCharacter(HSkinnedMesh* meshRef);
	void updateAnimation(float deltaTime) override;
};