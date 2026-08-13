#define VMA_IMPLEMENTATION

#ifndef NDEBUG
#define VMA_DEBUG_LOG_FORMAT(format, ...) \
    printf("[VMA] " format "\n", __VA_ARGS__)

#define VMA_LEAK_LOG_FORMAT(format, ...) \
    printf("[VMA-LEAK] " format "\n", __VA_ARGS__)
#endif

#include "hyacinthengine.h"
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
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (enableValLayers) {
        if (!vkdebugutils::CheckValLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
		}

        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

        vkdebugutils::PopulateDebugMessengerCreateInfo(debugCreateInfo);
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
        vkdebugutils::SetupDebugMessenger(m_instance, m_debugMessenger);
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

	VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
    deviceFeatures.samplerAnisotropy = true;
    deviceFeatures.depthClamp = true;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{};
    accelFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeature.accelerationStructure = true;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeature{};
    rtFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeature.pNext = &accelFeature;
    rtFeature.rayTracingPipeline = true;

    VkPhysicalDeviceVulkan12Features dev12Features{};
    dev12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    dev12Features.pNext = &rtFeature;
    dev12Features.bufferDeviceAddress = true;
    dev12Features.descriptorIndexing = true;
    dev12Features.runtimeDescriptorArray = true;
    dev12Features.runtimeDescriptorArray = true;
    dev12Features.separateDepthStencilLayouts = true;

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
    deviceCInfo.pEnabledFeatures = &deviceFeatures;

	// deviceExts declared in vkdeviceutils.h
    deviceCInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
    deviceCInfo.ppEnabledExtensionNames = deviceExts.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCInfo, nullptr, &m_device));
    vkdeviceutils::setDevice(m_device);

    vkGetDeviceQueue(m_device, m_qfIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_qfIndices.presentFamily.value(), 0, &m_presentQueue);
	vkdeviceutils::setGraphicsQueue(m_graphicsQueue);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR		rtProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR	asProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

    VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    rtProperties.pNext = &asProperties;
    prop2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

    rt::setRTProperties(rtProperties);
    rt::setASProperties(asProperties);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
    vkimageutils::setLinear(formatProperties.optimalTilingFeatures);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    vkimageutils::setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);

    m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;// vkdeviceutils::getMaxUsableSampleCount(m_physicalDevice);

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
    vkdeviceutils::setAllocator(m_allocator);

    vkdebugutils::initDebugLabelFunctions(m_instance);
}

void HyacinthEngine::createSwapchain()
{
    SWChainSuppDetails swInfo = vkdeviceutils::getDetails(m_physicalDevice, m_surface);

    VkSurfaceFormatKHR surfaceFormat = swInfo.chooseSwSurfaceFormat(swInfo.formats);
    VkPresentModeKHR presentMode = swInfo.chooseSwPresMode(swInfo.presentModes);
    VkExtent2D extent = swInfo.chooseSwExtent(swInfo.capabilities, m_window);

    uint32_t numImages = swInfo.capabilities.minImageCount + 1;

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
    std::vector<VkImage> tempSWImages(numImages);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &numImages, tempSWImages.data());

    for (uint32_t i = 0; i < numImages; i++) {
        m_swapChainImages[i].image = tempSWImages[i];
        m_swapChainImages[i].imageFormat = m_swImageFormat.format;
    }

    for (uint32_t i = 0; i < numImages; i++) {
        m_swapChainImages[i].imageView = vkimageutils::createImageView(m_swapChainImages[i], 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, false);
    }

    m_swImageFormat.aspectRatio = (float)m_swImageFormat.extent.width / (float)m_swImageFormat.extent.height;
}

void HyacinthEngine::createColorImages() {
    VkExtent3D extent{
        .width = m_swImageFormat.extent.width,
        .height = m_swImageFormat.extent.height,
        .depth = 1
    };
    int numImages = static_cast<int>(m_swapChainImages.size());
    m_gBuffers.resize(numImages);

    for (auto& gb : m_gBuffers) {
        gb.depth = vkimageutils::createImageandView(extent, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "depth_image");
        gb.albedo = vkimageutils::createImageandView(extent, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "albedo_image");
		gb.normal = vkimageutils::createImageandView(extent, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "normal_image");
        gb.AMR = vkimageutils::createImageandView(extent, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "amr_image");
        gb.ddgiImage = vkimageutils::createImageandView(extent, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "ddgi_image");
        gb.stencilDepth = vkimageutils::createImageandView(extent, 1, VK_FORMAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_msaaSamples, false, "stencil_depth_image");
        gb.compositeImage = vkimageutils::createImageandView(extent, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_msaaSamples, false, "composite_image");
	    vkimageutils::createImageSampler(gb.albedo);
	    vkimageutils::createImageSampler(gb.normal);
        vkimageutils::createImageSampler(gb.AMR);
        vkimageutils::createImageSampler(gb.depth, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
        vkimageutils::createImageSampler(gb.ddgiImage);
        vkimageutils::createImageSampler(gb.compositeImage);
    }
}

void HyacinthEngine::createCommandBuffers()
{
    // create command pool
    VkCommandPoolCreateInfo commandPoolCInfo{};
    commandPoolCInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCInfo.queueFamilyIndex = m_qfIndices.graphicsFamily.value();

	m_frameData.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCInfo, nullptr, &m_frameData[i].commandPool));

        // create command buffers
        VkCommandBufferAllocateInfo CBAllocateInfo{};
        CBAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CBAllocateInfo.commandPool = m_frameData[i].commandPool;
        CBAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CBAllocateInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_device, &CBAllocateInfo, &m_frameData[i].commandBuffer));
    }

    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCInfo, nullptr, &m_uploadFrame.commandPool));

    VkCommandBufferAllocateInfo CBAllocateInfo{};
    CBAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CBAllocateInfo.commandPool = m_uploadFrame.commandPool;
    CBAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CBAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(m_device, &CBAllocateInfo, &m_uploadFrame.commandBuffer));
	vkdeviceutils::setSingleTimeCommandBuffer(m_uploadFrame.commandBuffer);
}

void HyacinthEngine::createSyncObjects()
{
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaInfo = {};
    semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imageAcquiredSemas.resize(MAX_FRAMES_IN_FLIGHT);
    m_imageFinishedSemas.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]));

        VK_CHECK(vkCreateSemaphore(m_device, &semaInfo, nullptr, &m_imageAcquiredSemas[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaInfo, nullptr, &m_imageFinishedSemas[i]));
    }

    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_uploadFence));
	vkdeviceutils::setSingleTimeUploadFence(m_uploadFence);
}

void HyacinthEngine::createDepthPipeline()
{
    m_depthPipelineUtil.addShader("shaders/depth.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_depthPipelineUtil.addShader("shaders/depthDiscard.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_depthPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_depthPipelineUtil.setDefaultAttributes();
    m_depthPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_depthPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    m_depthPipelineUtil.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 0);
    m_depthPipelineUtil.setMultisampling(m_msaaSamples);
    m_depthPipelineUtil.disableBlending();
    m_depthPipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    m_depthPipelineUtil.setDepthAttachmentFormat(m_gBuffers[0].depth.imageFormat);
    m_depthPipelineUtil.numColorAttachments = 0;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swImageFormat.extent.width;
    viewport.height = (float)m_swImageFormat.extent.height;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 0.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swImageFormat.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    m_depthPipelineUtil.m_viewportState.pViewports = &viewport;
    m_depthPipelineUtil.m_viewportState.pScissors = &scissor;

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 2> sets = { m_descriptorSetLayout, m_textureSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &range;
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_depthPipelineUtil.m_pipeline.layout));

    m_depthPipelineUtil.buildPipeline();
}

