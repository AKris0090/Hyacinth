#pragma once

#include "vkswapchainutils.h"
#include "vkdebugutils.h"

#include <string>
#include <set>
#include <optional>

const std::vector<const char*> deviceExts = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    std::optional<uint32_t> presentFamily;

    bool isComplete() const { return (graphicsFamily.has_value() && presentFamily.has_value()); }
};

namespace vkdeviceutils {
    static void beginCommandBuffer(VkCommandBuffer& commandBuffer) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
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
}