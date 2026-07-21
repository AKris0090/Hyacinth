#pragma once

#include "animatedgameobject.h"

struct FlashAnimController : public AnimControllerBase {
	float currentTime = 0.f;
	HSkinnedMeshNode* baseNode = nullptr;
	HAnimation* currentAnim = nullptr;

	FlashAnimController() {};
	FlashAnimController(HSkinnedMesh* meshRef);
};

class HFlashBang : public HAnimatedGameObject {
public:
	FlashAnimController controller;

	HFlashBang(HSkinnedMesh* meshRef);
	void updateAnimation(float deltaTime) override;
};