void HyacinthEngine::createGraphicsPipeline()
{
    m_pipelineUtil.addShader("shaders/vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_pipelineUtil.addShader("shaders/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_pipelineUtil.setDefaultAttributes();
	m_pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_pipelineUtil.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 3);
    m_pipelineUtil.setMultisampling(m_msaaSamples);
	m_pipelineUtil.disableBlending();
    m_pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_EQUAL);
    m_pipelineUtil.setDepthAttachmentFormat(m_gBuffers[0].depth.imageFormat);
    m_pipelineUtil.numColorAttachments = 3;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swImageFormat.extent.width;
    viewport.height = (float)m_swImageFormat.extent.height;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 0.0f;

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

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 3> sets = { m_descriptorSetLayout, m_shadowSetLayout, m_textureSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &range;
	pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
	pipelineLayoutCInfo.pSetLayouts = sets.data();

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_pipelineUtil.m_pipeline.layout));

	m_pipelineUtil.buildPipeline();
}

void HyacinthEngine::createCompSkinPipeline() {
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(computeSkinPushConstant);
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    m_computeSkinPipeline = vkpipelineutils::createComputePipeline(nullptr, 0, &range, 1, "shaders/compSkin.spv");
}

void HyacinthEngine::createCompositePipeline()
{
    m_compositePipelineUtil.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_compositePipelineUtil.addShader("shaders/composite.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_compositePipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_compositePipelineUtil.setDefaultAttributes();
    m_compositePipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_compositePipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    m_compositePipelineUtil.setColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 1);
    m_compositePipelineUtil.setMultisampling(m_msaaSamples);
    m_compositePipelineUtil.disableBlending();

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

    m_compositePipelineUtil.m_viewportState.pViewports = &viewport;
    m_compositePipelineUtil.m_viewportState.pScissors = &scissor;

    std::array<VkDescriptorSetLayout, 2> sets = { m_compositeSetLayout, m_descriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_compositePipelineUtil.m_pipeline.layout));

    m_compositePipelineUtil.buildPipeline();
}

void HyacinthEngine::createFXAAPipeline()
{
    m_fxaaPipelineUtil.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_fxaaPipelineUtil.addShader("shaders/fxaaFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_fxaaPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_fxaaPipelineUtil.setDefaultAttributes();
    m_fxaaPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_fxaaPipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    m_fxaaPipelineUtil.setColorAttachmentFormat(m_swImageFormat.format, 1);
    m_fxaaPipelineUtil.setMultisampling(m_msaaSamples);
    m_fxaaPipelineUtil.disableBlending();

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

    m_fxaaPipelineUtil.m_viewportState.pViewports = &viewport;
    m_fxaaPipelineUtil.m_viewportState.pScissors = &scissor;

    VkPushConstantRange pcRange;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::vec2);
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 1> sets = { m_postProcessSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &pcRange;

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_fxaaPipelineUtil.m_pipeline.layout));

    m_fxaaPipelineUtil.buildPipeline();
}

void HyacinthEngine::createDDGIVolumePipeline()
{
    m_volumeStencilPipeline.addShader("shaders/volumeVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_volumeStencilPipeline.addShader("shaders/ddgiStencilFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_volumeStencilPipeline.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_volumeStencilPipeline.setDefaultAttributes();
    m_volumeStencilPipeline.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_volumeStencilPipeline.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    m_volumeStencilPipeline.setStencilAttachmentFormat(VK_FORMAT_S8_UINT);
    m_volumeStencilPipeline.setMultisampling(m_msaaSamples);
    m_volumeStencilPipeline.disableBlending();

    m_volumeStencilPipeline.m_depthStencil.stencilTestEnable = true;
    m_volumeStencilPipeline.m_depthStencil.front.compareMask = ANY_BIT;
    m_volumeStencilPipeline.m_depthStencil.front.writeMask = CURRENT_BIT;
    m_volumeStencilPipeline.m_depthStencil.front.compareOp = VK_COMPARE_OP_EQUAL;
    m_volumeStencilPipeline.m_depthStencil.front.reference = 1;
    m_volumeStencilPipeline.m_depthStencil.front.passOp = VK_STENCIL_OP_REPLACE;
    m_volumeStencilPipeline.m_depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
    m_volumeStencilPipeline.m_depthStencil.back = m_volumeStencilPipeline.m_depthStencil.front;

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

    m_volumeStencilPipeline.m_viewportState.pViewports = &viewport;
    m_volumeStencilPipeline.m_viewportState.pScissors = &scissor;

    VkPushConstantRange volumeInfoRange{};
    volumeInfoRange.offset = 0;
    volumeInfoRange.size = sizeof(volumeStencilPushConstant);
    volumeInfoRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 2> sets = { m_descriptorSetLayout, m_compositeSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &volumeInfoRange;

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_volumeStencilPipeline.m_pipeline.layout));

    m_volumeStencilPipeline.buildPipeline();
}

void HyacinthEngine::createDDGIPipeline()
{
    m_ddgiPipelineUtil.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_ddgiPipelineUtil.addShader("shaders/ddgiFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_ddgiPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_ddgiPipelineUtil.setDefaultAttributes();
    m_ddgiPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_ddgiPipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    m_ddgiPipelineUtil.setColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 1);
    m_ddgiPipelineUtil.setMultisampling(m_msaaSamples);
    m_ddgiPipelineUtil.disableBlending();
    m_ddgiPipelineUtil.setStencilAttachmentFormat(VK_FORMAT_S8_UINT);
    m_ddgiPipelineUtil.m_depthStencil.stencilTestEnable = true;
    m_ddgiPipelineUtil.m_depthStencil.front.compareMask = CURRENT_BIT;
    m_ddgiPipelineUtil.m_depthStencil.front.writeMask = ANY_BIT | CURRENT_BIT;
    m_ddgiPipelineUtil.m_depthStencil.front.compareOp = VK_COMPARE_OP_EQUAL;
    m_ddgiPipelineUtil.m_depthStencil.front.reference = 1;
    m_ddgiPipelineUtil.m_depthStencil.front.passOp = VK_STENCIL_OP_INVERT;
    m_ddgiPipelineUtil.m_depthStencil.front.failOp = VK_STENCIL_OP_ZERO;
    m_ddgiPipelineUtil.m_depthStencil.back = m_ddgiPipelineUtil.m_depthStencil.front;

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

    m_ddgiPipelineUtil.m_viewportState.pViewports = &viewport;
    m_ddgiPipelineUtil.m_viewportState.pScissors = &scissor;

    std::array<VkDescriptorSetLayout, 3> sets = { m_compositeSetLayout, m_descriptorSetLayout, m_owDDGIHelper.m_irradianceVisSetLayout };

    VkPushConstantRange volumeInfoRange{};
    volumeInfoRange.offset = 0;
    volumeInfoRange.size = sizeof(ComputePushConstant);
    volumeInfoRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &volumeInfoRange;

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_ddgiPipelineUtil.m_pipeline.layout));

    m_ddgiPipelineUtil.buildPipeline(); 
}

