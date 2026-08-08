#include "pch.h"
#include "tp_character.h"

// *********************** CONTROLLER *********************** //
ThirdPersonAnimationController::ThirdPersonAnimationController(LightMesh* mesh) {
	upperArmL = mesh->getNodeByName("upper_arm.L");    // left arm (pitch) controller
	upperArmR = mesh->getNodeByName("upper_arm.R");    // right arm (pitch) controller
	spine005 = mesh->getNodeByName("spine.005");     // head neck (pitch) controller
	spine007 = mesh->getNodeByName("spine.007");     // lower body yaw controller
	spine003 = mesh->getNodeByName("spine.003");     // upper body yaw controller
	spine = mesh->getNodeByName("spine");

	animations[A_TP_IDLE] = &mesh->animations[0];
	animations[A_TP_RUNNING] = &mesh->animations[1];
	animations[A_TP_LEFT_TURN] = &mesh->animations[2];
	animations[A_TP_RIGHT_TURN] = &mesh->animations[3];

	currentLowerBodyAnim = currentUpperBodyAnim = previousAnimation = animations[A_TP_IDLE];

	isUpperFlag = std::vector<bool>(mesh->numNodes, false);
	isLowerFlag = std::vector<bool>(mesh->numNodes, false);

	isLowerFlag[spine->nodeIndex] = true;

	for (const auto& jointNode : mesh->skin.joints) {
		if (mesh->isParentOf(jointNode, spine003)) isUpperFlag[jointNode->nodeIndex] = true;
		if (mesh->isParentOf(jointNode, spine007)) isLowerFlag[jointNode->nodeIndex] = true;
	}
};

void ThirdPersonAnimationController::updateAnimParams(Entity* entity) {
	pitch = entity->transform.pitch;
	yaw = entity->transform.yaw;
	isMoving = entity->isMoving;
}

// *********************** ANIM STATE MACHINE *********************** //

static void setNewBasis(ThirdPersonAnimationController& c, float basis) {
	basis = fmodf(basis, 360);
	c.basisRotation = glm::angleAxis(glm::radians(basis), glm::vec3(0, 1, 0));
}

static void turn(ThirdPersonAnimationController& c, float absolute) {
	c.prevBasisRotation = c.basisRotation;
	setNewBasis(c, absolute);
}

static void stageTurnAnim(ThirdPersonAnimationController& c, bool leftRight) {
	c.turnState = leftRight ? NEEDS_TURN_LEFT : NEEDS_TURN_RIGHT;
}

void ThirdPersonAnimationStateMachine::flushQueuedNodeTransforms(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	for (auto& node : { c.spine, c.spine003, c.upperArmL, c.upperArmR, c.spine005 }) {
		glm::quat finalPitch{ 1.f, 0.f, 0.f, 0.f };
		glm::quat finalYaw{ 1.f, 0.f, 0.f, 0.f };
	
		for (const auto f : transformMap[node->nodeIndex].queuedYawShifts) {
			glm::quat yawQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, -1, 0));
			finalYaw = yawQuat * finalYaw;
		}
		for (const auto f : transformMap[node->nodeIndex].queuedPitchShifts) {
			glm::quat pitchQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, 0, 1));
			finalPitch = pitchQuat * finalPitch;
		}
	
		glm::quat finalGlobal = finalPitch * finalYaw;
	
		glm::mat4 parentWorldMat = transformMap[node->parent->nodeIndex].getMatrix();
		glm::quat parentWorldRot = glm::quat_cast(parentWorldMat);
		glm::quat qRotLocal = glm::inverse(parentWorldRot) * finalGlobal * parentWorldRot;
	
		transformMap[node->nodeIndex].queuedQuatRotation = qRotLocal;
	
		transformMap[node->nodeIndex].queuedPitchShifts.clear();
		transformMap[node->nodeIndex].queuedYawShifts.clear();
	}

	for (auto& node : { c.spine, c.spine003, c.upperArmL, c.upperArmR, c.spine005 }) {
		transformMap[node->nodeIndex].rotation = transformMap[node->nodeIndex].queuedQuatRotation * transformMap[node->nodeIndex].rotation;
	}
}

void ThirdPersonAnimationStateMachine::updateFromPlayerState(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	glm::quat trueAngleQuat = glm::slerp(c.prevBasisRotation, c.basisRotation, c.alpha);
	float bodyAngle = yawFromQuaternion(trueAngleQuat);
	transformMap[c.spine->nodeIndex].queuedYawShifts.push_back(bodyAngle);
	
	if (!c.isMoving) {
		glm::quat yawQuaternion = glm::angleAxis(glm::radians(c.yaw), glm::vec3(0, 1, 0));	
		glm::quat delta = yawQuaternion * glm::inverse(trueAngleQuat);
		float deltaAngle = yawFromQuaternion(delta);
		transformMap[c.spine003->nodeIndex].queuedYawShifts.push_back(deltaAngle);
	}
	
	for (auto& node : { c.upperArmL, c.upperArmR, c.spine005 }) {
		transformMap[node->nodeIndex].queuedPitchShifts.push_back(c.pitch);
	}
	
	flushQueuedNodeTransforms(c, transformMap); // flush all at once so that rotations do not cause weird interactions with each other
}

void ThirdPersonAnimationStateMachine::updateUpperAnimation(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	for (auto& channel : c.currentUpperBodyAnim->channels) {
		if (!c.isUpperFlag[channel.node->nodeIndex]) continue;
		AnimControllerBase::updateSamplers(c.currentUpperBodyAnim, &channel, &transformMap[channel.node->nodeIndex], c.currentUpperTime);
	}
}

