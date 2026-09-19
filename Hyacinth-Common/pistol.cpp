#include "pch.h"
#include "pistol.h"

// *********************** CONTROLLER *********************** //
PistolAnimationController::PistolAnimationController(LightMesh* meshRef) {
    // baseNode = meshRef->getNodeByName("base");

    animations[A_PISTOL_IDLE] = &meshRef->animations["m1911 idle"];
	animations[A_PISTOL_WALK] = &meshRef->animations["Walk Loop M1911"];
    // animations[A_PISTOL_SHOOT] = &meshRef->animations[1];
    // animations[A_PISTOL_RELOAD] = &meshRef->animations[2];

    currentAnim = animations[A_PISTOL_IDLE];
}

void PistolAnimationController::updateAnimParams(PLAYER_MOVEMENT_STATE moveState, bool queueShoot, bool queueReload) {
	shoot = queueShoot;
	reload = queueReload;

	currentMoveState = moveState;
}

// *********************** ANIM STATE MACHINE *********************** //
void PistolAnimationStateMachine::lerpPreviousCurrentAnimations(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap) {
	float alpha = c.fadeTimer / c.fadeLength;
	for (auto& [id, nodeT] : prevTransformMap)
	{
		Transform lerpedT = nodeT.lerpToNoSet(transformMap[id], alpha);
		transformMap[id].copy(lerpedT);
	}
}

void PistolAnimationStateMachine::updateAnimationState(PistolAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, std::unordered_map<uint32_t, Transform>& prevTransformMap, float deltaTime) {
	if (c.previousMoveState != c.currentMoveState) {
		switch (c.currentMoveState) {
		case M_IDLE:
			transitionAnimationState(c, A_PISTOL_IDLE);
			break;
		case WALKING:
			transitionAnimationState(c, A_PISTOL_WALK);
			break;
		default:
			break;
		}
		c.previousMoveState = c.currentMoveState;
	}

	// if (c.reload) {
	// 	c.currentAnim = c.animations[A_PISTOL_RELOAD];
	// 	c.currentTime = c.currentAnim->start;
	// }
	// else if (c.shoot) {
	// 	c.currentAnim = c.animations[A_PISTOL_SHOOT];
	// 	c.currentTime = c.currentAnim->start;
	// }

	c.currentTime += deltaTime;
	// 
	// if (c.currentTime > c.currentAnim->end) {
	// 	c.currentAnim = c.animations[A_PISTOL_IDLE];
	// 	c.currentTime = c.currentAnim->start;
	// }
	// 

	c.currentTime = fmod(c.currentTime, c.currentAnim->end);

	for (auto& channel : c.currentAnim->channels)
	{
		AnimControllerBase::updateSamplers(c.currentAnim, &channel, &transformMap[channel.node->nodeIndex], c.currentTime);
	}

	if (c.transitioning) {
		c.fadeTimer += deltaTime;
		if (c.fadeTimer >= c.fadeLength) {
			c.transitioning = false;
		}
		else {
			c.previousTime += deltaTime;
			c.previousTime = fmod(c.previousTime, c.prevAnim->end);

			for (auto& channel : c.prevAnim->channels)
			{
				AnimControllerBase::updateSamplers(c.prevAnim, &channel, &prevTransformMap[channel.node->nodeIndex], c.previousTime);
			}

			lerpPreviousCurrentAnimations(c, transformMap, prevTransformMap);
		}
	}
}

void PistolAnimationStateMachine::transitionAnimationState(PistolAnimationController& c, ANIMATION_TYPE newAnim) {
	c.prevAnim = c.currentAnim;
	c.previousTime = c.currentTime;
	c.fadeTimer = 0.f;
	c.transitioning = true;

	c.currentAnim = c.animations[newAnim];
	c.currentTime = c.currentAnim->start;
}

// *********************** OBJECT *********************** // 

HPistol::HPistol(LightMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = PistolAnimationController(meshIn);
}

void HPistol::updateAnimation(float deltaTime, bool updateMatrices) {
	PistolAnimationStateMachine::updateAnimationState(controller, nodeTransforms, prevNodeTransforms, deltaTime);
	HAnimatedGameObject::updateAnimation(deltaTime);
}