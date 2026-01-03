#include "vkengine.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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

    VkPhysicalDeviceVulkan12Features dev12Features{};
    dev12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    dev12Features.bufferDeviceAddress = true;
    dev12Features.descriptorIndexing = true;

    // TODO: populate when needed
    VkPhysicalDeviceVulkan13Features dev13Features{};
    dev13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	dev13Features.pNext = &dev12Features;
    dev13Features.synchronization2 = true;
    dev13Features.dynamicRendering = true;

    VkDeviceCreateInfo deviceCInfo{};
    deviceCInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCInfo.pNext = &dev13Features;
    deviceCInfo.queueCreateInfoCount = static_cast<uint32_t>(queuecInfos.size());
    deviceCInfo.pQueueCreateInfos = queuecInfos.data();
    deviceCInfo.pEnabledFeatures = nullptr;

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

    VkExtent3D extent3D = {
        extent.width,
        extent.height,
        1
    };

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_depthImages.resize(numImages);
    for (uint32_t i = 0; i < numImages; i++) {
        m_depthImages[i].imageFormat = VK_FORMAT_D32_SFLOAT;
        m_depthImages[i].extent = extent3D;

        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo imageCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageCreateInfo.format = m_depthImages[i].imageFormat;
        imageCreateInfo.usage = depthImageUsages;
        imageCreateInfo.extent = m_depthImages[i].extent;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vmaCreateImage(m_allocator, &imageCreateInfo, &rimg_allocinfo, &m_depthImages[i].image, &m_depthImages[i].imageAllocation, nullptr);

        m_depthImages[i].imageView = vkimageutils::createImageView(m_device, m_depthImages[i].image, m_depthImages[i].imageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
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

    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCInfo, nullptr, &uploadFrame.commandPool));

    VkCommandBufferAllocateInfo CBAllocateInfo{};
    CBAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CBAllocateInfo.commandPool = uploadFrame.commandPool;
    CBAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CBAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(m_device, &CBAllocateInfo, &uploadFrame.commandBuffer));
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

	fenceInfo.flags = 0;
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_uploadFence));
}

void HyacinthEngine::createGraphicsPipeline()
{
    m_pipelineUtil.addShader(m_device, "shaders/vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_pipelineUtil.addShader(m_device, "shaders/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_pipelineUtil.setColorAttachmentFormat(m_swImageFormat.format);
    m_pipelineUtil.setMultisamplingNone();
	m_pipelineUtil.disableBlending();

    m_pipelineUtil.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    m_pipelineUtil.setDepthAttachmentFormat(m_depthImages[0].imageFormat);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swImageFormat.extent.width;
    viewport.height = (float)m_swImageFormat.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swImageFormat.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

	m_pipelineUtil.m_viewportState.pViewports = &viewport;
    m_pipelineUtil.m_viewportState.pScissors = &scissor;

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_pipelineUtil.m_pipeline.layout));

	m_pipelineUtil.buildPipeline(m_device);
}

// TODO: not hardcoded, load from model
void HyacinthEngine::createBuffers() {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

    float halfExtent = 0.5f;
	glm::vec4 color1 = { 1.0f, 0.0f, 0.0f, 1.0f };
    glm::vec4 color2 = { 0.0f, 1.0f, 0.0f, 1.0f };
    glm::vec4 color3 = { 0.0f, 0.0f, 1.0f, 1.0f };
    glm::vec4 color4 = { 1.0f, 1.0f, 1.0f, 1.0f };

    const glm::vec3 p[8] = {
        {-halfExtent, -halfExtent, -halfExtent}, // 0
        { halfExtent, -halfExtent, -halfExtent}, // 1
        { halfExtent,  halfExtent, -halfExtent}, // 2
        {-halfExtent,  halfExtent, -halfExtent}, // 3
        {-halfExtent, -halfExtent,  halfExtent}, // 4
        { halfExtent, -halfExtent,  halfExtent}, // 5
        { halfExtent,  halfExtent,  halfExtent}, // 6
        {-halfExtent,  halfExtent,  halfExtent}  // 7
    };

    struct Face {
        uint32_t i0, i1, i2, i3;
        glm::vec3 normal;
    };

    const Face faces[6] = {
        {4, 5, 6, 7, { 0,  0,  1}}, // Front
        {1, 0, 3, 2, { 0,  0, -1}}, // Back
        {0, 4, 7, 3, {-1,  0,  0}}, // Left
        {5, 1, 2, 6, { 1,  0,  0}}, // Right
        {3, 7, 6, 2, { 0,  1,  0}}, // Top
        {0, 1, 5, 4, { 0, -1,  0}}  // Bottom
    };

    for (uint32_t f = 0; f < 6; ++f) {
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        const Face& face = faces[f];

        vertices.push_back({ p[face.i0], 0.0f, face.normal, 0.0f, color1 });
        vertices.push_back({ p[face.i1], 1.0f, face.normal, 0.0f, color2 });
        vertices.push_back({ p[face.i2], 1.0f, face.normal, 1.0f, color3 });
        vertices.push_back({ p[face.i3], 0.0f, face.normal, 1.0f, color4 });

        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);

        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
        indices.push_back(baseIndex + 0);
    }

    m_meshBuffers = vkmeshutils::uploadMesh(m_device, m_allocator, uploadFrame.commandBuffer, m_graphicsQueue, m_uploadFence, indices, vertices);
	m_meshBuffers.indexCount = static_cast<uint32_t>(indices.size());
}

