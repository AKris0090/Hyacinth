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

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

namespace vkdebugutils {
    static bool CheckValLayerSupport() {
        uint32_t numLayers;
        vkEnumerateInstanceLayerProperties(&numLayers, nullptr);

        std::vector<VkLayerProperties> available(numLayers);
        vkEnumerateInstanceLayerProperties(&numLayers, available.data());

        for (const char* name : validationLayers) {
            bool found = false;

            // Check if layer is found using strcmp
            for (const auto& layerProps : available) {
                if (strcmp(name, layerProps.layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                return false;
            }
        }

        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
    }

    // Debug messenger creation and population called here
    static void SetupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        PopulateDebugMessengerCreateInfo(createInfo);

        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

        if (func != nullptr) {
            VkResult res = func(instance, &createInfo, nullptr, &debugMessenger);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
        else {
            throw std::runtime_error("failed to get vkCreateDebugUtilsMessengerEXT function address!");
        }
    }

    // After debug messenger is used, destroy
    static void DestroyDebugUtilsMessengerEXT(VkInstance& instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT> (vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
        else {
            throw std::runtime_error("failed to get vkDestroyDebugUtilsMessengerEXT function address!");
        }
    }

    static std::filesystem::path getExeDir()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer).parent_path();
    }
}