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