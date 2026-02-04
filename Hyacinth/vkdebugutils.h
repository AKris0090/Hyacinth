#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include <iostream>

#include <filesystem>
#include <windows.h>

// macro to check vk function results
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = (x);                                             \
        if (err != VK_SUCCESS) {                                        \
            std::cout << "[VK_ERR] " + x << std::endl;                  \
            throw std::runtime_error("Vulkan error: " #x);              \
        }                                                               \
    } while (0)

#define VK_LABEL(cmd, name) \
    if (vkdebugutils::BeginDebugLabel) { \
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT }; \
        label.pLabelName = name; \
        vkdebugutils::BeginDebugLabel(cmd, &label); \
    }

#define VK_LABEL_END(cmd) \
    if (vkdebugutils::EndDebugLabel) \
        vkdebugutils::EndDebugLabel(cmd);

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

namespace vkdebugutils {
    extern PFN_vkCmdBeginDebugUtilsLabelEXT BeginDebugLabel;
    extern PFN_vkCmdEndDebugUtilsLabelEXT EndDebugLabel;

    bool CheckValLayerSupport();
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    // Debug messenger creation and population called here
    void SetupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger);
    // After debug messenger is used, destroy
    void DestroyDebugUtilsMessengerEXT(VkInstance& instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
    void initDebugLabelFunctions(VkInstance& instance);
    std::filesystem::path getExeDir();
}