void HyacinthEngine::loadAssets() {
    auto path = vkdebugutils::getExeDir() / "objects" / "test_scene.glb";
    // auto path = vkdebugutils::getExeDir() / "objects" / "sponza" / "sponza.gltf";
    auto sphereTestPath = vkdebugutils::getExeDir() / "objects" / "sphereTest.glb";
    auto helmetPath = vkdebugutils::getExeDir() / "objects" / "DamagedHelmet.glb";
    auto thirdPersonCharacterPath = vkdebugutils::getExeDir() / "objects" / "char_skinned2.glb";
    auto firstPersonCharacterPath = vkdebugutils::getExeDir() / "objects" / "char_fp6.glb";
    auto pistolPath = vkdebugutils::getExeDir() / "objects" / "gun2.glb";
    auto tracerPath = vkdebugutils::getExeDir() / "objects" / "tracer.glb";
    auto flashPath = vkdebugutils::getExeDir() / "objects" / "flash.glb";

    m_assetDrawer.addDummyTextures();
    m_assetDrawer.addUITextures();

    m_assetDrawer.loadMesh(path.string(), "world", false, true);
    m_assetDrawer.loadMesh(sphereTestPath.string(), "sphere_test", false, true);
    m_assetDrawer.loadMesh(helmetPath.string(), "helmet", false, true);
    m_assetDrawer.loadMesh(tracerPath.string(), "tracer", false, true);
    m_assetDrawer.loadMesh(flashPath.string(), "flashbang", true, true);
    m_assetDrawer.loadMesh(thirdPersonCharacterPath.string(), "tp_character", true, true);
    m_assetDrawer.loadMesh(firstPersonCharacterPath.string(), "fp_arms", true, true);
    m_assetDrawer.loadMesh(pistolPath.string(), "pistol", true, true);
}

void HyacinthEngine::createBuffers() {
    m_assetDrawer.uploadBuffersToGPU();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].uniformBuffer = vkdeviceutils::createBuffer(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "frame_uniform");
        m_frameData[i].mappedUniformBuffer = m_frameData[i].uniformBuffer.info.pMappedData;

        // create render call buffer
        m_frameData[i].m_renderListBuffer = vkdeviceutils::createBuffer(sizeof(HRenderCall), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "render_list_buffer");

        // create indirect draw buffer
        m_frameData[i].m_indirectDrawBuffer = vkdeviceutils::createBuffer(sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "indirect_draw_buffer");

        // create skinnedVertexBuffer
        m_frameData[i].m_skinnedVertexBuffer = vkdeviceutils::createBuffer(sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "frame_skinned_vertex");
    }

    for (int j = 0; j < SHADOW_MAP_CASCADE_COUNT; j++) {
        m_shadowHelper.m_cascades[j].cascadeDrawBuffer = vkdeviceutils::createBuffer(sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "shadow_draw_buffer");
    }
}

void HyacinthEngine::createDescriptorSets()
{
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (((float)(m_assetDrawer.textures.size())) / (float)MAX_FRAMES_IN_FLIGHT)}, // divided because multiplied by maxSets later. we only want 1 descriptor per texture, not 3
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (float)(1.f / MAX_FRAMES_IN_FLIGHT) } // for shadows, only 1 (not 3)
    };
    m_descriptorAllocator.initPool(MAX_FRAMES_IN_FLIGHT * 5, sizes);

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        m_descriptorSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // shadow map
        m_shadowSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, static_cast<uint32_t>(m_assetDrawer.textures.size()), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        m_textureSetLayout = layoutBuilder.buildLayout(nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    {
        DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
        layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // normal
        layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // ambient, metallic, rough
        layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // depth
        layoutBuilder.addBinding(4, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // ddgi
        m_compositeSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // compositeimage
        m_postProcessSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

	m_textureSet = m_descriptorAllocator.allocate(m_textureSetLayout);
    m_assetDrawer.bindTextures(m_textureSet);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].uniformDescriptorSet = m_descriptorAllocator.allocate(m_descriptorSetLayout);
        m_frameData[i].shadowDescriptorSet = m_descriptorAllocator.allocate(m_shadowSetLayout);

        vkdescriptorutils::queueWriteBuffer(m_frameData[i].uniformDescriptorSet, 0, sizeof(UBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frameData[i].uniformBuffer);

        vkdescriptorutils::queueWriteImage(m_frameData[i].shadowDescriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_shadowHelper.m_shadowImage, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);

		m_gBuffers[i].m_compositeSet = m_descriptorAllocator.allocate(m_compositeSetLayout);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].AMR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].depth, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].ddgiImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_gBuffers[i].m_postProcessSet = m_descriptorAllocator.allocate(m_postProcessSetLayout);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_postProcessSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].compositeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    vkdescriptorutils::flushDescriptorWrites();
}

void HyacinthEngine::setupImGUI() 
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    imguihelper::ApplyDarkModeStyle();

    ImGui_ImplSDL3_InitForVulkan(m_window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.CheckVkResultFn = [](VkResult res) {
        if (res != VK_SUCCESS) {
            std::cerr << "ImGui Vulkan error: " << res << std::endl;
        }
	};

    VkPipelineRenderingCreateInfo imguiCreateInfo;
    imguiCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiCreateInfo.pNext = nullptr;
    imguiCreateInfo.colorAttachmentCount = 1;
    imguiCreateInfo.pColorAttachmentFormats = &m_swImageFormat.format;
    imguiCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    imguiCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    imguiCreateInfo.viewMask = 0x0;

    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physicalDevice;
    init_info.Device = m_device;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = imguiCreateInfo;
    init_info.PipelineInfoMain.MSAASamples = m_msaaSamples;
    init_info.QueueFamily = m_qfIndices.graphicsFamily.value();
    init_info.Queue = m_graphicsQueue;
    init_info.UseDynamicRendering = true;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    init_info.MinImageCount = static_cast<uint32_t>(m_swapChainImages.size());
    init_info.ImageCount = static_cast<uint32_t>(m_swapChainImages.size());
    init_info.Allocator = nullptr;
    bool res = ImGui_ImplVulkan_Init(&init_info);
}

void HyacinthEngine::bakeDDGI() {
    generateRenderList();
    m_owDDGIHelper.bakeDDGI(m_textureSet, m_frameData[0].m_renderListBuffer.gpuAddress, m_assetDrawer.g_vertexBuffer.gpuAddress, m_assetDrawer.g_indexBuffer.gpuAddress, m_assetDrawer.materialInfoBuffer.gpuAddress);
}

void HyacinthEngine::init()
{
	createInstance();

	createSwapchain();

	createCommandBuffers();

	createSyncObjects(); // also creates device context

    m_camera = Camera(m_swImageFormat.aspectRatio, BASE_FOV, 0.01f, 50.f);

    createColorImages();

    m_rtHelper.setup();

    loadAssets();

    m_frustumCullHelper.setup();

    m_shadowHelper.setup(MAX_FRAMES_IN_FLIGHT, m_frustumCullHelper.m_computeLayout);

    createBuffers();

    m_rtHelper.generateBLASForMesh(m_assetDrawer.getStaticMeshRef("world"), m_assetDrawer.g_vertexBuffer.gpuAddress, m_assetDrawer.g_indexBuffer.gpuAddress);
    m_rtHelper.createTopLevelAS();

    createDescriptorSets();

    m_skyboxHelper.setup(m_swImageFormat, m_descriptorSetLayout);

    m_owDDGIHelper.setup(&m_rtHelper, m_skyboxHelper.m_skyboxImage, m_textureSetLayout);
    m_owDDGIHelper.m_probeVis.createProbeVisualizationStructures(m_descriptorSetLayout, m_owDDGIHelper.m_irradianceVisSetLayout, m_gBuffers[0].depth.imageFormat, m_swImageFormat, m_msaaSamples);
	m_owDDGIHelper.m_volumeVis.createVolumeVisualizationStructures(m_descriptorSetLayout, m_gBuffers[0].depth.imageFormat, m_swImageFormat, m_msaaSamples);

    createDepthPipeline();

    createGraphicsPipeline();

    createCompSkinPipeline();

    createCompositePipeline();

    createFXAAPipeline();

    createDDGIPipeline();

    createDDGIVolumePipeline();

    setupImGUI();

    m_shadowHelper.setupImGui();

    m_initialized = true;

    volANormalBias = m_owDDGIHelper.m_probeVolumes[0].data.pos.w;
    // volBNormalBias = m_owDDGIHelper.m_probeVolumes[1].data.pos.w;
    volAViewBias = m_owDDGIHelper.m_probeVolumes[0].data.spacing.w;
    // volBViewBias = m_owDDGIHelper.m_probeVolumes[1].data.spacing.w;

    m_uiHelper.setup(m_textureSetLayout, static_cast<uint32_t>(DUMMY_TEX_PATHS.size()), glm::vec2(m_swImageFormat.extent.width, m_swImageFormat.extent.height), m_swImageFormat, m_msaaSamples);
    m_worldHealthManager.setup(m_textureSetLayout, m_descriptorSetLayout, static_cast<uint32_t>(DUMMY_TEX_PATHS.size() + UI_TEXTURE_PATHS.size()), m_swImageFormat, m_gBuffers[0].depth.imageFormat, m_msaaSamples);
    m_ambientHelper.setup(m_descriptorSetLayout, m_compositeSetLayout, m_swImageFormat);

#ifdef DEBUG_NETWORK
    m_netDebugRenderer.setup(m_swImageFormat, m_msaaSamples, m_descriptorSetLayout);
#endif
}

