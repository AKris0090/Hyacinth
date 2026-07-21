#include "animation.h"

// THIRD PERSON //////////////////////////////////////////
//////////////////////////////////////////////////////////

// void ThirdPersonAnimationStateMachine::flushQueuedNodeTransforms(ThirdPersonAnimationController& c) {
// 	for (auto& node : { c.spine, c.spine003, c.upperArmL, c.upperArmR, c.spine005 }) {
// 		glm::quat finalPitch{ 1.f, 0.f, 0.f, 0.f };
// 		glm::quat finalYaw{ 1.f, 0.f, 0.f, 0.f };
// 
// 		for (const auto f : node->queuedYawShifts) {
// 			glm::quat yawQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, -1, 0));
// 			finalYaw = yawQuat * finalYaw;
// 		}
// 		for (const auto f : node->queuedPitchShifts) {
// 			glm::quat pitchQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, 0, 1));
// 			finalPitch = pitchQuat * finalPitch;
// 		}
// 
// 		glm::quat finalGlobal = finalPitch * finalYaw;
// 
// 		glm::mat4 parentWorldMat = node->parent ? getNodeMatrix(node->parent) : glm::mat4(1.0f);
// 		glm::quat parentWorldRot = glm::quat_cast(parentWorldMat);
// 		glm::quat qRotLocal = glm::inverse(parentWorldRot) * finalGlobal * parentWorldRot;
// 
// 		node->queuedQuatRotation = qRotLocal;
// 
// 		node->queuedPitchShifts.clear();
// 		node->queuedYawShifts.clear();
// 	}
// 	for (auto& node : { c.spine, c.spine003, c.upperArmL, c.upperArmR, c.spine005 }) {
// 		node->localTransform.rotation = node->queuedQuatRotation * node->localTransform.rotation;
// 	}
// }
// 
// void ThirdPersonAnimationStateMachine::updateFromPlayerState(ThirdPersonAnimationController& c, float pitch, float yaw, float alpha, bool isMoving) {
// 	glm::quat trueAngleQuat = glm::slerp(c.prevBasisRotation, c.basisRotation, alpha);
// 	float bodyAngle = yawFromQuaternion(trueAngleQuat);
// 	c.spine->queuedYawShifts.push_back(bodyAngle);
// 
// 	if (!isMoving) {
// 		glm::quat yawQuaternion = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));	
// 		glm::quat delta = yawQuaternion * glm::inverse(trueAngleQuat);
// 		float deltaAngle = yawFromQuaternion(delta);
// 		c.spine003->queuedYawShifts.push_back(deltaAngle);
// 	}
// 
// 	for (auto& node : { c.upperArmL, c.upperArmR, c.spine005 }) {
// 		node->queuedPitchShifts.push_back(pitch);
// 	}
// 
// 	flushQueuedNodeTransforms(c); // flush all at once so that rotations do not cause weird interactions with each other
// }
// 
// void ThirdPersonAnimationStateMachine::updateUpperAnimation(ThirdPersonAnimationController& c) {
// 	for (auto& channel : c.currentUpperBodyAnim->channels)
// 	{
// 		if (!channel.node->upperBody) continue;
// 		updateSamplers(c.currentUpperBodyAnim, &channel, &channel.node->localTransform, c.currentUpperTime);
// 	}
// }
// 
// void ThirdPersonAnimationStateMachine::updateLowerAnimation(ThirdPersonAnimationController& c) {
// 	for (auto& channel : c.currentLowerBodyAnim->channels)
// 	{
// 		if (!channel.node->lowerBody) continue;
// 		updateSamplers(c.currentLowerBodyAnim, &channel, &channel.node->localTransform, c.currentLowerTime);
// 	}
// }
// 
// void ThirdPersonAnimationStateMachine::updatePreviousWholeBodyAnimation(ThirdPersonAnimationController& c) {
// 	for (auto& channel : c.previousAnimation->channels)
// 	{
// 		Transform& t = c.previousAnimationTransforms[channel.node->index];
// 		updateSamplers(c.previousAnimation, &channel, &t, c.currentUpperTime);
// 	}
// }
// 
// void ThirdPersonAnimationStateMachine::transitionToNewAnimation(ThirdPersonAnimationController& c, Animation* current, Animation* next) {
// 	c.previousAnimation = current;
// 	c.transitioning = true;
// 	c.fadeTimer = 0.f;
// 	c.currentLowerBodyAnim = next;
// 	c.currentUpperBodyAnim = next;
// 	c.currentUpperTime = c.currentLowerTime = next->start;
// }
// 
// void ThirdPersonAnimationStateMachine::lerpPreviousCurrentAnimations(ThirdPersonAnimationController& c) {
// 	float alpha = c.fadeTimer / c.fadeLength;
// 	for (auto& channel : c.currentLowerBodyAnim->channels)
// 	{
// 		Transform& t = c.previousAnimationTransforms[channel.node->index];
// 		channel.node->localTransform = t.lerpToNoSet(channel.node->localTransform, alpha);
// 	}
// }
// 
// void ThirdPersonAnimationStateMachine::updateAnimationState(ThirdPersonAnimationController& c, float deltaTime, float motionFB, float motionLR, float pitch, float yaw) {
// 	bool playerInMotion = false;
// 	if (motionFB != 0.f || motionLR != 0.f) {
// 		playerInMotion = true;
// 	}
// 
// 	if (!playerInMotion) {
// 		if (c.turnState == IDLE || c.turnState == TURNING) {
// 			glm::quat yawQuaternion = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
// 
// 			// get delta quaternion
// 			glm::quat delta = yawQuaternion * glm::inverse(c.basisRotation);
// 			float deltaAngle = yawFromQuaternion(delta);
// 
// 			if (deltaAngle < -90.f) {
// 				turn(c, yaw);
// 				c.currentLowerBodyAnim = c.animations[A_TP_LEFT_TURN];
// 				c.currentLowerTime = c.animations[A_TP_LEFT_TURN]->start;
// 				c.turnState = TURNING;
// 			}
// 			else if (deltaAngle > 90.f) {
// 				turn(c, yaw);
// 				c.currentLowerBodyAnim = c.animations[A_TP_RIGHT_TURN];
// 				c.currentLowerTime = c.animations[A_TP_RIGHT_TURN]->start;
// 				c.turnState = TURNING;
// 			}
// 			else if (c.motionState == MOVING) {
// 				transitionToNewAnimation(c, c.animations[A_TP_RUNNING], c.animations[A_TP_IDLE]); // TODO: once implemented other motion, update this
// 				c.motionState = STILL;
// 			}
// 		}
// 	}
// 	else {
// 		if (c.motionState != MOVING) {
// 			transitionToNewAnimation(c, c.animations[A_TP_IDLE], c.animations[A_TP_RUNNING]); // TODO: once implemented other motion, update this
// 			c.motionState = MOVING;
// 		}
// 		setNewBasis(c, yaw);
// 	}
// 
// 	c.currentLowerTime += deltaTime;
// 	if (c.turnState == TURN_ANIM_STATE::TURNING) {
// 		if (c.currentLowerTime >= c.currentLowerBodyAnim->end) {
// 			c.currentLowerBodyAnim = c.animations[A_TP_IDLE];
// 			c.currentLowerTime = c.currentLowerBodyAnim->start;
// 			c.turnState = IDLE;
// 		}
// 	}
// 	c.currentLowerTime = fmod(c.currentLowerTime, c.currentLowerBodyAnim->end);
// 
// 	c.currentUpperTime += deltaTime;
// 	c.currentUpperTime = fmod(c.currentUpperTime, c.currentUpperBodyAnim->end);
// 
// 	float alpha = 1.f;
// 	if (c.turnState == TURN_ANIM_STATE::TURNING) {
// 		alpha = c.currentLowerTime / c.currentLowerBodyAnim->end;
// 	}
// 
// 	updateUpperAnimation(c);
// 	updateLowerAnimation(c);
// 
// 	if (c.transitioning) {
// 		c.fadeTimer += deltaTime;
// 		if (c.fadeTimer > c.fadeLength) {
// 			c.transitioning = false;
// 		}
// 		else {
// 			c.previousTime += deltaTime;
// 			c.previousTime = fmod(c.previousTime, c.previousAnimation->end);
// 			updatePreviousWholeBodyAnimation(c);
// 
// 			// lerp between animation channel nodes and previous animation transforms
// 			lerpPreviousCurrentAnimations(c);
// 		}
// 	}
// 
// 	updateFromPlayerState(c, pitch, yaw, alpha, playerInMotion);
// }
// 
// 
// // PISTOL ////////////////////////////////////////////////
// //////////////////////////////////////////////////////////
// 
// void PistolAnimationStateMachine::updateAnimation(PistolAnimationController& c, float deltaTime) {
// 	if (c.queueReload) {
// 		c.queueReload = false;
// 		c.currentAnim = c.reloadAnimation;
// 		c.currentTime = c.reloadAnimation->start;
// 	} else if (c.queueShoot) {
// 		c.queueShoot = false;
// 		c.currentAnim = c.shootAnimation;
// 		c.currentTime = c.currentAnim->start;
// 	}
// 	c.currentTime += deltaTime;
// 
// 	if (c.currentTime > c.currentAnim->end) {
// 		c.currentAnim = c.idleAnimation;
// 		c.currentTime = c.idleAnimation->start;
// 	}
// 
// 	c.currentTime = fmod(c.currentTime, c.currentAnim->end);
// 
// 	for (auto& channel : c.currentAnim->channels)
// 	{
// 		updateSamplers(c.currentAnim, &channel, &channel.node->localTransform, c.currentTime);
// 	}
// }
// 
// void PistolAnimationStateMachine::updateAnimationState(PistolAnimationController& c, float deltaTime) {
// 	updateAnimation(c, deltaTime);
// }