void ThirdPersonAnimationStateMachine::updateLowerAnimation(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	for (auto& channel : c.currentLowerBodyAnim->channels) {
		if (!c.isLowerFlag[channel.node->nodeIndex]) continue;
		AnimControllerBase::updateSamplers(c.currentLowerBodyAnim, &channel, &transformMap[channel.node->nodeIndex], c.currentLowerTime);
	}
}

void ThirdPersonAnimationStateMachine::updatePreviousWholeBodyAnimation(ThirdPersonAnimationController& c) {
	for (auto& channel : c.previousAnimation->channels) {
		Transform* t = &c.previousAnimationTransforms[channel.node->nodeIndex];
		AnimControllerBase::updateSamplers(c.previousAnimation, &channel, t, c.previousTime);
	}
}

void ThirdPersonAnimationStateMachine::transitionToNewAnimation(ThirdPersonAnimationController& c, LightAnimation* current, LightAnimation* next) {
	c.previousAnimation = current;
	c.previousTime = c.currentLowerTime;
	c.transitioning = true;
	c.fadeTimer = 0.f;
	c.currentLowerBodyAnim = next;
	c.currentUpperBodyAnim = next;
	c.currentUpperTime = c.currentLowerTime = next->start;
}

void ThirdPersonAnimationStateMachine::lerpPreviousCurrentAnimations(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap) {
	float alpha = c.fadeTimer / c.fadeLength;
	for (auto& [id, nodeT] : c.previousAnimationTransforms)
	{
		Transform lerpedT = nodeT.lerpToNoSet(transformMap[id], alpha);
		transformMap[id].copy(lerpedT);
	}
}

void ThirdPersonAnimationStateMachine::updateAnimationState(ThirdPersonAnimationController& c, std::unordered_map<uint32_t, Transform>& transformMap, float deltaTime) {
	if (!c.isMoving) {
		if (c.turnState == IDLE || c.turnState == TURNING) {
			glm::quat yawQuaternion = glm::angleAxis(glm::radians(c.yaw), glm::vec3(0, 1, 0));
	
			// get delta quaternion
			glm::quat delta = yawQuaternion * glm::inverse(c.basisRotation);
			float deltaAngle = yawFromQuaternion(delta);
	
			if (deltaAngle < -90.f) {
				turn(c, c.yaw);
				c.currentLowerBodyAnim = c.animations[A_TP_LEFT_TURN];
				c.currentLowerTime = c.animations[A_TP_LEFT_TURN]->start;
				c.turnState = TURNING;
			}
			else if (deltaAngle > 90.f) {
				turn(c, c.yaw);
				c.currentLowerBodyAnim = c.animations[A_TP_RIGHT_TURN];
				c.currentLowerTime = c.animations[A_TP_RIGHT_TURN]->start;
				c.turnState = TURNING;
			}
			else if (c.motionState == MOVING) {
				transitionToNewAnimation(c, c.animations[A_TP_RUNNING], c.animations[A_TP_IDLE]); // TODO: once implemented other motion, update this
				c.motionState = STILL;
			}
		}
	}
	else {
		if (c.motionState != MOVING) {
			transitionToNewAnimation(c, c.animations[A_TP_IDLE], c.animations[A_TP_RUNNING]); // TODO: once implemented other motion, update this
			c.motionState = MOVING;
		}
		setNewBasis(c, c.yaw);
	}
	
	c.currentLowerTime += deltaTime;
	if (c.turnState == TURN_ANIM_STATE::TURNING) {
		if (c.currentLowerTime >= c.currentLowerBodyAnim->end) {
			c.currentLowerBodyAnim = c.animations[A_TP_IDLE];
			c.currentLowerTime = c.currentLowerBodyAnim->start;
			c.turnState = IDLE;
		}
	}
	c.currentLowerTime = fmod(c.currentLowerTime, c.currentLowerBodyAnim->end);
	
	c.currentUpperTime += deltaTime;
	c.currentUpperTime = fmod(c.currentUpperTime, c.currentUpperBodyAnim->end);
	
	c.alpha = 1.f;
	if (c.turnState == TURN_ANIM_STATE::TURNING) {
		c.alpha = c.currentLowerTime / c.currentLowerBodyAnim->end;
	}

	updateUpperAnimation(c, transformMap);
	updateLowerAnimation(c, transformMap);

	if (c.transitioning) {
		c.fadeTimer += deltaTime;
		if (c.fadeTimer > c.fadeLength) {
			c.transitioning = false;
		}
		else {
			c.previousTime += deltaTime;
			c.previousTime = fmod(c.previousTime, c.previousAnimation->end);
			updatePreviousWholeBodyAnimation(c);
	
			// lerp between animation channel nodes and previous animation transforms
			lerpPreviousCurrentAnimations(c, transformMap);
		}
	}
	
	updateFromPlayerState(c, transformMap);
}

// *********************** OBJECT *********************** //

HTPCharacter::HTPCharacter(LightMesh* meshIn) : HAnimatedGameObject(meshIn) {
	controller = ThirdPersonAnimationController(meshIn);

	for (const auto& n : mesh->parentNodes) {
		addNodeTransform(n, controller.previousAnimationTransforms);
	}
}

void HTPCharacter::updateAnimation(float deltaTime, bool updateMatrices) {
	ThirdPersonAnimationStateMachine::updateAnimationState(controller, nodeTransforms, deltaTime); // includes updating node transforms
	if (updateMatrices) HAnimatedGameObject::updateAnimation(deltaTime); // updates joint matrix buffer
}