void HyacinthEngine::addStaticGameObject(HStaticGameObject* gameObjectRef) {
    m_staticObjects.push_back(gameObjectRef);
}

void HyacinthEngine::addAnimatedGameObject(HAnimatedGameObject* gameObjectRef) {
    AnimatedObjectWrap wrap;
    wrap.gameObject = gameObjectRef;
    wrap.jointMatrixBuffer = vkdeviceutils::createBuffer(gameObjectRef->mesh->jointMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_joint_matrix_buffer_otra");
    m_animatedObjects.push_back(wrap);
}

void HyacinthEngine::generateRenderList() {
    m_renderList.clear();
    for (const auto& o : m_staticObjects) {
        for (const auto& n : o->mesh->meshedNodes) {
            for (const auto& p : n->primitives) {
                m_renderList.push_back(HRenderCall{
                    .transformMatrix = o->transform.getMatrix() * n->getMatrix(),
                    .aaBBMin = p.bounds.min,
                    .aabbMax = p.bounds.max,
                    .materialIndex = p.materialIndex,
                    .indexCount = p.indexCount,
                    .firstIndex = p.firstIndex,
                    .vertexOffset = p.firstVertex,
                    .meshID = 0
                });
            }
        }
    }

    for (const auto& tracer : m_tracerManager.tracers) {
        for (const auto& n : tracer.gameObject->mesh->meshedNodes) {
            for (const auto& p : n->primitives) {
                m_renderList.push_back(HRenderCall{
                    .transformMatrix = tracer.matrix * n->getMatrix(),
                    .aaBBMin = p.bounds.min,
                    .aabbMax = p.bounds.max,
                    .materialIndex = p.materialIndex,
                    .indexCount = p.indexCount,
                    .firstIndex = p.firstIndex,
                    .vertexOffset = p.firstVertex,
                    .meshID = 0
                });
            }
        }
    }

    numStaticDrawCommands = static_cast<uint32_t>(m_renderList.size());

    uint32_t animatedVertexOffset = 0;
    for (const auto& ao : m_animatedObjects) {
        if (!ao.gameObject->active) continue;
        for (const auto& n : ao.gameObject->mesh->meshedNodes) {
            for (const auto& p : n->primitives) {
                m_renderList.push_back(HRenderCall{
                    .transformMatrix = ao.gameObject->transform.getMatrix() * n->getMatrix(),
                    .materialIndex = p.materialIndex,
                    .indexCount = p.indexCount,
                    .firstIndex = p.firstIndex,
                    .vertexOffset = animatedVertexOffset + p.firstVertex - ao.gameObject->mesh->vertexOffset,
                    .meshID = 0
                    });
            }
        }
        animatedVertexOffset += ao.gameObject->mesh->numVertices;
    }
    if (p_netEntManager) {
        for (const auto& [id, ao] : p_netEntManager->gameObjects) {
            if (!ao.gameObject->active) continue;
            for (const auto& n : ao.gameObject->mesh->meshedNodes) {
                for (const auto& p : n->primitives) {
                    m_renderList.push_back(HRenderCall{
                        .transformMatrix = ao.gameObject->transform.getMatrix() * n->getMatrix(),
                        .materialIndex = p.materialIndex,
                        .indexCount = p.indexCount,
                        .firstIndex = p.firstIndex,
                        .vertexOffset = animatedVertexOffset + p.firstVertex - ao.gameObject->mesh->vertexOffset,
                        .meshID = 0
                        });
                }
            }
            animatedVertexOffset += ao.gameObject->mesh->numVertices;
        }
    }
    numDynamicDrawCommands = static_cast<uint32_t>(m_renderList.size()) - numStaticDrawCommands;

    if (m_frameData[m_frameIndex].m_skinnedVertexBuffer.info.size < (animatedVertexOffset * sizeof(Vertex))) {
        vkdeviceutils::resizeBuffer(m_frameData[m_frameIndex].m_skinnedVertexBuffer, (animatedVertexOffset * sizeof(Vertex)));
    }
    vkdeviceutils::updateBuffer(m_frameData[m_frameIndex].m_renderListBuffer, m_renderList.size() * sizeof(HRenderCall), m_renderList.data());
}

void HyacinthEngine::generateDrawCommands() {
    m_drawCommands.clear();
    uint32_t drawIndex = 0;
    for (const auto& d : m_renderList) {
        VkDrawIndexedIndirectCommand dCom;
        dCom.firstIndex = d.firstIndex;
        dCom.firstInstance = drawIndex;
        dCom.indexCount = d.indexCount;
        dCom.instanceCount = 1;
        dCom.vertexOffset = d.vertexOffset;
        m_drawCommands.push_back(dCom);
        drawIndex++;
    }

    vkdeviceutils::updateBuffer(m_frameData[m_frameIndex].m_indirectDrawBuffer, m_drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand), m_drawCommands.data());
    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        vkdeviceutils::updateBuffer(m_shadowHelper.m_cascades[i].cascadeDrawBuffer, m_drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand), m_drawCommands.data());
    }
}

void HyacinthEngine::update() {
    if (InputManager::tabKeyDown()) {
		m_showImGui = !m_showImGui;
    }

    LightMesh* worldMeshRef = m_assetDrawer.getStaticMeshRef("world");
    AABB worldBounds{};
    worldBounds.min = worldMeshRef->bounds.min;
    worldBounds.max = worldMeshRef->bounds.max;
    m_shadowHelper.update(m_camera, worldBounds, m_frameIndex);
    m_frustumCullHelper.update(m_camera.m_frustumPlanes, m_frameIndex);

    std::vector<VolumeData> volumeData;
    for (auto& vol : m_owDDGIHelper.m_probeVolumes) {
        volumeData.push_back(vol.data);
       }
    volumeData[0].pos.w = volANormalBias;
    // volumeData[1].pos.w = volBNormalBias;
    volumeData[0].spacing.w = volAViewBias;
    // volumeData[1].spacing.w = volBViewBias;
    memcpy(m_owDDGIHelper.volumeDataBuffer.pMappedData, volumeData.data(), sizeof(VolumeData) * volumeData.size());

    m_uiHelper.update(p_netEntManager->self->pistolController.currentAmmo, p_netEntManager->self->flashPercentage, p_netEntManager->self->flashNDCX, p_netEntManager->self->flashNDCY);

    // update tracers
    m_tracerManager.updateTracers(Time::getDeltaTime());
    
    std::vector<glm::mat4> matrices;
    for(int i = 0; i < m_owDDGIHelper.m_probeVolumes.size(); i++) {
        Transform t = m_owDDGIHelper.m_probeVolumes[i].transform;
        t.scale -= (glm::vec3)m_owDDGIHelper.m_probeVolumes[i].data.spacing;
        t.position += t.scale / 2.f;
        matrices.push_back(t.getMatrix());
	}
    m_owDDGIHelper.m_volumeVis.update(matrices, m_frameIndex);

    camMutex.lock();
    UBO newuniform{};
    newuniform.proj = m_camera.m_proj;
    newuniform.view = m_camera.m_view;
    newuniform.viewPos = glm::vec4(m_camera.m_transform.position, 1.f);
    newuniform.lightPos = glm::vec4(m_shadowHelper.transform.position, 1.f);
    newuniform.ABOD = glm::vec4(ambientToggle, m_shadowHelper.bias, m_shadowHelper.offsetScale, m_shadowHelper.DDGIntensity);
    newuniform.globalShadowMatrix = m_shadowHelper.shaderShadowMatrix;
    newuniform.cascadeSplits = glm::vec4(m_shadowHelper.shaderSplits[0], m_shadowHelper.shaderSplits[1], m_shadowHelper.shaderSplits[2], m_camera.m_zFar );
    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        newuniform.cascadeOffsets[i] = m_shadowHelper.cascadeOffsets[i];
        newuniform.cascadeScales[i] = m_shadowHelper.cascadeScales[i];
        newuniform.cascadeViewProj[i] = m_shadowHelper.m_cascades[i].viewProj;
    }

    memcpy(m_frameData[m_frameIndex].mappedUniformBuffer, &newuniform, sizeof(UBO));
    camMutex.unlock();

    generateRenderList();
    generateDrawCommands();
}

