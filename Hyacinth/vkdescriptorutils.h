#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stages);
	void clear();
	VkDescriptorSetLayout buildLayout(VkDevice& layout, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void initPool(VkDevice& device, uint32_t maxSets, std::vector<PoolSizeRatio> poolRatios);
    void clearDescriptors(VkDevice& device);
    void destroyPool(VkDevice& device);

    VkDescriptorSet allocate(VkDevice& device, VkDescriptorSetLayout layout);
};