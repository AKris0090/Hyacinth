#include "fp_arms.h"

// *********************** CONTROLLER *********************** //
FirstPersonAnimationController::FirstPersonAnimationController(HSkinnedMesh* meshRef) {
	gunBone = meshRef->getNodeByName("gun");
    leftWrist = meshRef->getNodeByName("hand.L");
    rightWrist = meshRef->getNodeByName("hand.R");

    animations[A_GRENADE_THROW] = &meshRef->animations[1];
    animations[A_GRENADE_IDLE] = &meshRef->animations[2];
    animations[A_GRENADE_EQUIP] = &meshRef->animations[3];

    animations[A_PISTOL_RELOAD] = &meshRef->animations[4];
    animations[A_PISTOL_SHOOT] = &meshRef->animations[5];
    animations[A_PISTOL_IDLE] = &meshRef->animations[6];
    animations[A_PISTOL_EQUIP] = &meshRef->animations[7];

    currentAnim = animations[A_PISTOL_IDLE];
}

void FirstPersonAnimationController::updateAnimParams(WEAPON_STATE newState, float currentDPitch, float currentDYaw) {
	currentState = newState;
	deltaPitch = currentDPitch;
	deltaYaw = currentDYaw;
}

// *********************** ANIM STATE MACHINE *********************** //

void FirstPersonAnimationStateMachine::flushQueuedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	for (auto& node : { c.leftWrist, c.rightWrist }) {
		Transform& t = transformMap[node->nodeIndex];
		t.rotation = t.queuedQuatRotation * t.rotation;
	}
}

void FirstPersonAnimationStateMachine::updateAnimatedNodeTransforms(FirstPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime) {
	c.currentTime += deltaTime;
	c.currentTime = fmod(c.currentTime, c.currentAnim->end);

	for (auto& channel : c.currentAnim->channels)
	{
		AnimControllerBase::updateSamplers(c.currentAnim, &channel, &transformMap[channel.node->nodeIndex], c.currentTime);
	}

	// calculate local roll shift on wrists depending on delta yaw
	float targetYaw = -c.deltaYaw * HORIZONTAL_GUN_SWAY;
	glm::quat targetQ = glm::angleAxis(targetYaw, glm::vec3(0, -1, 0));
	c.currentSwayYaw = glm::slerp(c.currentSwayYaw, targetQ, deltaTime * 10.f);

	float targetPitch = -c.deltaPitch * VERTICAL_GUN_SWAY;
	glm::quat targetQP = glm::angleAxis(targetPitch, glm::vec3(0, 0, 1));
	c.currentSwayPitch = glm::slerp(c.currentSwayPitch, targetQP, deltaTime * 10.f);

	for (auto& node : { c.leftWrist, c.rightWrist }) {
		glm::mat4 parentWorldMat = node->parent ? node->parent->getMatrix() : glm::mat4(1.0f);
		glm::quat parentWorldRot = glm::quat_cast(parentWorldMat);
		glm::quat qRotYawLocal = glm::inverse(parentWorldRot) * c.currentSwayYaw * parentWorldRot;

		glm::quat qRotPitchLocal = glm::inverse(parentWorldRot) * c.currentSwayPitch * parentWorldRot;

		transformMap[node->nodeIndex].queuedQuatRotation = qRotYawLocal * qRotPitchLocal;
	}

	flushQueuedNodeTransforms(c, transformMap);
}

void FirstPersonAnimationStateMachine::updateAnimationState(FirstPersonAnimationController& c, float deltaTime) {
	if (c.shootTrigger || c.reloadTrigger) { // reset animation triggers
		c.shootTrigger = false;
		c.reloadTrigger = false;
	}
	if (c.previousState != c.currentState) {
		switch (c.currentState) {
		case PISTOL_EQUIP:
			c.currentAnim = c.animations[A_PISTOL_EQUIP];
			c.currentTime = c.currentAnim->start;
			break;
		case PISTOL_SHOOT:
			c.currentAnim = c.animations[A_PISTOL_SHOOT];
			c.currentTime = c.currentAnim->start;
			c.shootTrigger = true;
			break;
		case PISTOL_RELOAD:
			c.currentAnim = c.animations[A_PISTOL_RELOAD];
			c.currentTime = c.currentAnim->start;
			c.reloadTrigger = true;
			break;
		case PISTOL_IDLE:
			c.currentAnim = c.animations[A_PISTOL_IDLE];
			c.currentTime = c.currentAnim->start;
			break;
		case GRENADE_EQUIP:
			c.currentAnim = c.animations[A_GRENADE_EQUIP];
			c.currentTime = c.currentAnim->start;
			break;
		case GRENADE_IDLE:
			c.currentAnim = c.animations[A_GRENADE_IDLE];
			c.currentTime = c.currentAnim->start;
			break;
		case GRENADE_THROW:
			c.currentAnim = c.animations[A_GRENADE_THROW];
			c.currentTime = c.currentAnim->start;
			break;
		}
		c.previousState = c.currentState;
	}
}

// *********************** OBJECT *********************** // 

HFPArms::HFPArms(HSkinnedMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = FirstPersonAnimationController(meshIn);
}

void HFPArms::updateAnimation(float deltaTime) {
	FirstPersonAnimationStateMachine::updateAnimationState(controller, deltaTime);
	FirstPersonAnimationStateMachine::updateAnimatedNodeTransforms(controller, nodeTransforms, deltaTime);
	HAnimatedGameObject::updateAnimation(deltaTime); // updates joint matrix buffer
}