int HyacinthEngine::setupDraw()
{
    VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX));
    vkResetFences(m_device, 1, &m_inFlightFences[m_frameIndex]);

    VkResult res = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAcquiredSemas[m_frameIndex], VK_NULL_HANDLE, &m_swImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
        return 1;
    }

    update();

    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    vkdeviceutils::beginCommandBuffer(cmd);

    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].albedo, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].normal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].AMR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].ddgiImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].compositeImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].depth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, m_gBuffers[m_swImageIndex].stencilDepth, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_STENCIL_BIT);
    
    vkimageutils::transitionImageColorAttachment(cmd, m_swapChainImages[m_swImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, true);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_assetDrawer.g_vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, m_assetDrawer.g_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    return 0;
}

void HyacinthEngine::drawImGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (!m_showImGui) {
        return;
    }

    ImGuiID dockspace_id = ImGui::GetID("My Dockspace");
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Create settings
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
        ImGuiID dock_id_left = 0;
        ImGuiID dock_id_main = dockspace_id;
        ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, &dock_id_left, &dock_id_main);
        ImGuiID dock_id_left_top = 0;
        ImGuiID dock_id_left_bottom = 0;
        ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.50f, &dock_id_left_top, &dock_id_left_bottom);
        ImGui::DockBuilderDockWindow("Info", dock_id_left_top);
        ImGui::DockBuilderDockWindow("Properties", dock_id_left_bottom);
        ImGui::DockBuilderFinish(dockspace_id);

        ImGuiID dock_id_bottom = 0;
        ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, &dock_id_bottom, &dock_id_main);
        ImGui::DockBuilderDockWindow("Shadow Maps", dock_id_bottom);
    }

    // Submit dockspace
    ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Info");
    auto framesPerSecond = 1.0f / Time::getDeltaTime();
    ImGui::Text("rfps: %.0f", framesPerSecond);
    ImGui::Text("ft: %.2f ms", Time::getDeltaTime() * 1000.0f);

    std::stringstream s;
    s << "ID: " << p_netEntManager->self->id << "\n";
    s << "FLASH PERCENTAGE: " << p_netEntManager->self->flashPercentage;
    ImGui::Text(s.str().c_str());

    ImGui::End();

    ImGui::Begin("Properties");
    ImGui::DragFloat3("camera position", &m_camera.m_transform.position.x, 0.1f);
    ImGui::DragFloat3("light position", &m_shadowHelper.transform.position.x, 0.1f);
    ImGui::DragFloat("light intensity", &m_shadowHelper.DDGIntensity, 0.01f);
    ImGui::DragFloat("cascade split delta", &m_shadowHelper.cascadeSplitLambda, 0.01f);
    ImGui::DragFloat("cascade min distance (zNear)", &m_camera.m_zNear, 0.01f);
    ImGui::DragFloat("cascade max distance (zFar)", &m_camera.m_zFar, 0.01f);
    ImGui::DragFloat("bias", &m_shadowHelper.bias, 0.01f);
    ImGui::DragFloat("offset scale", &m_shadowHelper.offsetScale, 0.01f);

    ImGui::Checkbox("show probes", &m_owDDGIHelper.showProbes);
    ImGui::Checkbox("show probes A", &m_owDDGIHelper.showProbesA);
    ImGui::Checkbox("show probes B", &m_owDDGIHelper.showProbesB);
	ImGui::Checkbox("show volumes", &m_owDDGIHelper.showVolumes);
	ImGui::Checkbox("ambient toggle", &ambientToggle);
    ImGui::Text("Volumes");
    for(int i = 0; i < m_owDDGIHelper.m_probeVolumes.size(); i++) {
        ImGui::PushID(i);
        ImGui::DragFloat3("position", &m_owDDGIHelper.m_probeVolumes[i].transform.position.x, 0.01f);
        ImGui::DragFloat3("rotation", &m_owDDGIHelper.m_probeVolumes[i].transform.rotation.x, 0.01f);
        ImGui::DragFloat3("scale", &m_owDDGIHelper.m_probeVolumes[i].transform.scale.x, 0.01f);
        ImGui::PopID();
	}
    ImGui::DragFloat("vol a normal bias", &volANormalBias, 0.01f);
    ImGui::DragFloat("vol b normal bias", &volBNormalBias, 0.01f);
    ImGui::DragFloat("vol a view bias", &volAViewBias, 0.01f);
    ImGui::DragFloat("vol b view bias", &volBViewBias, 0.01f);
    ImGui::End();

    ImGui::Begin("Shadow Maps");
    for(int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        ImGui::Image(m_shadowHelper.m_imGuiSets[i], ImVec2(128, 128));
        if (i + 1 < SHADOW_MAP_CASCADE_COUNT)
            ImGui::SameLine();
	}
    ImGui::End();
}

