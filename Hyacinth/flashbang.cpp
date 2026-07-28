#include "flashbang.h"

// *********************** CONTROLLER *********************** //
FlashAnimController::FlashAnimController(LightMesh* meshRef) {
	baseNode = meshRef->getNodeByName("base");
}

// *********************** OBJECT *********************** // 

void HFlashBang::updateAnimation(float deltaTime) {
	HAnimatedGameObject::updateAnimation(deltaTime);
}

HFlashBang::HFlashBang(LightMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = FlashAnimController(meshIn);
}