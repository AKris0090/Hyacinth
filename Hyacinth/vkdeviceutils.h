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
	VkDeviceAddress gpuAddress;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct DeviceContext {
    VkDevice* device;
    VmaAllocator* allocator;
	VkQueue* graphicsQueue;
    VkCommandBuffer* commandBuffer;
	VkFence* uploadFence;
};

namespace vkdeviceutils {
    void beginCommandBuffer(VkCommandBuffer& commandBuffer);
    void beginSingleTimeCommandBuffer(DeviceContext* ctx);
    void endSubmitCommandBuffer(DeviceContext* ctx);

    template<typename Func>
    void executeSingleTimeCommands(DeviceContext& ctx, Func&& f) {
        beginSingleTimeCommandBuffer(&ctx);
        f(*ctx.commandBuffer);
        endSubmitCommandBuffer(&ctx);
    }

    bool checkExtSupport(VkPhysicalDevice physicalDevice);
    SWChainSuppDetails getDetails(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    bool isSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR& surface);
    VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice& physicalDevice);
    VkRenderingInfo createRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);
    VulkanBuffer createBuffer(DeviceContext& ctx, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, std::string qual = "", VkDeviceSize alignment = 0);
    VulkanBuffer createBufferWithAlignment(DeviceContext& ctx, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, VkDeviceSize alignment = 0, std::string qual = "");
    void destroyBuffer(VmaAllocator& allocator, VulkanBuffer& buffer);
    void uploadToBuffer(DeviceContext& ctx, VulkanBuffer& buffer, size_t size, void* data);
}