void HyacinthEngine::draw() {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swImageFormat.extent.width);
    viewport.height = static_cast<float>(m_swImageFormat.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swImageFormat.extent;

    if (setupDraw() > 0) {
        return;
    }

    drawImGui();
    VkCommandBuffer cmd = getCurrentFrame().commandBuffer;

    GPUDrawPushConstants pushConstants{};
    pushConstants.renderCallBuffer = m_frameData[m_frameIndex].m_renderListBuffer.gpuAddress;
    pushConstants.materialDataAddress = m_assetDrawer.materialInfoBuffer.gpuAddress;

    // compute skin pass
    {
        VK_LABEL(cmd, "Compute Skin Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computeSkinPipeline.pipeline);
        computeSkinPushConstant cskin;
        cskin.vertexBufferInAddress = m_assetDrawer.g_vertexBuffer.gpuAddress;
        cskin.vertexBufferOutAddress = m_frameData[m_frameIndex].m_skinnedVertexBuffer.gpuAddress;
        uint32_t dstVOffset = 0;
        for (const auto& ao : m_animatedObjects) {
            if (!ao.gameObject->active) continue;
            cskin.srcVertexOffset = ao.gameObject->mesh->vertexOffset;
            cskin.dstVertexOffset = dstVOffset;
            cskin.numVertices = ao.gameObject->mesh->numVertices;
            cskin.jointBufferAddress = ao.jointMatrixBuffer.gpuAddress;

            vkCmdPushConstants(cmd, m_computeSkinPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computeSkinPushConstant), &cskin);
            vkCmdDispatch(cmd, cskin.numVertices, 1, 1);

            dstVOffset += ao.gameObject->mesh->numVertices;
        }
        for (const auto& [id, ao] : p_netEntManager->gameObjects) {
            if (!ao.gameObject->active) continue;
            cskin.srcVertexOffset = ao.gameObject->mesh->vertexOffset;
            cskin.dstVertexOffset = dstVOffset;
            cskin.numVertices = ao.gameObject->mesh->numVertices;
            cskin.jointBufferAddress = ao.jointMatrixBuffer.gpuAddress;

            vkCmdPushConstants(cmd, m_computeSkinPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computeSkinPushConstant), &cskin);
            vkCmdDispatch(cmd, cskin.numVertices, 1, 1);

            dstVOffset += ao.gameObject->mesh->numVertices;
        }
        VK_LABEL_END(cmd);
    }

    {
        VK_LABEL(cmd, "Compute Cull Main");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_frustumCullHelper.m_computeCullPipeline.pipeline);
        m_frustumCullHelper.executeCull(cmd, m_frustumCullHelper.m_computeSets[m_frameIndex], m_frameData[m_frameIndex].m_indirectDrawBuffer.gpuAddress, m_frameData[m_frameIndex].m_renderListBuffer.gpuAddress, numStaticDrawCommands);
        VK_LABEL_END(cmd);
        // for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        //     VK_LABEL(cmd, "Compute Cull Shadow");
        //     m_frustumCullHelper.executeCull(cmd, m_shadowHelper.m_cascades[i].cascadeCullDescriptorSets[m_frameIndex], m_shadowHelper.m_cascades[i].cascadeDrawBuffer.gpuAddress, m_frameData[m_frameIndex].m_renderListBuffer.gpuAddress, numStaticDrawCommands);
        //     VK_LABEL_END(cmd);
        // }
    }

    // std::vector<VkBufferMemoryBarrier2> shadowBufferBarrier;
    // for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    //     // barrier for skin and cull write out
    //     VkBufferMemoryBarrier2 barrier{};
    //     barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    //     barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    //     barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    //     barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    //     barrier.size = VK_WHOLE_SIZE;
    //     barrier.buffer = m_shadowHelper.m_cascades[i].cascadeDrawBuffer.buffer;
    //     shadowBufferBarrier.push_back(barrier);
    // }
    // 
    // VkDependencyInfo depInfo{};
    // depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    // depInfo.bufferMemoryBarrierCount = SHADOW_MAP_CASCADE_COUNT;
    // depInfo.pBufferMemoryBarriers = shadowBufferBarrier.data();
    // 
    // vkCmdPipelineBarrier2(cmd, &depInfo); // barrier for shadow cascades

    // shadows
    {
        m_shadowHelper.drawShadowMaps(cmd, numStaticDrawCommands, numDynamicDrawCommands, m_frameIndex, m_frameData[m_frameIndex].m_renderListBuffer.gpuAddress, m_assetDrawer.g_vertexBuffer, m_frameData[m_frameIndex].m_skinnedVertexBuffer);
    }

    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    barrier.size = VK_WHOLE_SIZE;
    barrier.buffer = m_frameData[m_frameIndex].m_indirectDrawBuffer.buffer;

    VkDependencyInfo depInfoMain{};
    depInfoMain.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfoMain.bufferMemoryBarrierCount = 1;
    depInfoMain.pBufferMemoryBarriers = &barrier;
    
    vkCmdPipelineBarrier2(cmd, &depInfoMain); // barrier for main draw pass

    // depth prepass
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipelineUtil.m_pipeline.pipeline);
        VkRenderingAttachmentInfo depthAttachment = vkimageutils::createDepthAttachmentInfo(m_gBuffers[m_swImageIndex].depth.imageView);
        VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 0, nullptr, &depthAttachment);
        VK_LABEL(cmd, "Depth Pre Pass");
        vkCmdBeginRendering(cmd, &renderingInfo);

        std::array<VkDescriptorSet, 2> sets = { m_frameData[m_frameIndex].uniformDescriptorSet, m_textureSet };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, m_depthPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // draw world
        vkCmdDrawIndexedIndirect(cmd, m_frameData[m_frameIndex].m_indirectDrawBuffer.buffer, 0, numStaticDrawCommands, sizeof(VkDrawIndexedIndirectCommand));

        // draw animated objects
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_frameData[m_frameIndex].m_skinnedVertexBuffer.buffer, offsets);
        vkCmdDrawIndexedIndirect(cmd, m_frameData[m_frameIndex].m_indirectDrawBuffer.buffer, sizeof(VkDrawIndexedIndirectCommand) * numStaticDrawCommands, numDynamicDrawCommands, sizeof(VkDrawIndexedIndirectCommand));

        vkCmdBindVertexBuffers(cmd, 0, 1, &m_assetDrawer.g_vertexBuffer.buffer, offsets);

        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

    // main render pass
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineUtil.m_pipeline.pipeline);
        VkRenderingAttachmentInfo albedoAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].albedo.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo normalAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].normal.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo amrattachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].AMR.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = vkimageutils::createDepthAttachmentInfo(m_gBuffers[m_swImageIndex].depth.imageView, false);
        std::array<VkRenderingAttachmentInfo, 3> colorAttachments = { albedoAttachment, normalAttachment, amrattachment };
        VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 3, colorAttachments.data(), &depthAttachment);
        VK_LABEL(cmd, "G Buffer Pass");
        vkCmdBeginRendering(cmd, &renderingInfo);

        std::array<VkDescriptorSet, 3> sets = { m_frameData[m_frameIndex].uniformDescriptorSet, m_frameData[m_frameIndex].shadowDescriptorSet, m_textureSet };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

        vkCmdPushConstants(cmd, m_pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // draw world
        vkCmdDrawIndexedIndirect(cmd, m_frameData[m_frameIndex].m_indirectDrawBuffer.buffer, 0, numStaticDrawCommands, sizeof(VkDrawIndexedIndirectCommand));

        // draw animated objects
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_frameData[m_frameIndex].m_skinnedVertexBuffer.buffer, offsets);
        vkCmdDrawIndexedIndirect(cmd, m_frameData[m_frameIndex].m_indirectDrawBuffer.buffer, sizeof(VkDrawIndexedIndirectCommand) * numStaticDrawCommands, numDynamicDrawCommands, sizeof(VkDrawIndexedIndirectCommand));

        vkCmdBindVertexBuffers(cmd, 0, 1, &m_assetDrawer.g_vertexBuffer.buffer, offsets);

        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

    vkimageutils::transitionImageDepthRead(cmd, m_gBuffers[m_swImageIndex].depth);
    vkimageutils::transitionImageShaderRead(cmd, m_gBuffers[m_swImageIndex].albedo, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageShaderRead(cmd, m_gBuffers[m_swImageIndex].normal, VK_IMAGE_ASPECT_COLOR_BIT);

    {
        VK_LABEL(cmd, "AO Pass");
        m_ambientHelper.drawAO(cmd, m_gBuffers[m_swImageIndex].m_compositeSet, m_frameData[m_frameIndex].uniformDescriptorSet, m_gBuffers[m_swImageIndex].AMR, m_swImageFormat);
        VK_LABEL_END(cmd);
    }

    vkimageutils::transitionImageShaderRead(cmd, m_gBuffers[m_swImageIndex].AMR, VK_IMAGE_ASPECT_COLOR_BIT);

    {
        VK_LABEL(cmd, "DDGI Pass");

        ComputePushConstant ddgiPushConstant{};
        ddgiPushConstant.volumeDataAddress = m_owDDGIHelper.volumeDataBuffer.gpuAddress;

        for (int i = static_cast<int>(m_owDDGIHelper.m_probeVolumes.size() - 1); i >= 0; i--) {
            ddgiPushConstant.volumeIndex = i;

            volumeStencilPushConstant vsPushConstant{};
            vsPushConstant.volumeTransformAddress = m_owDDGIHelper.m_volumeVis.volumeTransformBuffers[m_frameIndex].gpuAddress;
            vsPushConstant.volumeIndex = i;
     
            VK_LABEL(cmd, "Stencil Volume");
            // bind stencil pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_volumeStencilPipeline.m_pipeline.pipeline);
            VkRenderingAttachmentInfo stencilAttachmentInfo = vkimageutils::createStencilAttachmentInfo(m_gBuffers[m_swImageIndex].stencilDepth.imageView, (i == m_owDDGIHelper.m_probeVolumes.size() - 1));
            VkRenderingInfo volumeStencilRenderingInfo = vkdeviceutils::createStencilRenderingInfo(m_swImageFormat.extent, &stencilAttachmentInfo);
            vkCmdBeginRendering(cmd, &volumeStencilRenderingInfo);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            std::array<VkDescriptorSet, 2> stencilSets = { m_frameData[m_frameIndex].uniformDescriptorSet, m_gBuffers[m_swImageIndex].m_compositeSet };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_volumeStencilPipeline.m_pipeline.layout, 0, static_cast<uint32_t>(stencilSets.size()), stencilSets.data(), 0, nullptr);
            vkCmdPushConstants(cmd, m_volumeStencilPipeline.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ComputePushConstant), &vsPushConstant);
            // draw volume to stencil buffer
            vkCmdDrawIndexed(cmd, UNIT_CUBE_INDEX_COUNT, 1, QUAD_INDEX_COUNT, QUAD_VERTEX_COUNT, 0);
            vkCmdEndRendering(cmd);
            VK_LABEL_END(cmd);
     
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ddgiPipelineUtil.m_pipeline.pipeline);
            VkRenderingAttachmentInfo ddgiAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].ddgiImage.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (i == m_owDDGIHelper.m_probeVolumes.size() - 1));
            VkRenderingAttachmentInfo stencilAttachment = vkimageutils::createStencilAttachmentInfo(m_gBuffers[m_swImageIndex].stencilDepth.imageView, false);
            VkRenderingInfo ddgiRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &ddgiAttachment, nullptr);
            ddgiRenderingInfo.pStencilAttachment = &stencilAttachment;
            vkCmdBeginRendering(cmd, &ddgiRenderingInfo);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
    
            std::array<VkDescriptorSet, 3> ddgiSets = { m_gBuffers[m_swImageIndex].m_compositeSet, m_frameData[m_frameIndex].uniformDescriptorSet, m_owDDGIHelper.m_probeVolumes[i].irradianceVisSet };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ddgiPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(ddgiSets.size()), ddgiSets.data(), 0, nullptr);
            vkCmdPushConstants(cmd, m_ddgiPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ComputePushConstant), &ddgiPushConstant);
            vkCmdDrawIndexed(cmd, QUAD_INDEX_COUNT, 1, 0, 0, 0);
            vkCmdEndRendering(cmd);
        }
        VK_LABEL_END(cmd);
    }

    vkimageutils::transitionImageShaderRead(cmd, m_gBuffers[m_swImageIndex].ddgiImage, VK_IMAGE_ASPECT_COLOR_BIT);

    { 
        VK_LABEL(cmd, "Skybox Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxHelper.m_skyboxPipelineUtil.m_pipeline.pipeline);
    
        std::array<VkDescriptorSet, 2> sets = { m_frameData[m_frameIndex].uniformDescriptorSet, m_skyboxHelper.m_skyboxSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxHelper.m_skyboxPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
        VkRenderingAttachmentInfo skyboxAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].compositeImage.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
        VkRenderingInfo skyboxRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &skyboxAttachment, nullptr);
        vkCmdBeginRendering(cmd, &skyboxRenderingInfo);
        m_skyboxHelper.drawSkybox(cmd);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

    {
        VK_LABEL(cmd, "Composite Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipelineUtil.m_pipeline.pipeline);
        std::array<VkDescriptorSet, 2> compositeSets = { m_gBuffers[m_swImageIndex].m_compositeSet, m_frameData[m_frameIndex].uniformDescriptorSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(compositeSets.size()), compositeSets.data(), 0, nullptr);
        VkRenderingAttachmentInfo compositeAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].compositeImage.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
        VkRenderingInfo compositeRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &compositeAttachment, nullptr);
        vkCmdBeginRendering(cmd, &compositeRenderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDrawIndexed(cmd, QUAD_INDEX_COUNT, 1, 0, 0, 0);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

    vkimageutils::transitionDepthBackToWrite(cmd, m_gBuffers[m_swImageIndex].depth);
    
    {
        VK_LABEL(cmd, "Health Bars Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_worldHealthManager.worldUIPipelineUtil.m_pipeline.pipeline);
        std::array<VkDescriptorSet, 2> healthSets = { m_textureSet, m_frameData[m_frameIndex].uniformDescriptorSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_worldHealthManager.worldUIPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(healthSets.size()), healthSets.data(), 0, nullptr);
        VkRenderingAttachmentInfo healthBarAttachment = vkimageutils::createColorAttachmentInfo(m_gBuffers[m_swImageIndex].compositeImage.imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
        VkRenderingAttachmentInfo healthDepthAttachment = vkimageutils::createDepthAttachmentInfo(m_gBuffers[m_swImageIndex].depth.imageView, false);
        VkRenderingInfo healthBarRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &healthBarAttachment, &healthDepthAttachment);
        vkCmdBeginRendering(cmd, &healthBarRenderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        m_worldHealthManager.draw(cmd);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

    vkimageutils::transitionImageShaderRead(cmd, m_gBuffers[m_swImageIndex].compositeImage, VK_IMAGE_ASPECT_COLOR_BIT);

    {
        glm::vec2 invSS = 1.f / glm::vec2(m_swImageFormat.extent.width, m_swImageFormat.extent.height);
        VK_LABEL(cmd, "FXAA Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fxaaPipelineUtil.m_pipeline.pipeline);
        std::array<VkDescriptorSet, 1> fxaaSet = { m_gBuffers[m_swImageIndex].m_postProcessSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fxaaPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(fxaaSet.size()), fxaaSet.data(), 0, nullptr);
        VkRenderingAttachmentInfo fxaaAttachment = vkimageutils::createColorAttachmentInfo(m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
        VkRenderingInfo fxaaRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &fxaaAttachment, nullptr);
        vkCmdPushConstants(cmd, m_fxaaPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec2), &invSS);
        vkCmdBeginRendering(cmd, &fxaaRenderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDrawIndexed(cmd, QUAD_INDEX_COUNT, 1, 0, 0, 0);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }
    
    {
        VK_LABEL(cmd, "UI Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiHelper.uiPipelineUtil.m_pipeline.pipeline);
        std::array<VkDescriptorSet, 1> uiSets = { m_textureSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiHelper.uiPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(uiSets.size()), uiSets.data(), 0, nullptr);
        VkRenderingAttachmentInfo uiAttachment = vkimageutils::createColorAttachmentInfo(m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
        VkRenderingInfo uiRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &uiAttachment, nullptr);
        vkCmdBeginRendering(cmd, &uiRenderingInfo);
        m_uiHelper.draw(cmd);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }

#ifdef DEBUG_NETWORK
    {
        VK_LABEL(cmd, "Network Entity Debug Pass");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_netDebugRenderer.pipelineUtil.m_pipeline.pipeline);
        VkRenderingAttachmentInfo netDebugAttachment = vkimageutils::createColorAttachmentInfo(m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
        VkRenderingInfo netDebugRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &netDebugAttachment, nullptr);
        vkCmdBeginRendering(cmd, &netDebugRenderingInfo);
        m_netDebugRenderer.draw(cmd, m_frameData[m_frameIndex].uniformDescriptorSet);
        vkCmdEndRendering(cmd);
        VK_LABEL_END(cmd);
    }
#endif

    if (m_owDDGIHelper.showProbes || m_owDDGIHelper.showVolumes) {
        VkRenderingAttachmentInfo visInfo = vkimageutils::createColorAttachmentInfo(m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
	 	VkRenderingAttachmentInfo depthVisInfo = vkimageutils::createDepthAttachmentInfo(m_gBuffers[m_swImageIndex   ].depth.imageView, false);
        VkRenderingInfo visRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &visInfo, &depthVisInfo);
        vkCmdBeginRendering(cmd, &visRenderingInfo);
        if (m_owDDGIHelper.showProbes) {
            for (int i = 0; i < m_owDDGIHelper.m_probeVolumes.size(); i++) {
                if (i == 0) if (!m_owDDGIHelper.showProbesA) continue;
                if (i == 1) if (!m_owDDGIHelper.showProbesB) continue;
                m_owDDGIHelper.m_probeVis.drawProbes(cmd, m_owDDGIHelper.m_probeVolumes[i].irradianceVisSet, m_owDDGIHelper.m_probeVolumes[i].probePositionBuffer.gpuAddress, m_frameData[m_frameIndex].uniformDescriptorSet, m_owDDGIHelper.m_probeVolumes[i].totalNumProbes, m_owDDGIHelper.m_probeVolumes[i].data.densityWidth, m_owDDGIHelper.m_probeVolumes[i].data.densityHeight, m_owDDGIHelper.m_probeVolumes[i].data.densityDepth);
            }
        }
    
        if (m_owDDGIHelper.showVolumes) {
            m_owDDGIHelper.m_volumeVis.drawVolumes(cmd, m_frameData[m_frameIndex].uniformDescriptorSet, m_frameIndex, static_cast<uint32_t>(m_owDDGIHelper.m_probeVolumes.size()));
        }
        vkCmdEndRendering(cmd);
    }
    
    VkRenderingAttachmentInfo imguiAttachment = vkimageutils::createColorAttachmentInfo(m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
    VkRenderingInfo imguiRenderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, 1, &imguiAttachment, nullptr);
    vkCmdBeginRendering(cmd, &imguiRenderingInfo);
    VK_LABEL(cmd, "ImGui Pass");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    VK_LABEL_END(cmd);
    vkCmdEndRendering(cmd);

    vkimageutils::transitionImagePresent(cmd, m_swapChainImages[m_swImageIndex]);
        
    endDraw();
}

void HyacinthEngine::endDraw()
{
    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkEndCommandBuffer(cmd));

    // submit the command buffer to the graphics queue
    VkSemaphoreSubmitInfo waitSemaSubmitInfo{};
    waitSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaSubmitInfo.semaphore = m_imageAcquiredSemas[m_frameIndex];
    waitSemaSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signalSemaSubmitInfo{};
    signalSemaSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaSubmitInfo.semaphore = m_imageFinishedSemas[m_frameIndex];
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
    presentInfo.pWaitSemaphores = &m_imageFinishedSemas[m_frameIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &m_swImageIndex;

    VkResult res = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HyacinthEngine::recreateSwapchain() {
    vkDeviceWaitIdle(m_device);

    for (int i = 0; i < m_swapChainImages.size(); i++) {
        vkimageutils::destroyImage(m_gBuffers[i].albedo);
        vkimageutils::destroyImage(m_gBuffers[i].normal);
        vkimageutils::destroyImage(m_gBuffers[i].AMR);
        vkimageutils::destroyImage(m_gBuffers[i].depth);
        vkimageutils::destroyImage(m_gBuffers[i].ddgiImage);
        vkimageutils::destroyImage(m_gBuffers[i].stencilDepth);
        vkimageutils::destroyImage(m_gBuffers[i].compositeImage);
    }

    for (VulkanImage& img : m_swapChainImages) {
        vkDestroyImageView(m_device, img.imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	createSwapchain();
    createColorImages();

    for (int i = 0; i < m_swapChainImages.size(); i++) {
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].AMR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].depth, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_compositeSet, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].ddgiImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkdescriptorutils::queueWriteImage(m_gBuffers[i].m_postProcessSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_gBuffers[i].compositeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    vkdescriptorutils::flushDescriptorWrites();

    m_uiHelper.onresize(static_cast<uint32_t>(DUMMY_TEX_PATHS.size()), glm::vec2(m_swImageFormat.extent.width, m_swImageFormat.extent.height));
}

void HyacinthEngine::shutdown()
{
    if (!m_initialized) {
        return;
	}

	vkDeviceWaitIdle(m_device);

    for (auto& ao : m_animatedObjects) {
        ao.gameObject->destroy();
        vkdeviceutils::destroyBuffer(ao.jointMatrixBuffer);
    }

    m_frustumCullHelper.shutdown();
	m_shadowHelper.shutdown();
	m_owDDGIHelper.shutdown();
    m_rtHelper.shutdown();
    m_uiHelper.shutdown();
    m_worldHealthManager.shutdown();
    m_skyboxHelper.shutdown();
    m_assetDrawer.shutdown();
    m_ambientHelper.shutdown();

#ifdef DEBUG_NETWORK
    m_netDebugRenderer.shutdown();
#endif

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

	vkDestroyFence(m_device, m_uploadFence, nullptr);

    m_pipelineUtil.destroyPipeline();            
    m_depthPipelineUtil.destroyPipeline();
    m_compositePipelineUtil.destroyPipeline();
    m_ddgiPipelineUtil.destroyPipeline();
    m_volumeStencilPipeline.destroyPipeline();
    m_skinnedPipelineUtil.destroyPipeline();
    m_tracerPipelineUtil.destroyPipeline();
    m_fxaaPipelineUtil.destroyPipeline();
    m_computeSkinPipeline.destroy();

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemas[i], nullptr);
		vkDestroySemaphore(m_device, m_imageFinishedSemas[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);

        vkDestroyCommandPool(m_device, m_frameData[i].commandPool, nullptr);

		vkdeviceutils::destroyBuffer(m_frameData[i].uniformBuffer);
        vkdeviceutils::destroyBuffer(m_frameData[i].m_renderListBuffer);
        vkdeviceutils::destroyBuffer(m_frameData[i].m_indirectDrawBuffer);
        vkdeviceutils::destroyBuffer(m_shadowHelper.m_uniformBuffers[i]);
        vkdeviceutils::destroyBuffer(m_frameData[i].m_skinnedVertexBuffer);
	}

    for (int i = 0; i < m_swapChainImages.size(); i++) {
        vkimageutils::destroyImage(m_gBuffers[i].albedo);
        vkimageutils::destroyImage(m_gBuffers[i].normal);
        vkimageutils::destroyImage(m_gBuffers[i].AMR);
        vkimageutils::destroyImage(m_gBuffers[i].depth);
        vkimageutils::destroyImage(m_gBuffers[i].ddgiImage);
        vkimageutils::destroyImage(m_gBuffers[i].stencilDepth);
        vkimageutils::destroyImage(m_gBuffers[i].compositeImage);
    }

	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_textureSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_compositeSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_postProcessSetLayout, nullptr);

    vmaDestroyAllocator(m_allocator);

	vkDestroyCommandPool(m_device, m_uploadFrame.commandPool, nullptr);

    for (VulkanImage& img : m_swapChainImages) {
        vkDestroyImageView(m_device, img.imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

    vkDestroyDevice(m_device, nullptr);
    
#ifndef NDEBUG
    vkdebugutils::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    
    vkDestroyInstance(m_instance, nullptr);
}
