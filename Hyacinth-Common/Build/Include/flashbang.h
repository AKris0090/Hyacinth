#pragma once

#include "animatedgameobject.h"

struct FlashAnimController : public AnimControllerBase {
	float currentTime = 0.f;
	LightNode* baseNode = nullptr;
	LightAnimation* currentAnim = nullptr;

	FlashAnimController() {};
	FlashAnimController(LightMesh* meshRef);
};

class HFlashBang : public HAnimatedGameObject {
public:
	FlashAnimController controller;

	HFlashBang(LightMesh* meshRef);
	void updateAnimation(float deltaTime, bool updateMatrices) override;
};