#include "animation.h"

void Skin::loadSkins(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Skin>& skinsOut)
{
	skinsOut.resize(input->skins.size());

	for (size_t i = 0; i < input->skins.size(); i++)
	{
		tinygltf::Skin glTFSkin = input->skins[i];

		skinsOut[i].name = glTFSkin.name;
		skinsOut[i].skeletonRoot = nodeFromIndex(nodes, glTFSkin.skeleton);

		for (int jointIndex : glTFSkin.joints)
		{
			gltfNode* node = nodeFromIndex(nodes, jointIndex);
			if (node)
			{
				skinsOut[i].joints.push_back(node);
			}
		}

		if (glTFSkin.inverseBindMatrices > -1)
		{
			const tinygltf::Accessor& accessor = input->accessors[glTFSkin.inverseBindMatrices];
			const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
			skinsOut[i].inverseBindMatrices.resize(accessor.count);
			memcpy(skinsOut[i].inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
		}
	}
}


void Animation::loadAnimations(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Animation>& animsOut)
{
	animsOut.resize(input->animations.size());

	for (size_t i = 0; i < input->animations.size(); i++)
	{
		tinygltf::Animation glTFAnimation = input->animations[i];
		animsOut[i].name = glTFAnimation.name;

		animsOut[i].samplers.resize(glTFAnimation.samplers.size());
		for (size_t j = 0; j < glTFAnimation.samplers.size(); j++)
		{
			tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
			AnimationSampler& dstSampler = animsOut[i].samplers[j];
			dstSampler.interpolation = glTFSampler.interpolation;

			{
				const tinygltf::Accessor& accessor = input->accessors[glTFSampler.input];
				const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const float* buf = static_cast<const float*>(dataPtr);
				for (size_t index = 0; index < accessor.count; index++)
				{
					dstSampler.inputs.push_back(buf[index]);
				}
				for (auto input : animsOut[i].samplers[j].inputs)
				{
					if (input < animsOut[i].start)
					{
						animsOut[i].start = input;
					};
					if (input > animsOut[i].end)
					{
						animsOut[i].end = input;
					}
				}
			}

			{
				const tinygltf::Accessor& accessor = input->accessors[glTFSampler.output];
				const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				switch (accessor.type)
				{
				case TINYGLTF_TYPE_VEC3: {
					const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++)
					{
						dstSampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
					}
					break;
				}
				case TINYGLTF_TYPE_VEC4: {
					const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++)
					{
						dstSampler.outputsVec4.push_back(buf[index]);
					}
					break;
				}
				default: {
					std::cout << "unknown type" << std::endl;
					break;
				}
				}
			}
		}

		animsOut[i].channels.resize(glTFAnimation.channels.size());
		for (size_t j = 0; j < glTFAnimation.channels.size(); j++)
		{
			tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
			AnimationChannel& dstChannel = animsOut[i].channels[j];
			dstChannel.path = glTFChannel.target_path;
			dstChannel.samplerIndex = glTFChannel.sampler;
			dstChannel.node = nodeFromIndex(nodes, glTFChannel.target_node);
		}
	}
}

void AnimationStateMachine::flushQueuedNodeTransforms() {
	for (auto& node : { spine, spine003, upperArmL, upperArmR, spine005 }) {
		glm::quat finalPitch{ 1.f, 0.f, 0.f, 0.f };
		glm::quat finalYaw{ 1.f, 0.f, 0.f, 0.f };

		for (const auto f : node->queuedYawShifts) {
			glm::quat yawQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, -1, 0));
			finalYaw = yawQuat * finalYaw;
		}
		for (const auto f : node->queuedPitchShifts) {
			glm::quat pitchQuat = glm::angleAxis(glm::radians(f), glm::vec3(0, 0, 1));
			finalPitch = pitchQuat * finalPitch;
		}

		glm::quat finalGlobal = finalPitch * finalYaw;

		glm::mat4 parentWorldMat = node->parent ? getNodeMatrix(node->parent) : glm::mat4(1.0f);
		glm::quat parentWorldRot = glm::quat_cast(parentWorldMat);
		glm::quat qRotLocal = glm::inverse(parentWorldRot) * finalGlobal * parentWorldRot;

		node->queuedQuatRotation = qRotLocal;

		node->queuedPitchShifts.clear();
		node->queuedYawShifts.clear();
	}
	for (auto& node : { spine, spine003, upperArmL, upperArmR, spine005 }) {
		node->matComponents.rotation = node->queuedQuatRotation * node->matComponents.rotation;
	}
}

