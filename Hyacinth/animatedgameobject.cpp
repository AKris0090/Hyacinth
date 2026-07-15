#include "animatedgameobject.h"

void AnimControllerBase::updateSamplers(HAnimation* animation, HAnimChannel* channel, Transform* t, float currentTime) {
	HAnimSampler& sampler = animation->samplers[channel->samplerIndex];
	for (size_t i = 0; i < sampler.inputs.size() - 1; i++)
	{
		if ((currentTime >= sampler.inputs[i]) && (currentTime <= sampler.inputs[i + 1]))
		{
			float a = (currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
			if (channel->path == "translation")
			{
				t->position = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
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

				t->rotation = glm::normalize(glm::slerp(q1, q2, a));
			}
			if (channel->path == "scale")
			{
				t->scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
			}
		}
	}
}

glm::mat4 HAnimatedGameObject::getStackedNodeMatrix(HSkinnedMeshNode* node) {
	glm::mat4 nodeMatrix = nodeTransforms[node->nodeIndex].getMatrix();

	if (parentObject && parentMeshNode) {
		nodeMatrix = parentObject->getStackedNodeMatrix(parentMeshNode) * nodeMatrix;
	}
	return nodeMatrix;
}

void HAnimatedGameObject::updateJoints() {
	glm::mat4				inverseTransform = glm::inverse(nodeTransforms[mesh->meshOwnerNode->nodeIndex].getMatrix());
	size_t					numJoints = (uint32_t) mesh->skin.joints.size();
	std::vector<glm::mat4>	finalJointMatrices(numJoints);
	for (size_t i = 0; i < numJoints; i++)
	{
		glm::mat4 jointMatrix = getStackedNodeMatrix(mesh->skin.joints[i]);
		finalJointMatrices[i] = inverseTransform * (jointMatrix * mesh->skin.inverseBindMatrices[i]);
	}

	memcpy(jointMatrixBuffer.pMappedData, finalJointMatrices.data(), finalJointMatrices.size() * sizeof(glm::mat4));
}

void HAnimatedGameObject::updateAnimation(float deltaTime) {
	updateJoints();
}

void HAnimatedGameObject::hookUpTransformParents(HSkinnedMeshNode* n, std::unordered_map<uint32_t, Transform>& nodeTransforms) {
	if (n->parent) {
		Transform* parentTransform = &nodeTransforms[n->parent->nodeIndex];
		nodeTransforms[n->nodeIndex].parent = parentTransform;
	}

	for (const auto& nc : n->children) {
		hookUpTransformParents(nc, nodeTransforms);
	}
}

void HAnimatedGameObject::addNodeTransform(HSkinnedMeshNode* n, std::unordered_map<uint32_t, Transform>& nodeTransforms) {
	nodeTransforms[n->nodeIndex] = n->transform; // need base pose

	for (const auto& nc : n->children) {
		addNodeTransform(nc, nodeTransforms);
	}
}

HAnimatedGameObject::HAnimatedGameObject(HSkinnedMesh* meshRef) {
	mesh = meshRef;

	for (const auto& n : mesh->parentNodes) {
		addNodeTransform(n, nodeTransforms);
	}

	for (const auto& n : mesh->parentNodes) {
		hookUpTransformParents(n, nodeTransforms);
	}

	jointMatrixBuffer = vkdeviceutils::createBuffer(mesh->jointMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_joint_matrix_buffer");
}

void HAnimatedGameObject::destroy() {
	vkdeviceutils::destroyBuffer(jointMatrixBuffer);
}

void HAnimatedGameObject::setParentObject(HAnimatedGameObject* aobject, HSkinnedMeshNode* childNode, HSkinnedMeshNode* parentNode) {
	parentObject = aobject;
	parentMeshNode = parentNode;
}