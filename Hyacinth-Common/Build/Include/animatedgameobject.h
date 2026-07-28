#pragma once

#include "light_loader.h"
#include "entity.h" // for anim state stuff

enum TURN_ANIM_STATE {
	NEEDS_TURN_LEFT,
	NEEDS_TURN_RIGHT,
	TURNING,
	IDLE
};

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
	std::unordered_map<ANIMATION_TYPE, LightAnimation*> animations;
	void updateTime();

	static void updateSamplers(LightAnimation* animation, LightAnimChannel* channel, Transform* t, float currentTime);
};

// not listed, but each needs an update function for parameters used in animation update
class HAnimatedGameObject {
private:
	void updateJoints();

public:
	bool active = true;
	HAnimatedGameObject* parentObject = nullptr;
	LightNode* parentMeshNode = nullptr;
	Transform transform;
	LightMesh* mesh = nullptr;
	std::unordered_map<uint32_t, Transform> nodeTransforms;
	glm::mat4* jointMatrixData = nullptr;

	virtual void updateAnimation(float deltaTime);

	void destroy();

	// helpers for setting up node transform storage
	static void hookUpTransformParents(LightNode* n, std::unordered_map<uint32_t, Transform>& nodeTransforms);
	static void addNodeTransform(LightNode* n, std::unordered_map<uint32_t, Transform>& nodeTransforms);

	HAnimatedGameObject() {};
	HAnimatedGameObject(LightMesh* meshRef);
	glm::mat4 getStackedNodeMatrix(LightNode* node);
	void setParentObject(HAnimatedGameObject* aobject, LightNode* childNode, LightNode* parentNode);
};