#pragma once

#include "hcinth_animatedmesh.h"

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
	std::unordered_map<ANIMATION_TYPE, HAnimation*> animations;
	void updateTime();

	static void updateSamplers(HAnimation* animation, HAnimChannel* channel, Transform* t, float currentTime);
};

// not listed, but each needs an update function for parameters used in animation update
class HAnimatedGameObject {
private:
	void updateJoints();

public:
	bool active = true;
	HAnimatedGameObject* parentObject = nullptr;
	HSkinnedMeshNode* parentMeshNode = nullptr;
	Transform transform;
	HSkinnedMesh* mesh = nullptr;
	std::unordered_map<uint32_t, Transform> nodeTransforms;
	VulkanBuffer jointMatrixBuffer{};

	virtual void updateAnimation(float deltaTime);

	void destroy();

	HAnimatedGameObject() {};
	HAnimatedGameObject(HSkinnedMesh* meshRef);
	glm::mat4 getStackedNodeMatrix(HSkinnedMeshNode* node);
	void setParentObject(HAnimatedGameObject* aobject, HSkinnedMeshNode* childNode, HSkinnedMeshNode* parentNode);
};