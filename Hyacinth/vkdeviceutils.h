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
    VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME
};

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
    static void beginCommandBuffer(VkCommandBuffer& commandBuffer) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    }

    static void endSubmitCommandBuffer(DeviceContext* ctx) {
        VK_CHECK(vkEndCommandBuffer(*ctx->commandBuffer));

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = ctx->commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

        VK_CHECK(vkQueueSubmit(*ctx->graphicsQueue, 1, &submitInfo, *ctx->uploadFence));
        vkWaitForFences(*ctx->device, 1, ctx->uploadFence, true, 9999999999);
        vkResetFences(*ctx->device, 1, ctx->uploadFence);
	}

    template<typename Func>
    void executeSingleTimeCommands(DeviceContext& ctx, Func&& f) {
        vkResetCommandBuffer(*ctx.commandBuffer, 0);
        beginCommandBuffer(*ctx.commandBuffer);
        f(*ctx.commandBuffer);
        endSubmitCommandBuffer(&ctx);
    }

    static bool checkExtSupport(VkPhysicalDevice physicalDevice) {
        uint32_t numExts;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExts, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(numExts);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExts, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExts.begin(), deviceExts.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    static SWChainSuppDetails getDetails(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
        SWChainSuppDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
        QueueFamilyIndices indices;

        uint32_t numQueueFamilies = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 prSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &prSupport);

            if (prSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    static bool isSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR& surface) {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        bool extsSupported = checkExtSupport(physicalDevice);

        bool isSWChainAdequate = false;
        if (extsSupported) {
            SWChainSuppDetails SWChainSupp = getDetails(physicalDevice, surface);
            isSWChainAdequate = !SWChainSupp.formats.empty() && !SWChainSupp.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
        return (indices.isComplete() && extsSupported && isSWChainAdequate);
    }

    static VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice& physicalDevice) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSampleCountFlags counts =
            props.limits.framebufferColorSampleCounts &
            props.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
        if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
        if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
        if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;

        return VK_SAMPLE_COUNT_1_BIT;
    }

    static VkRenderingInfo createRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment) {
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, renderArea };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = colorAttachment;
        renderingInfo.pDepthAttachment = depthAttachment;
        renderingInfo.pStencilAttachment = nullptr;

        return renderingInfo;
	}

    static VulkanBuffer createBuffer(VkDevice& device, VmaAllocator& allocator, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags) {
		VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = memUsage;
        allocInfo.flags = vmaFlags;

        VulkanBuffer buffer{};

        VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));

        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer.buffer;
        buffer.gpuAddress = vkGetBufferDeviceAddress(device, &addressInfo);
        
        return buffer;
    }

    static void destroyBuffer(VmaAllocator& allocator, VulkanBuffer& buffer) {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    }

    static void uploadToBuffer(DeviceContext& ctx, VulkanBuffer& buffer, size_t size, void* data) {
        VulkanBuffer stagingBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        memcpy(stagingBuffer.info.pMappedData, data, size);

        vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
            VkBufferCopy copyRegion{};
            copyRegion.size = size;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, 1, &copyRegion);
        });

        vkdeviceutils::destroyBuffer(*ctx.allocator, stagingBuffer);
    }
}