#include "flashbang.h"

// *********************** CONTROLLER *********************** //
FlashAnimController::FlashAnimController(HSkinnedMesh* meshRef) {
	baseNode = meshRef->getNodeByName("base");
}

// *********************** OBJECT *********************** // 

void HFlashBang::updateAnimation(float deltaTime) {
	HAnimatedGameObject::updateAnimation(deltaTime);
}

HFlashBang::HFlashBang(HSkinnedMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = FlashAnimController(meshIn);
}