void AnimationStateMachine::updateFromPlayerState(float pitch, float yaw, float alpha, bool isMoving) {
	glm::quat trueAngleQuat = glm::slerp(prevBasisRotation, basisRotation, alpha);
	float bodyAngle = yawFromQuaternion(trueAngleQuat);

	spine->queuedYawShifts.push_back(bodyAngle);

	if (!isMoving) {
		glm::quat yawQuaternion = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));

		// get delta quaternion
		glm::quat delta = yawQuaternion * glm::inverse(trueAngleQuat);
		float deltaAngle = yawFromQuaternion(delta);

		if (deltaAngle < -120.f) {
			turn(yaw);
			turnState = NEEDS_TURN_LEFT;
			delta = yawQuaternion * glm::inverse(basisRotation);
			deltaAngle = yawFromQuaternion(delta);
		}
		else if (deltaAngle > 120.f) {
			turn(yaw);
			turnState = NEEDS_TURN_RIGHT;
			delta = yawQuaternion * glm::inverse(basisRotation);
			deltaAngle = yawFromQuaternion(delta);
		}

		spine003->queuedYawShifts.push_back(deltaAngle);
	}

	for (auto& node : { upperArmL, upperArmR, spine005 }) {
		node->queuedPitchShifts.push_back(pitch);
	}

	flushQueuedNodeTransforms(); // flush all at once so that rotations do not cause weird interactions with each other
}

void AnimationStateMachine::updateSamplers(Animation* animation, AnimationChannel* channel) {
	AnimationSampler& sampler = animation->samplers[channel->samplerIndex];
	for (size_t i = 0; i < sampler.inputs.size() - 1; i++)
	{
		if ((animation->currentTime >= sampler.inputs[i]) && (animation->currentTime <= sampler.inputs[i + 1]))
		{
			float a = (animation->currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
			if (channel->path == "translation")
			{
				channel->node->matComponents.position = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
			}
			if (channel->path == "rotation")
			{
				glm::quat q1;
				q1.x = sampler.outputsVec4[i].x;
				q1.y = sampler.outputsVec4[i].y;
				q1.z = sampler.outputsVec4[i].z;
				q1.w = sampler.outputsVec4[i].w;

				glm::quat q2;
				q2.x = sampler.outputsVec4[i + 1].x;
				q2.y = sampler.outputsVec4[i + 1].y;
				q2.z = sampler.outputsVec4[i + 1].z;
				q2.w = sampler.outputsVec4[i + 1].w;

				channel->node->matComponents.rotation = glm::normalize(glm::slerp(q1, q2, a));
			}
			if (channel->path == "scale")
			{
				channel->node->matComponents.scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
			}
		}
	}
	spine->matComponents.rotation = spineDefaultRot;
}

void AnimationStateMachine::updateUpperAnimation(float deltaTime) {
	for (auto& channel : currentUpperBodyAnim->channels)
	{
		if (!channel.node->upperBody) continue;
		updateSamplers(currentUpperBodyAnim, &channel);
	}
}

void AnimationStateMachine::updateLowerAnimation(float deltaTime) {
	for (auto& channel : currentLowerBodyAnim->channels)
	{
		if (!channel.node->lowerBody) continue;
		updateSamplers(currentLowerBodyAnim, &channel);
	}
}

void AnimationStateMachine::updateAnimationState(float deltaTime, float motionFB, float motionLR, float pitch, float yaw) {
	bool playerInMotion = false;
	if (motionFB != 0.f || motionLR != 0.f) {
		playerInMotion = true;
	}

	float alpha = 1.f;
	if (!playerInMotion) {
		if (turnState == NEEDS_TURN_LEFT) {
			currentLowerBodyAnim = leftTurnAnimation;
			leftTurnAnimation->currentTime = leftTurnAnimation->start;
			turnState = TURNING;
		}
		else if (turnState == NEEDS_TURN_RIGHT) {
			currentLowerBodyAnim = rightTurnAnimation;
			rightTurnAnimation->currentTime = rightTurnAnimation->start;
			turnState = TURNING;
		}
		else {
			if (motionState == MOVING) {
				currentLowerBodyAnim = idleAnimation;
				idleAnimation->currentTime = idleAnimation->start;
				motionState = STILL;
			}
		}
	}
	else {
		if (motionState != MOVING) {
			runningAnimation->currentTime = runningAnimation->start;
			currentLowerBodyAnim = runningAnimation;
			motionState = MOVING;
		}
		setNewBasis(yaw);
	}

	currentLowerBodyAnim->currentTime += deltaTime;
	if (currentLowerBodyAnim == rightTurnAnimation || currentLowerBodyAnim == leftTurnAnimation) {
		if (currentLowerBodyAnim->currentTime >= currentLowerBodyAnim->end) {
			currentLowerBodyAnim = idleAnimation;
			currentLowerBodyAnim->currentTime = 0.f;
			turnState = IDLE;
		}
	}
	currentLowerBodyAnim->currentTime = fmod(currentLowerBodyAnim->currentTime, currentLowerBodyAnim->end);
	currentUpperBodyAnim->currentTime += deltaTime;
	currentUpperBodyAnim->currentTime = fmod(currentUpperBodyAnim->currentTime, currentUpperBodyAnim->end);

	if (currentLowerBodyAnim == leftTurnAnimation || currentLowerBodyAnim == rightTurnAnimation) {
		alpha = currentLowerBodyAnim->currentTime / currentLowerBodyAnim->end;
	}

	updateUpperAnimation(deltaTime);
	updateLowerAnimation(deltaTime);

	updateFromPlayerState(pitch, yaw, alpha, playerInMotion);
}

