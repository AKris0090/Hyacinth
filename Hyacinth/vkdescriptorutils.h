#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vkimageutils.h"

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stages);
	void clear();
	VkDescriptorSetLayout buildLayout(void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void initPool(uint32_t maxSets, std::vector<PoolSizeRatio> poolRatios);
    void clearDescriptors();
    void destroyPool();

    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
};

namespace vkdescriptorutils {
	extern std::vector<VkWriteDescriptorSet> queuedWrites;
    extern std::vector<VkDescriptorImageInfo*> imageInfos;
    extern std::vector<VkDescriptorBufferInfo*> bufferInfos;

	void queueWriteImage(VkDescriptorSet& descriptorSet, uint32_t binding, uint32_t arrayLayer, VkDescriptorType type, VulkanImage& image, VkImageLayout layout);
    void queueWriteBuffer(VkDescriptorSet& descriptorSet, uint32_t binding, size_t size, VkDescriptorType type, VulkanBuffer& buffer);

	void flushDescriptorWrites();
}