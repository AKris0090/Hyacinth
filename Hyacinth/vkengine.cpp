#include "vkengine.h"

void HyacinthEngine::createInstance()
{
	// Create vulkan instance and set up debug messenger if validation layers are enabled
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hyacinth Engine";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "Hyacinth";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCInfo{};
    instanceCInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCInfo.pApplicationInfo = &appInfo;

    unsigned int extensionCount = 0;
    std::vector<const char*> requiredExtensions(extensionCount);
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	requiredExtensions.assign(extensions, extensions + extensionCount);

    if (enableValLayers) {
        if (!vkdebugutils::checkValLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
		}

        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

        vkdebugutils::populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		instanceCInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceCInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        instanceCInfo.enabledLayerCount = 0;
    }

    instanceCInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceCInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VK_CHECK(vkCreateInstance(&instanceCInfo, nullptr, &m_instance));

    if (enableValLayers) {
        vkdebugutils::setupDebugMessenger(m_instance, m_debugMessenger);
	}

    // create surface and pick physical/logical device
	SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface);

    uint32_t numDevices = 0;
    VkPhysicalDevice* devices = nullptr;
	vkEnumeratePhysicalDevices(m_instance, &numDevices, devices);
    if (numDevices == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

	std::vector<VkPhysicalDevice> deviceList(numDevices);
	vkEnumeratePhysicalDevices(m_instance, &numDevices, deviceList.data());
    
    for (const auto& dev : deviceList) {
        if (vkdeviceutils::isSuitable(dev, m_surface)) {
            m_physicalDevice = dev;
            break;
		}
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    m_qfIndices = vkdeviceutils::findQueueFamilies(m_physicalDevice, m_surface);
    std::vector<VkDeviceQueueCreateInfo> queuecInfos;
    std::set<uint32_t> uniqueQFamilies = { m_qfIndices.graphicsFamily.value(), m_qfIndices.presentFamily.value() };

    float queuePrio = 1.0f;
    for (uint32_t queueFamily : uniqueQFamilies) {
        VkDeviceQueueCreateInfo queuecInfo{};
        queuecInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queuecInfo.queueFamilyIndex = queueFamily;
        queuecInfo.queueCount = 1;
        queuecInfo.pQueuePriorities = &queuePrio;
        queuecInfos.push_back(queuecInfo);
    }

    // TODO: populate when needed
    VkPhysicalDeviceFeatures gpuFeatures{};

    // TODO: populate when needed
    VkPhysicalDeviceVulkan13Features physDevFeatures{};
	physDevFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    physDevFeatures.synchronization2 = true;

    VkDeviceCreateInfo deviceCInfo{};
    deviceCInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCInfo.pNext = &physDevFeatures;
    deviceCInfo.queueCreateInfoCount = static_cast<uint32_t>(queuecInfos.size());
    deviceCInfo.pQueueCreateInfos = queuecInfos.data();
    deviceCInfo.pEnabledFeatures = &gpuFeatures;

	// deviceExts declared in vkdeviceutils.h
    deviceCInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
    deviceCInfo.ppEnabledExtensionNames = deviceExts.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, m_qfIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_qfIndices.presentFamily.value(), 0, &m_presentQueue);
}

