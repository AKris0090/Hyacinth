#pragma once

#include "vkswapchainutils.h"
#include "vkdebugutils.h"
#include "vk_mem_alloc.h"

#include <string>
#include <set>
#include <optional>

const std::vector<const char*> deviceExts = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
    VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
};

constexpr int MAX_FRAMES_IN_FLIGHT = 3;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    std::optional<uint32_t> presentFamily;

    bool isComplete() const { return (graphicsFamily.has_value() && presentFamily.has_value()); }
};

struct VulkanBuffer {
    VkBuffer buffer;
	void* pMappedData;
	VkDeviceAddress gpuAddress;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

namespace vkdeviceutils {
	extern VkDevice device;
	extern VmaAllocator allocator;
    extern VkQueue graphicsQueue;
	extern VkCommandBuffer commandBuffer;
	extern VkFence uploadFence;

    void setDevice(VkDevice& dev);
    void setAllocator(VmaAllocator& alloc);
    void setGraphicsQueue(VkQueue& queue);
    void setSingleTimeCommandBuffer(VkCommandBuffer& cmd);
    void setSingleTimeUploadFence(VkFence& fence);

    void beginCommandBuffer(VkCommandBuffer& commandBuffer);
    void beginSingleTimeCommandBuffer();
    void endSubmitCommandBuffer();

    template<typename Func>
    void executeSingleTimeCommands(Func&& f) {
        beginSingleTimeCommandBuffer();
        f(commandBuffer);
        endSubmitCommandBuffer();
    }

    bool checkExtSupport(VkPhysicalDevice physicalDevice);
    SWChainSuppDetails getDetails(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    bool isSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR& surface);
    VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice& physicalDevice);
    VkRenderingInfo createDepthRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* depthAttachment);
    VkRenderingInfo createRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);
    VulkanBuffer createBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, std::string qual = "", VkDeviceSize alignment = 0);
    VulkanBuffer createBufferWithAlignment(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, VkDeviceSize alignment = 0, std::string qual = "");
    void destroyBuffer(VulkanBuffer& buffer);
    void uploadToBuffer(VulkanBuffer& buffer, size_t size, void* data);
	void stageAndUploadBuffers(VkDeviceSize* pSizes, void** ppData, VulkanBuffer* pBuffers, uint32_t count);
}