glm::mat4 HyacinthEngine::getCamMatrix() {
    glm::mat4 rotation = glm::mat4(1.0f);
    rotation = glm::rotate(rotation, (SDL_GetTicks() / 1000.0f), glm::vec3(1.0f, 1.0f, 1.0f));

    glm::mat4 view = glm::translate(glm::mat4(1.f), glm::vec3{0,0,-4});
    glm::mat4 projection = glm::perspective(glm::radians(90.f), (float)m_swImageFormat.extent.width / (float)m_swImageFormat.extent.height, 0.1f, 1000.0f);

    projection[1][1] *= -1;

    return projection * view * rotation;
}

void HyacinthEngine::init()
{
	createInstance();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

	createSwapchain();

	createCommandBuffers();

	createSyncObjects();

    createGraphicsPipeline();

    createBuffers();

    m_initialized = true;
}

void HyacinthEngine::setupDraw()
{
    VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX));

    VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAcquiredSemas[m_frameIndex], VK_NULL_HANDLE, &m_imageIndex));

    vkResetFences(m_device, 1, &m_inFlightFences[m_frameIndex]);
    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    vkdeviceutils::beginCommandBuffer(cmd);

    vkimageutils::transitionImage(cmd, m_swapChainImages[m_imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkimageutils::transitionImage(cmd, m_depthImages[m_imageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
}

void HyacinthEngine::draw()
{
    setupDraw();
    VkCommandBuffer cmd = getCurrentFrame().commandBuffer;
    VkRenderingAttachmentInfo colorAttachment = vkimageutils::createAttachmentInfo(m_swapChainImageViews[m_imageIndex], clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkimageutils::createDepthAttachmentInfo(m_depthImages[m_imageIndex].imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineUtil.m_pipeline.pipeline);

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = getCamMatrix();
    push_constants.vertexBuffer = m_meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, m_pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, m_meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swImageFormat.extent.width);
    viewport.height = static_cast<float>(m_swImageFormat.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swImageFormat.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdDrawIndexed(cmd, m_meshBuffers.indexCount, 1, 0, 0, 0);

    endDraw();
}

void HyacinthEngine::endDraw()
{
    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    vkCmdEndRendering(cmd);
    vkimageutils::transitionImage(cmd, m_swapChainImages[m_imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // submit the command buffer to the graphics queue
    VkSemaphoreSubmitInfo waitSemaSubmitInfo{};
    waitSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaSubmitInfo.semaphore = m_imageAcquiredSemas[m_frameIndex];
    waitSemaSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfo signalSemaSubmitInfo{};
    signalSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaSubmitInfo.semaphore = m_imageFinishedSemas[m_imageIndex];
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
    presentInfo.pWaitSemaphores = &m_imageFinishedSemas[m_imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &m_imageIndex;

    VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

    incrementFrameIndex(m_frameIndex);
}

void HyacinthEngine::cleanup()
{
    if (!m_initialized) {
        return;
	}

	vkDeviceWaitIdle(m_device);

	vkdeviceutils::destroyBuffer(m_allocator, m_meshBuffers.indexBuffer);
	vkdeviceutils::destroyBuffer(m_allocator, m_meshBuffers.vertexBuffer);

	vkDestroyFence(m_device, m_uploadFence, nullptr);

	vkDestroyPipeline(m_device, m_pipelineUtil.m_pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineUtil.m_pipeline.layout, nullptr);

    for(int i = 0; i < maxFramesInFlight; i++) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemas[i], nullptr);
		vkDestroySemaphore(m_device, m_imageFinishedSemas[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);

        vkDestroyCommandPool(m_device, m_frameData[i].commandPool, nullptr);

        vkDestroyImageView(m_device, m_depthImages[i].imageView, nullptr);
        vmaDestroyImage(m_allocator, m_depthImages[i].image, m_depthImages[i].imageAllocation);
	}

    vmaDestroyAllocator(m_allocator);

	vkDestroyCommandPool(m_device, uploadFrame.commandPool, nullptr);

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