void HyacinthEngine::createSwapchain()
{
    SWChainSuppDetails swInfo = vkdeviceutils::getDetails(m_physicalDevice, m_surface);

    VkSurfaceFormatKHR surfaceFormat = swInfo.chooseSwSurfaceFormat(swInfo.formats);
    VkPresentModeKHR presentMode = swInfo.chooseSwPresMode(swInfo.presentModes);
    VkExtent2D extent = swInfo.chooseSwExtent(swInfo.capabilities, m_window);

    uint32_t numImages = swInfo.capabilities.maxImageCount;
    maxFramesInFlight = numImages;

    if (swInfo.capabilities.maxImageCount > 0 && numImages > swInfo.capabilities.maxImageCount) {
        numImages = swInfo.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = m_surface;
    swapchainCreateInfo.minImageCount = numImages;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = vkdeviceutils::findQueueFamilies(m_physicalDevice, m_surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainCreateInfo.preTransform = swInfo.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapChain));

    m_swImageFormat.format = surfaceFormat.format;
    m_swImageFormat.extent = extent;
    m_swapChainImages.resize(numImages);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &numImages, m_swapChainImages.data());

    m_swapChainImageViews.resize(numImages);
    for (uint32_t i = 0; i < numImages; i++) {
        m_swapChainImageViews[i] = vkimageutils::createImageView(m_device, m_swapChainImages[i], m_swImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void HyacinthEngine::createCommandBuffers()
{
    // create command pool
    VkCommandPoolCreateInfo commandPoolCInfo{};
    commandPoolCInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCInfo.queueFamilyIndex = m_qfIndices.graphicsFamily.value();

	m_frameData.resize(maxFramesInFlight);
    
    for (int i = 0; i < maxFramesInFlight; i++) {
        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCInfo, nullptr, &m_frameData[i].commandPool));

        // create command buffers
        VkCommandBufferAllocateInfo CBAllocateInfo{};
        CBAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CBAllocateInfo.commandPool = m_frameData[i].commandPool;
        CBAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CBAllocateInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_device, &CBAllocateInfo, &m_frameData[i].commandBuffer));
    }
}

void HyacinthEngine::createSyncObjects()
{
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaInfo = {};
    semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_inFlightFences.resize(maxFramesInFlight);
    m_imageAcquiredSemas.resize(maxFramesInFlight);
    m_imageFinishedSemas.resize(maxFramesInFlight);

    for (int i = 0; i < maxFramesInFlight; i++) {
        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]));

        VK_CHECK(vkCreateSemaphore(m_device, &semaInfo, nullptr, &m_imageAcquiredSemas[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaInfo, nullptr, &m_imageFinishedSemas[i]));
    }

}

void HyacinthEngine::init()
{
	createInstance();

	createSwapchain();

	createCommandBuffers();

	createSyncObjects();

    m_initialized = true;
}

void HyacinthEngine::draw()
{
    VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX));

    uint32_t imageIndex = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAcquiredSemas[m_frameIndex], VK_NULL_HANDLE, &imageIndex));

    vkResetFences(m_device, 1, &m_inFlightFences[m_frameIndex]);
	VkCommandBuffer cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
	vkdeviceutils::beginCommandBuffer(cmd);

	vkimageutils::transition_image(cmd, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(m_frameIndex / 30.0f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange{};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdClearColorImage(cmd, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
	vkimageutils::transition_image(cmd, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    VK_CHECK(vkEndCommandBuffer(cmd));

    // submit the command buffer to the graphics queue
    VkSemaphoreSubmitInfo waitSemaSubmitInfo{};
    waitSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaSubmitInfo.semaphore = m_imageAcquiredSemas[m_frameIndex];
    waitSemaSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfo signalSemaSubmitInfo{};
    signalSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaSubmitInfo.semaphore = m_imageFinishedSemas[imageIndex];
    signalSemaSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    VkCommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = cmd;

    VkSubmitInfo2 queueSubmitInfo = {};
    queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    queueSubmitInfo.waitSemaphoreInfoCount = 1;
    queueSubmitInfo.pWaitSemaphoreInfos = &waitSemaSubmitInfo;
    queueSubmitInfo.signalSemaphoreInfoCount = 1;
    queueSubmitInfo.pSignalSemaphoreInfos = &signalSemaSubmitInfo;
    queueSubmitInfo.commandBufferInfoCount = 1;
    queueSubmitInfo.pCommandBufferInfos = &cmdSubmitInfo;

    VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &queueSubmitInfo, m_inFlightFences[m_frameIndex]));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_imageFinishedSemas[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &imageIndex;

    VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

    incrementFrameIndex(m_frameIndex);
}

void HyacinthEngine::cleanup()
{
    if (!m_initialized) {
        return;
	}

	vkDeviceWaitIdle(m_device);

    for(int i = 0; i < maxFramesInFlight; i++) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemas[i], nullptr);
		vkDestroySemaphore(m_device, m_imageFinishedSemas[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);

        vkDestroyCommandPool(m_device, m_frameData[i].commandPool, nullptr);
	}

	cleanupSwapchain(m_device, m_swapChain, m_swapChainImageViews);

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_debugMessenger != VK_NULL_HANDLE) {
        vkdebugutils::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}
