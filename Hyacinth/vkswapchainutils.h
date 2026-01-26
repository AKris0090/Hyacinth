#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>

struct SWChainSuppDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    VkSurfaceFormatKHR chooseSwSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwPresMode(const std::vector<VkPresentModeKHR>& availablePresModes);
    VkExtent2D chooseSwExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
};

struct SWChainImageFormat {
    VkFormat format;
	VkExtent2D extent;
    float aspectRatio;
};