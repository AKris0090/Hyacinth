#include "pch.h"
#include "flashbang.h"

// *********************** CONTROLLER *********************** //
FlashAnimController::FlashAnimController(LightMesh* meshRef) {
	baseNode = meshRef->getNodeByName("base");
}

// *********************** OBJECT *********************** // 

void HFlashBang::updateAnimation(float deltaTime, bool updateMatrices) {
	HAnimatedGameObject::updateAnimation(deltaTime, updateMatrices);
}

HFlashBang::HFlashBang(LightMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = FlashAnimController(meshIn);
}