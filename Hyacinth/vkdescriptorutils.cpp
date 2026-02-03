#include "vkdescriptorutils.h"
#include "vkdebugutils.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding newBind{};
    newBind.binding = binding;
    newBind.descriptorCount = count;
    newBind.descriptorType = type;
    newBind.stageFlags = stages;

    bindings.push_back(newBind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::buildLayout(void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(vkdeviceutils::device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::initPool(uint32_t maxSets, std::vector<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
            });
    }

    VkDescriptorPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(vkdeviceutils::device, &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors()
{
    vkResetDescriptorPool(vkdeviceutils::device, pool, 0);
}

void DescriptorAllocator::destroyPool()
{
    vkDestroyDescriptorPool(vkdeviceutils::device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(vkdeviceutils::device, &allocInfo, &set));

    return set;
}

namespace vkdescriptorutils {
    std::vector<VkWriteDescriptorSet> queuedWrites;
    std::vector<VkDescriptorImageInfo*> imageInfos;
    std::vector<VkDescriptorBufferInfo*> bufferInfos;
	std::vector<VkWriteDescriptorSetAccelerationStructureKHR*> accelStructureInfos;
}

void vkdescriptorutils::queueWriteAccelStructure(VkDescriptorSet& descriptorSet, uint32_t binding, int numStructures, VkAccelerationStructureKHR* pAccelStructures) {
    VkWriteDescriptorSetAccelerationStructureKHR* asInfo = new VkWriteDescriptorSetAccelerationStructureKHR{};
    asInfo->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo->accelerationStructureCount = numStructures;
    asInfo->pAccelerationStructures = pAccelStructures;

    VkWriteDescriptorSet accelWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    accelWrite.pNext = asInfo;
    accelWrite.dstSet = descriptorSet;
    accelWrite.dstBinding = 0;
    accelWrite.dstArrayElement = 0;
    accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelWrite.descriptorCount = 1;

    queuedWrites.push_back(accelWrite);
}

void vkdescriptorutils::queueWriteImage(VkDescriptorSet& descriptorSet, uint32_t binding, uint32_t arrayLayer, VkDescriptorType type, VulkanImage& image, VkImageLayout layout) {
    VkDescriptorImageInfo* imageInfo = new VkDescriptorImageInfo{};
    imageInfo->imageLayout = layout;
    imageInfo->imageView = image.imageView;
    if (type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        imageInfo->sampler = image.imageSampler;
    }
	imageInfos.push_back(imageInfo);

    VkWriteDescriptorSet imageWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    imageWrite.dstSet = descriptorSet;
    imageWrite.dstBinding = binding;
    imageWrite.dstArrayElement = arrayLayer;
    imageWrite.descriptorType = type;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = imageInfo;

	queuedWrites.push_back(imageWrite);
}

void vkdescriptorutils::queueWriteBuffer(VkDescriptorSet& descriptorSet, uint32_t binding, size_t size, VkDescriptorType type, VulkanBuffer& buffer) {
    VkDescriptorBufferInfo* bufferInfo = new VkDescriptorBufferInfo{};
    bufferInfo->buffer = buffer.buffer;
    bufferInfo->offset = 0;
    bufferInfo->range = size;

    VkWriteDescriptorSet bufferWrite{};
    bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    bufferWrite.dstSet = descriptorSet;
    bufferWrite.dstBinding = binding;
    bufferWrite.dstArrayElement = 0;
    bufferWrite.descriptorType = type;
    bufferWrite.descriptorCount = 1;
    bufferWrite.pBufferInfo = bufferInfo;

    queuedWrites.push_back(bufferWrite);
}

void vkdescriptorutils::flushDescriptorWrites() {
    vkUpdateDescriptorSets(vkdeviceutils::device, (uint32_t)vkdescriptorutils::queuedWrites.size(), vkdescriptorutils::queuedWrites.data(), 0, nullptr);
    vkdescriptorutils::queuedWrites.clear();
    for (auto* info : imageInfos) {
        delete info;
	}
    for (auto* info : bufferInfos) {
        delete info;
    }
    for (auto* info : accelStructureInfos) {
        delete info;
	}
	bufferInfos.clear();
	imageInfos.clear();
	accelStructureInfos.clear();
}