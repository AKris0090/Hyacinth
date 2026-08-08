#include "pch.h"
#include "pistol.h"

// *********************** CONTROLLER *********************** //
PistolAnimationController::PistolAnimationController(LightMesh* meshRef) {
    baseNode = meshRef->getNodeByName("base");

    animations[A_PISTOL_IDLE] = &meshRef->animations[0];
    animations[A_PISTOL_SHOOT] = &meshRef->animations[1];
    animations[A_PISTOL_RELOAD] = &meshRef->animations[2];

    currentAnim = animations[A_PISTOL_IDLE];
}

void PistolAnimationController::updateAnimParams(bool queueShoot, bool queueReload) {
	shoot = queueShoot;
	reload = queueReload;
}

// *********************** ANIM STATE MACHINE *********************** //
void PistolAnimationStateMachine::updateAnimationState(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime) {
	if (c.reload) {
		c.currentAnim = c.animations[A_PISTOL_RELOAD];
		c.currentTime = c.currentAnim->start;
	}
	else if (c.shoot) {
		c.currentAnim = c.animations[A_PISTOL_SHOOT];
		c.currentTime = c.currentAnim->start;
	}
	c.currentTime += deltaTime;

	if (c.currentTime > c.currentAnim->end) {
		c.currentAnim = c.animations[A_PISTOL_IDLE];
		c.currentTime = c.currentAnim->start;
	}

	c.currentTime = fmod(c.currentTime, c.currentAnim->end);

	for (auto& channel : c.currentAnim->channels)
	{
		AnimControllerBase::updateSamplers(c.currentAnim, &channel, &transformMap[channel.node->nodeIndex], c.currentTime);
	}
}

// *********************** OBJECT *********************** // 

HPistol::HPistol(LightMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = PistolAnimationController(meshIn);
}

void HPistol::updateAnimation(float deltaTime, bool updateMatrices) {
	PistolAnimationStateMachine::updateAnimationState(controller, nodeTransforms, deltaTime);
	HAnimatedGameObject::updateAnimation(deltaTime);
}