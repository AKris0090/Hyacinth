#pragma once

#include "gltfcommon.h"
#include "entity.h"

constexpr float HORIZONTAL_GUN_SWAY = 0.13f;
constexpr float VERTICAL_GUN_SWAY = 0.25f;

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

enum ANIMATION_TYPE {
	A_TP_IDLE,
	A_TP_RUNNING,

	A_TP_LEFT_TURN,
	A_TP_RIGHT_TURN,

	A_PISTOL_EQUIP,
	A_PISTOL_IDLE,
	A_PISTOL_SHOOT,
	A_PISTOL_RELOAD,

	A_GRENADE_EQUIP,
	A_GRENADE_IDLE,
	A_GRENADE_THROW,
};

class AnimControllerBase {
public:
	std::unordered_map<ANIMATION_TYPE, Animation*> animations;
	void updateTime();
};

class ThirdPersonAnimationController : public AnimControllerBase {
public:
	gltfNode* upperArmL;    // left arm (pitch) controller
	gltfNode* upperArmR;    // right arm (pitch) controller
	gltfNode* spine005;     // head neck (pitch) controller
	gltfNode* spine007;     // lower body yaw controller
	gltfNode* spine003;     // upper body yaw controller
	gltfNode* spine;		// full body yaw controller

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
		currentLowerBodyAnim = currentUpperBodyAnim = previousAnimation = nullptr;
	};
};

class FirstPersonAnimationController : public AnimControllerBase {
public:
	gltfNode* gunBone;
	gltfNode* leftWrist;
	gltfNode* rightWrist;

	float currentTime = 0.f;

	Animation* currentAnim;

	FirstPersonAnimationController() {
		gunBone = leftWrist = rightWrist = nullptr;
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

class FirstPersonAnimationStateMachine {
private:

	glm::quat currentSwayYaw = { 1.f, 0.f, 0.f, 0.f };
	glm::quat currentSwayPitch = { 1.f, 0.f, 0.f, 0.f };
	WEAPON_STATE previousState = NULL_STATE;
	void flushQueuedNodeTransforms(FirstPersonAnimationController& c);
	void updateAnimation(FirstPersonAnimationController& c, float deltaTime, float deltaPitch, float deltaYaw);

public:
	void updateAnimationState(FirstPersonAnimationController& c, WEAPON_STATE state, float deltaTime, float deltaPitch, float deltaYaw, bool& shootingOut, bool& reloadOut);
};

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