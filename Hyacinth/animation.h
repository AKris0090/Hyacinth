#pragma once

#include "gltfcommon.h"

enum TURN_ANIM_STATE {
	NEEDS_TURN_LEFT,
	NEEDS_TURN_RIGHT,
	TURNING,
	IDLE
};

enum CURRENT_PLAYER_MOTION_STATE {
	MOVING,
	STILL
};

static float yawFromQuaternion(glm::quat q) {
	return glm::degrees(atan2(
		2.0f * (q.w * q.y + q.x * q.z),
		1.0f - 2.0f * (q.y * q.y + q.x * q.x)
	));
}

struct ThirdPersonAnimationController {
	gltfNode* upperArmL;    // left arm (pitch) controller
	gltfNode* upperArmR;    // right arm (pitch) controller
	gltfNode* spine005;     // head neck (pitch) controller
	gltfNode* spine007;     // lower body yaw controller
	gltfNode* spine003;     // upper body yaw controller
	gltfNode* spine;		// full body yaw controller

	Animation* leftTurnAnimation;
	Animation* rightTurnAnimation;
	Animation* idleAnimation;
	Animation* runningAnimation;

	float currentLowerTime = 0.f;
	float currentUpperTime = 0.f;
	float previousTime = 0.f;

	Animation* previousAnimation;
	std::vector<Transform> previousAnimationTransforms;

	Animation* currentLowerBodyAnim;
	Animation* currentUpperBodyAnim;

	glm::quat prevBasisRotation{ 1.f, 0.f, 0.f, 0.f };
	glm::quat basisRotation{ 1.f, 0.f, 0.f, 0.f };

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;

	CURRENT_PLAYER_MOTION_STATE motionState = STILL;
	TURN_ANIM_STATE turnState = IDLE;

	ThirdPersonAnimationController() {
		upperArmL = upperArmR = spine005 = spine007 = spine003 = spine = nullptr;
		idleAnimation = leftTurnAnimation = rightTurnAnimation = runningAnimation = currentLowerBodyAnim = currentUpperBodyAnim = previousAnimation = nullptr;
	};

	ThirdPersonAnimationController(gltfNode* upperL, gltfNode* upperR, gltfNode* spine5, gltfNode* spine7, gltfNode* spine3, gltfNode* sp, Animation* idle, Animation* leftTurn, Animation* rightTurn, Animation* run, std::vector<gltfNode*> allNodes) {
		upperArmL = upperL;
		upperArmR = upperR;
		spine005 = spine5;
		spine007 = spine7;
		spine003 = spine3;
		spine = sp;

		idleAnimation = idle;
		leftTurnAnimation = leftTurn;
		rightTurnAnimation = rightTurn;
		runningAnimation = run;

		currentLowerBodyAnim = idle;
		currentUpperBodyAnim = idle;
		previousAnimation = nullptr;

		previousAnimationTransforms.resize(allNodes.size());
	}
};

struct FirstPersonAnimationController {
	Animation* idleAnimation;
	Animation* spinningAnimation;

	float currentTime = 0.f;
	float previousTime = 0.f;

	Animation* previousAnimation;
	std::vector<Transform> previousAnimationTransforms;

	Animation* currentAnim;

	float						  fadeTimer = 0.f;
	float						  fadeLength = 0.15f;
	bool						  transitioning = false;

	FirstPersonAnimationController() {
		currentAnim = idleAnimation = previousAnimation = spinningAnimation = nullptr;
	};

	FirstPersonAnimationController(Animation* idleAnim, Animation* spinningAn) {
		currentAnim = idleAnimation = idleAnim;
		previousAnimation = nullptr;
		spinningAnimation = spinningAn;
	};
};

static void updateSamplers(Animation* animation, AnimationChannel* channel, Transform* t, float currentTime);

class ThirdPersonAnimationStateMachine {
private:
	void setNewBasis(ThirdPersonAnimationController& c, float basis) {
		basis = fmodf(basis, 360);
		c.basisRotation = glm::angleAxis(glm::radians(basis), glm::vec3(0, 1, 0));
	}

	void turn(ThirdPersonAnimationController& c, float absolute) {
		c.prevBasisRotation = c.basisRotation;
		setNewBasis(c, absolute);
	}

	void stageTurnAnim(ThirdPersonAnimationController& c, bool leftRight) {
		c.turnState = leftRight ? NEEDS_TURN_LEFT : NEEDS_TURN_RIGHT;
	}

	void flushQueuedNodeTransforms(ThirdPersonAnimationController& c);
	void updateUpperAnimation(ThirdPersonAnimationController& c);
	void updateLowerAnimation(ThirdPersonAnimationController& c);
	void updatePreviousWholeBodyAnimation(ThirdPersonAnimationController& c);
	void lerpPreviousCurrentAnimations(ThirdPersonAnimationController& c);
	void updateFromPlayerState(ThirdPersonAnimationController& c, float pitch, float yaw, float alpha, bool isMoving);
	void transitionToNewAnimation(ThirdPersonAnimationController& c, Animation* current, Animation* next);

public:
	void updateAnimationState(ThirdPersonAnimationController& c, float deltaTime, float motionFB, float motionLR, float pitch, float yaw);
};

enum FIRSTPERSON_STATE {
	IDLE_PISTOL
};

class FirstPersonAnimationStateMachine {
private:
	FIRSTPERSON_STATE state;

	void updateAnimation(FirstPersonAnimationController& c, float deltaTime);
	void transitionToNewAnimation(FirstPersonAnimationController& c, Animation* current, Animation* next);

public:
	void updateAnimationState(FirstPersonAnimationController& c, float deltaTime);
};



struct PistolAnimationController {
	Animation* idleAnimation;
	float currentTime = 0.f;
	bool done = false;

	Animation* currentAnim;

	PistolAnimationController() {
		currentAnim = idleAnimation = nullptr;
	};

	PistolAnimationController(Animation* idleAnim) {
		currentAnim = idleAnimation = idleAnim;
	};
};

class PistolAnimationStateMachine {
private:
	void updateAnimation(PistolAnimationController& c, float deltaTime);
public:
	void updateAnimationState(PistolAnimationController& c, float deltaTime);
};