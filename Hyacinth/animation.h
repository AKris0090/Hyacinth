#pragma once

#include "gltfcommon.h"

struct Skin
{
	std::string					name;
	gltfNode* skeletonRoot =	nullptr;
	std::vector<glm::mat4>		inverseBindMatrices;
	std::vector<gltfNode*>		joints;
	VulkanBuffer				jointMatrixBuffer;

	static void loadSkins(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Skin>& skinsOut);
};

struct AnimationSampler
{
	std::string            interpolation;
	std::vector<float>     inputs;
	std::vector<glm::vec4> outputsVec4;
};

struct AnimationChannel
{
	std::string path;
	gltfNode*	node;
	uint32_t    samplerIndex;
};

struct Animation
{
	std::string                   name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float                         start = std::numeric_limits<float>::max();
	float                         end = std::numeric_limits<float>::min();
	float                         currentTime = 0.0f;

	static void loadAnimations(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Animation>& animsOut);
};

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

class AnimationStateMachine {
private:
	void setNewBasis(float basis) {
		yawDelta = basis;
		yawDelta = fmod(yawDelta, 360);
		if (yawDelta < 0.f) {
			yawDelta += 360.f;
		}
		basisRotation = glm::angleAxis(glm::radians(yawDelta), glm::vec3(0, 1, 0));
	}

	void turn(float absolute) {
		prevBasisRotation = basisRotation;
		setNewBasis(absolute);
	}

	TURN_ANIM_STATE turnState = IDLE;
	void stageTurnAnim(bool leftRight) {
		turnState = leftRight ? NEEDS_TURN_LEFT : NEEDS_TURN_RIGHT;
	}

	glm::quat prevBasisRotation{ 1.f, 0.f, 0.f, 0.f };
	glm::quat basisRotation{ 1.f, 0.f, 0.f, 0.f };

	void flushQueuedNodeTransforms();
	void updateSamplers(Animation* animation, AnimationChannel* channel);
	void updateUpperAnimation(float deltaTime);
	void updateLowerAnimation(float deltaTime);

public:
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

	Animation* currentLowerBodyAnim;
	Animation* currentUpperBodyAnim;

	CURRENT_PLAYER_MOTION_STATE motionState = STILL;
	float yawDelta = 0.f;
	glm::quat spineDefaultRot;

	void updateFromPlayerState(float pitch, float yaw, float alpha, bool isMoving);
	void updateAnimationState(float deltaTime, float motionFB, float motionLR, float pitch, float yaw);
};