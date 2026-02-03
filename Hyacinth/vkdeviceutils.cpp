#include "vkdeviceutils.h"

void vkdeviceutils::beginCommandBuffer(VkCommandBuffer& commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void vkdeviceutils::beginSingleTimeCommandBuffer(DeviceContext* ctx) {
    VK_CHECK(vkWaitForFences(*ctx->device, 1, ctx->uploadFence, true, 9999999999));
    VK_CHECK(vkResetFences(*ctx->device, 1, ctx->uploadFence));
    VK_CHECK(vkResetCommandBuffer(*ctx->commandBuffer, 0));
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(*ctx->commandBuffer, &beginInfo));
}

void vkdeviceutils::endSubmitCommandBuffer(DeviceContext* ctx) {
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
    VkResult res = vkWaitForFences(*ctx->device, 1, ctx->uploadFence, true, 9999999999);
    VK_CHECK(res);
}

bool vkdeviceutils::checkExtSupport(VkPhysicalDevice physicalDevice) {
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

SWChainSuppDetails vkdeviceutils::getDetails(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
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

QueueFamilyIndices vkdeviceutils::findQueueFamilies(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
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

bool vkdeviceutils::isSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR& surface) {
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

VkSampleCountFlagBits vkdeviceutils::getMaxUsableSampleCount(VkPhysicalDevice& physicalDevice) {
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

VkRenderingInfo vkdeviceutils::createRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment) {
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

VulkanBuffer vkdeviceutils::createBuffer(DeviceContext& ctx, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, std::string qual, VkDeviceSize alignment) {
    VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    allocInfo.flags = vmaFlags;

    VulkanBuffer buffer{};
    VK_CHECK(vmaCreateBuffer(*ctx.allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));

    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer.buffer;
    buffer.gpuAddress = vkGetBufferDeviceAddress(*ctx.device, &addressInfo);

    if (qual != "") {
        vmaSetAllocationName(*ctx.allocator, buffer.allocation, qual.c_str());
    }

    return buffer;
}

VulkanBuffer vkdeviceutils::createBufferWithAlignment(DeviceContext& ctx, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, VkDeviceSize alignment, std::string qual) {
    return createBuffer(ctx, size, usage, memUsage, vmaFlags, qual, alignment);
}

void vkdeviceutils::destroyBuffer(VmaAllocator& allocator, VulkanBuffer& buffer) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void vkdeviceutils::uploadToBuffer(DeviceContext& ctx, VulkanBuffer& buffer, size_t size, void* data) {
    VulkanBuffer stagingBuffer = createBuffer(ctx, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    memcpy(stagingBuffer.info.pMappedData, data, size);

    executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, 1, &copyRegion);
        });

    destroyBuffer(*ctx.allocator, stagingBuffer);
}