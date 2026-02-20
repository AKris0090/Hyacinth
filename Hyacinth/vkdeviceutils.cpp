#include "vkdeviceutils.h"

namespace vkdeviceutils {
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence uploadFence = VK_NULL_HANDLE;

    void setDevice(VkDevice& dev) { device = dev; }
    void setAllocator(VmaAllocator& alloc) { allocator = alloc; }
    void setGraphicsQueue(VkQueue& queue) { graphicsQueue = queue; }
    void setSingleTimeCommandBuffer(VkCommandBuffer& cmd) { commandBuffer = cmd; }
    void setSingleTimeUploadFence(VkFence& fence) { uploadFence = fence; }

    void beginCommandBuffer(VkCommandBuffer& commandBuffer) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    }

    void beginSingleTimeCommandBuffer() {
        VK_CHECK(vkWaitForFences(device, 1, &uploadFence, true, 9999999999));
        VK_CHECK(vkResetFences(device, 1, &uploadFence));
        VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    }

    void endSubmitCommandBuffer() {
        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence));
        VkResult res = vkWaitForFences(device, 1, &uploadFence, true, 9999999999);
        VK_CHECK(res);
    }

    bool checkExtSupport(VkPhysicalDevice physicalDevice) {
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

    SWChainSuppDetails getDetails(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
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

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface) {
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

    bool isSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR& surface) {
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

    VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice& physicalDevice) {
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

    VkRenderingInfo createDepthRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* depthAttachment) {
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, renderArea };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pDepthAttachment = depthAttachment;
        renderingInfo.pStencilAttachment = nullptr;

        return renderingInfo;
    }

    VkRenderingInfo createRenderingInfo(VkExtent2D renderArea, uint32_t numColorAttachments, VkRenderingAttachmentInfo* colorAttachments, VkRenderingAttachmentInfo* depthAttachment) {
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, renderArea };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = numColorAttachments;
        renderingInfo.pColorAttachments = colorAttachments;
        renderingInfo.pDepthAttachment = depthAttachment;
        renderingInfo.pStencilAttachment = nullptr;

        return renderingInfo;
    }

    VkRenderingInfo createStencilRenderingInfo(VkExtent2D renderArea, VkRenderingAttachmentInfo* pStencilAttachment) {
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, renderArea };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pStencilAttachment = pStencilAttachment;

        return renderingInfo;
    }

    VulkanBuffer createBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, std::string qual, VkDeviceSize alignment) {
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

        if (qual != "") {
            vmaSetAllocationName(allocator, buffer.allocation, qual.c_str());
        }

        if (vmaFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
			buffer.pMappedData = buffer.info.pMappedData;
        }

        return buffer;
    }

    VulkanBuffer createBufferWithAlignment(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags vmaFlags, VkDeviceSize alignment, std::string qual) {
        return createBuffer(size, usage, memUsage, vmaFlags, qual, alignment);
    }

    void destroyBuffer(VulkanBuffer& buffer) {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    }

    void uploadToBuffer(VulkanBuffer& buffer, size_t size, void* data) {
        VulkanBuffer stagingBuffer = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        memcpy(stagingBuffer.info.pMappedData, data, size);

        executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
            VkBufferCopy copyRegion{};
            copyRegion.size = size;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, 1, &copyRegion);
            });

        destroyBuffer(stagingBuffer);
    }

    void stageAndUploadBuffers(VkDeviceSize* pSizes, void** ppData, VulkanBuffer* pBuffers, uint32_t count) {
		std::vector<VkBufferCopy> copyRegions(count);

        VkDeviceSize stagingSize = 0;
        for (int i = 0; i < count; i++) {
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = stagingSize;
            copyRegion.dstOffset = 0;
            copyRegion.size = pSizes[i];
            copyRegions[i] = copyRegion;

            stagingSize += pSizes[i];
        }

		VulkanBuffer stagingBuffer = createBuffer(stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, "staging_buffer");
        for (int i = 0; i < count; i++) {
			memcpy((char*)stagingBuffer.info.pMappedData + copyRegions[i].srcOffset, ppData[i], pSizes[i]);
        }

        executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
            for (int i = 0; i < count; i++) {
                vkCmdCopyBuffer(cmd, stagingBuffer.buffer, pBuffers[i].buffer, 1, &copyRegions[i]);
            }
		});
    }
}