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

    VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    m_rtHelper.m_rtProperties.pNext = &m_rtHelper.m_asProperties;
    prop2.pNext = &m_rtHelper.m_rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
    vkimageutils::setLinear(formatProperties.optimalTilingFeatures);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    vkimageutils::setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);

    m_msaaSamples = vkdeviceutils::getMaxUsableSampleCount(m_physicalDevice);

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
        m_swapChainImages[i].imageView = vkimageutils::createImageView(m_swapChainImages[i], 0, 1, VK_IMAGE_ASPECT_COLOR_BIT);
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
    m_depthImages.resize(numImages);
    m_colorImages.resize(numImages);
    m_depthResolveImages.resize(numImages);
    for (int i = 0; i < numImages; i++) {
        m_depthImages[i] = vkimageutils::createImageandView(extent, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_msaaSamples, false, "depth_image");
        m_depthResolveImages[i] = vkimageutils::createImageandView(extent, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false, "depth_resolve");
        m_colorImages[i] = vkimageutils::createImageandView(extent, 1, m_swImageFormat.format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_msaaSamples, false, "color_image");
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

void HyacinthEngine::createGraphicsPipeline()
{
    m_pipelineUtil.addShader("shaders/vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_pipelineUtil.addShader("shaders/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_pipelineUtil.setDefaultAttributes();
	m_pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_pipelineUtil.setColorAttachmentFormat(m_swImageFormat.format);
    m_pipelineUtil.setMultisampling(m_msaaSamples);
	m_pipelineUtil.disableBlending();

    m_pipelineUtil.enableDepthTest(false, VK_COMPARE_OP_LESS_OR_EQUAL);
    m_pipelineUtil.setDepthAttachmentFormat(m_depthImages[0].imageFormat);

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

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 3> sets = { m_descriptorSetLayout, m_textureSetLayout, m_owDDGIHelper.m_probeVis.visSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = sets.size();
	pipelineLayoutCInfo.pSetLayouts = sets.data();

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_pipelineUtil.m_pipeline.layout));

	m_pipelineUtil.buildPipeline();
}

void HyacinthEngine::createDepthPipeline()
{
    m_depthPipelineUtil.addShader("shaders/depth.spv", VK_SHADER_STAGE_VERTEX_BIT);

    m_depthPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_depthPipelineUtil.setPositionAttribute();
    m_depthPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_depthPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    m_depthPipelineUtil.setMultisampling(m_msaaSamples);
    m_depthPipelineUtil.disableBlending();

    m_depthPipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    m_depthPipelineUtil.setDepthAttachmentFormat(m_depthImages[0].imageFormat);

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

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
    pipelineLayoutCInfo.setLayoutCount = 1;
    pipelineLayoutCInfo.pSetLayouts = &m_descriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_depthPipelineUtil.m_pipeline.layout));

    m_depthPipelineUtil.buildPipeline();
}

void HyacinthEngine::loadScene() {
    // auto path = "C:/Users/ajnkr/Documents/Hyacinth/Hyacinth/objects/test_scene.glb";
    // auto path = vkdebugutils::getExeDir() / "objects" / "test_scene.glb";
    auto path = vkdebugutils::getExeDir() / "objects" / "sponza" / "sponza.gltf";
    // auto path = "C:/Users/ajnkr/Documents/Orchid/Sandbox/trainStation/station.gltf";
    // auto path2 = vkdebugutils::getExeDir() / "objects" / "SM_Deccer_Cubes_Textured_Complex.glb";
    // auto path = vkdebugutils::getExeDir() / "objects" / "bistro.glb";

    m_scene.objects.push_back(gltfutils::loadFromFile(path.string(), true));
    // m_scene.objects.push_back(gltfutils::loadFromFile(path2.string(), m_devContext));
    m_scene.buildSceneGraph();
    m_meshBuffers = vkmeshutils::uploadMesh(m_scene.indices, m_scene.vertices, m_scene.boundingBoxes);
    m_scene.createDummyTextures();
}

void HyacinthEngine::createBuffers() {
    size_t drawDataBufferSize = sizeof(DrawData) * m_scene.drawData.size();
    m_drawDataBuffer = vkdeviceutils::createBuffer(drawDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "draw_data_ssbo");
    vkdeviceutils::uploadToBuffer(m_drawDataBuffer, drawDataBufferSize, m_scene.drawData.data());

    size_t materialDataBufferSize = sizeof(GPUMaterialIndices) * m_scene.materialObjects.size();
    m_materialBuffer = vkdeviceutils::createBuffer(materialDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "material_ssbo");
    vkdeviceutils::uploadToBuffer(m_materialBuffer, materialDataBufferSize, m_scene.materialObjects.data());

	size_t drawCmdBufferSize = sizeof(VkDrawIndexedIndirectCommand) * m_scene.drawCommands.size();
    m_indirectDrawBuffer = vkdeviceutils::createBuffer(drawCmdBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "indirect_ssbo");
	vkdeviceutils::uploadToBuffer(m_indirectDrawBuffer, drawCmdBufferSize, m_scene.drawCommands.data());
    
    vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer cmd) {
        for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
            m_shadowHelper.m_cascades[i].cascadeDrawBuffer = vkdeviceutils::createBuffer(drawCmdBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "indirect_shadow_ssbo");

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = drawCmdBufferSize;
			vkCmdCopyBuffer(cmd, m_indirectDrawBuffer.buffer, m_shadowHelper.m_cascades[i].cascadeDrawBuffer.buffer, 1, &copyRegion);
        }
    });

	size_t matrixBuffSize = sizeof(glm::mat4) * m_scene.transformMatrices.size();
    m_worldMatrixBuffer = vkdeviceutils::createBuffer(matrixBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "world_mat_ssbo");
	vkdeviceutils::uploadToBuffer(m_worldMatrixBuffer, matrixBuffSize, m_scene.transformMatrices.data());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].uniformBuffer = vkdeviceutils::createBuffer(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "frame_uniform");
        m_frameData[i].mappedUniformBuffer = m_frameData[i].uniformBuffer.info.pMappedData;
    }
}

void HyacinthEngine::createDescriptorSets()
{
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (((float) (m_scene.numTextures + 2)) / (float) MAX_FRAMES_IN_FLIGHT) }, // divided because multiplied by maxSets later. we only want 1 descriptor per texture, not 3
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (float) (1.f / MAX_FRAMES_IN_FLIGHT) } // for shadows, only 1 (not 3)
    };
    m_descriptorAllocator.initPool(MAX_FRAMES_IN_FLIGHT * 2, sizes);

    {
		DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // shadow map
		m_descriptorSetLayout = layoutBuilder.buildLayout(nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, m_scene.numTextures + 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        m_textureSetLayout = layoutBuilder.buildLayout(nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }
	m_textureSet = m_descriptorAllocator.allocate(m_textureSetLayout);
    m_scene.uploadTextures(m_textureSet);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].descriptorSet = m_descriptorAllocator.allocate(m_descriptorSetLayout);

        vkdescriptorutils::queueWriteBuffer(m_frameData[i].descriptorSet, 0, sizeof(UBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frameData[i].uniformBuffer);
        vkdescriptorutils::queueWriteImage(m_frameData[i].descriptorSet, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_shadowHelper.m_shadowImage, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
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
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physicalDevice;
    init_info.Device = m_device;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = m_pipelineUtil.m_renderInfo;
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

void HyacinthEngine::init()
{
	createInstance();

	createSwapchain();

	createCommandBuffers();

	createSyncObjects(); // also creates device context

    m_camera = Camera(m_swImageFormat.aspectRatio, 90.f, 0.01f, 150.f);

    createColorImages();

    loadScene();

    m_frustumCullHelper.setup();

    m_shadowHelper.setup(MAX_FRAMES_IN_FLIGHT, m_frustumCullHelper.m_computeLayout);

    createBuffers();

    createDescriptorSets();

    m_rtHelper.setup(m_scene);

    m_owDDGIHelper.setup(&m_rtHelper, m_scene);
    m_owDDGIHelper.m_probeVis.createProbeVisualizationStructures(m_descriptorSetLayout, m_owDDGIHelper.m_probeVolumes[0].irradianceImage, m_owDDGIHelper.m_probeVolumes[0].visibilityImage, m_depthImages[0].imageFormat, m_swImageFormat, m_msaaSamples);
	m_owDDGIHelper.m_volumeVis.createVolumeVisualizationStructures(m_descriptorSetLayout, m_depthImages[0].imageFormat, m_swImageFormat, m_msaaSamples);

    createGraphicsPipeline();

    createDepthPipeline();

    setupImGUI();

    m_shadowHelper.setupImGui();

    m_owDDGIHelper.bakeDDGI(m_textureSet);

    m_initialized = true;
}

void HyacinthEngine::update() {
    if (InputManager::tabKeyDown()) {
		m_showImGui = !m_showImGui;
    }

    m_camera.update(Time::getDeltaTime(), mouseLocked, m_frameIndex);
    m_shadowHelper.update(m_camera, m_frameIndex);
    m_frustumCullHelper.update(m_camera.m_frustumPlanes, m_frameIndex);

    std::vector<glm::mat4> matrices;
    for(int i = 0; i < m_owDDGIHelper.m_probeVolumes.size(); i++) {
        matrices.push_back(m_owDDGIHelper.m_probeVolumes[i].transform.getMatrix());
	}
    m_owDDGIHelper.m_volumeVis.update(matrices, m_frameIndex);

    UBO newuniform{};
    newuniform.proj = m_camera.m_proj;
    newuniform.view = m_camera.m_view;
    newuniform.viewPos = glm::vec4(m_camera.m_transform.position, ambientToggle);
    newuniform.lightPos = glm::vec4(m_shadowHelper.transform.position, m_shadowHelper.DDGIntensity);
    newuniform.cascadeSplits = glm::vec4(m_shadowHelper.m_cascades[0].splitDepth, m_shadowHelper.m_cascades[1].splitDepth, m_shadowHelper.m_cascades[2].splitDepth, m_shadowHelper.m_cascades[2].splitDepth);
    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        newuniform.cascadeViewProj[i] = m_shadowHelper.m_cascades[i].viewProj;
    }

    memcpy(m_frameData[m_frameIndex].mappedUniformBuffer, &newuniform, sizeof(UBO));
}

void HyacinthEngine::setupDraw()
{
    VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX));
    vkResetFences(m_device, 1, &m_inFlightFences[m_frameIndex]);

    VkResult res2 = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAcquiredSemas[m_frameIndex], VK_NULL_HANDLE, &m_swImageIndex);

    update();

    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    vkdeviceutils::beginCommandBuffer(cmd);

    vkimageutils::transitionImage(cmd, m_colorImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImage(cmd, m_swapChainImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkimageutils::transitionImage(cmd, m_depthImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vkimageutils::transitionImage(cmd, m_depthResolveImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_meshBuffers.vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, m_meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
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
    ImGui::End();

    ImGui::Begin("Properties");
    ImGui::DragFloat3("camera position", &m_camera.m_transform.position.x, 0.1f);
    ImGui::DragFloat3("light position", &m_shadowHelper.transform.position.x, 0.1f);
    ImGui::DragFloat("light intensity", &m_shadowHelper.DDGIntensity, 0.01f);
    ImGui::DragFloat("cascade split delta", &m_shadowHelper.cascadeSplitLambda, 0.01f);
    ImGui::DragFloat("cascade min distance (zNear)", &m_camera.m_zNear, 0.01f);
    ImGui::DragFloat("cascade max distance (zFar)", &m_camera.m_zFar, 0.01f);
    ImGui::Checkbox("show probes", &m_owDDGIHelper.showProbes);
	ImGui::Checkbox("show volumes", &m_owDDGIHelper.showVolumes);
	ImGui::Checkbox("ambient toggle", &ambientToggle);
    ImGui::Text("Volumes");
    for(int i = 0; i < m_owDDGIHelper.m_probeVolumes.size(); i++) {
        ImGui::PushID(i);
        ImGui::DragFloat3("position", &m_owDDGIHelper.m_probeVolumes[i].transform.position.x, 0.1f);
        ImGui::DragFloat3("rotation", &m_owDDGIHelper.m_probeVolumes[i].transform.rotation.x, 0.1f);
        ImGui::DragFloat3("scale", &m_owDDGIHelper.m_probeVolumes[i].transform.scale.x, 0.1f);
        ImGui::PopID();
	}
    ImGui::End();

    ImGui::Begin("Shadow Maps");
    for(int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        ImGui::Image(m_shadowHelper.m_imGuiSets[i], ImVec2(128, 128));
        if (i + 1 < SHADOW_MAP_CASCADE_COUNT)
            ImGui::SameLine();
	}
    ImGui::End();
}

void HyacinthEngine::draw()
{
    GPUDrawPushConstants pushConstants{};
    pushConstants.transformAddress = m_worldMatrixBuffer.gpuAddress;
    pushConstants.materialAddress = m_materialBuffer.gpuAddress;
    pushConstants.drawDataAddress = m_drawDataBuffer.gpuAddress;
    pushConstants.probePositionAddress = m_owDDGIHelper.m_probeVolumes[0].probePositionBuffer.gpuAddress;
    pushConstants.volumeDataAddress = m_owDDGIHelper.volumeDataBuffer.gpuAddress;
    pushConstants.volumeIndex = 0;

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

    uint32_t numDraws = static_cast<uint32_t>(m_scene.drawCommands.size());

    drawImGui();

    setupDraw();
    VkCommandBuffer cmd = getCurrentFrame().commandBuffer;

    VK_LABEL(cmd, "Compute Cull Main");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_frustumCullHelper.m_computeCullPipeline.pipeline);
    m_frustumCullHelper.executeCull(cmd, m_frustumCullHelper.m_computeSets[m_frameIndex], m_indirectDrawBuffer.gpuAddress, m_meshBuffers.aabbBuffer.gpuAddress, m_worldMatrixBuffer.gpuAddress, m_drawDataBuffer.gpuAddress, numDraws);
    VK_LABEL_END(cmd);
    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        VK_LABEL(cmd, "Compute Cull Shadow");
        m_frustumCullHelper.executeCull(cmd, m_shadowHelper.m_cascades[i].cascadeCullDescriptorSets[m_frameIndex], m_shadowHelper.m_cascades[i].cascadeDrawBuffer.gpuAddress, m_meshBuffers.aabbBuffer.gpuAddress, m_worldMatrixBuffer.gpuAddress, m_drawDataBuffer.gpuAddress, numDraws);
        VK_LABEL_END(cmd);
    }

    // shadows
    m_shadowHelper.drawShadowMaps(cmd, numDraws, m_frameIndex, m_worldMatrixBuffer.gpuAddress, m_drawDataBuffer.gpuAddress);

    VK_LABEL(cmd, "Depth Pre-Pass");
    // depth pre-pass
    VkRenderingAttachmentInfo depthAttachment = vkimageutils::createDepthAttachmentInfo(m_depthImages[m_swImageIndex].imageView, m_depthResolveImages[m_swImageIndex].imageView);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipelineUtil.m_pipeline.pipeline);
    VkRenderingInfo depthRenderingInfo = vkdeviceutils::createDepthRenderingInfo(m_swImageFormat.extent, &depthAttachment);
    vkCmdBeginRendering(cmd, &depthRenderingInfo);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipelineUtil.m_pipeline.layout, 0, 1, &m_frameData[m_frameIndex].descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_depthPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer.buffer, 0, static_cast<uint32_t>(m_scene.drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));
    vkCmdEndRendering(cmd);
	VK_LABEL_END(cmd);

    // main render pass
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineUtil.m_pipeline.pipeline);
    VkRenderingAttachmentInfo colorAttachment = vkimageutils::createColorAttachmentInfo(m_colorImages[m_swImageIndex].imageView, m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, &colorAttachment, &depthAttachment);
    VK_LABEL(cmd, "Main Render Pass");
    vkCmdBeginRendering(cmd, &renderingInfo);

    std::array<VkDescriptorSet, 3> sets = { m_frameData[m_frameIndex].descriptorSet, m_textureSet, m_owDDGIHelper.m_probeVis.visSet };

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineUtil.m_pipeline.layout,
        0,
        sets.size(),
        sets.data(),
        0,
        nullptr
    );

    vkCmdPushConstants(cmd, m_pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer.buffer, 0, static_cast<uint32_t>(m_scene.drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));

    if (m_owDDGIHelper.showProbes) {
        m_owDDGIHelper.m_probeVis.drawProbes(cmd, m_owDDGIHelper.m_probeVolumes[0].probePositionBuffer.gpuAddress, m_frameData[m_frameIndex].descriptorSet);
    }
    
    if (m_owDDGIHelper.showVolumes) {
		m_owDDGIHelper.m_volumeVis.drawVolumes(cmd, m_frameData[m_frameIndex].descriptorSet, m_frameIndex);
    }

    VK_LABEL_END(cmd);

    endDraw();
}

void HyacinthEngine::endDraw()
{
    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;

    VK_LABEL(cmd, "ImGui Pass");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    VK_LABEL_END(cmd);

    vkCmdEndRendering(cmd);
    vkimageutils::transitionImage(cmd, m_swapChainImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

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
        vkimageutils::destroyImage(m_depthImages[i]);
        vkimageutils::destroyImage(m_depthResolveImages[i]);
        vkimageutils::destroyImage(m_colorImages[i]);
    }

    for (VulkanImage& img : m_swapChainImages) {
        vkDestroyImageView(m_device, img.imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	createSwapchain();
    createColorImages();
}

void HyacinthEngine::cleanup()
{
    if (!m_initialized) {
        return;
	}

	vkDeviceWaitIdle(m_device);

    m_frustumCullHelper.shutdown();
	m_shadowHelper.destroy();
	m_owDDGIHelper.shutdown();
    m_rtHelper.shutdown();

	vkdeviceutils::destroyBuffer(m_meshBuffers.indexBuffer);
	vkdeviceutils::destroyBuffer(m_meshBuffers.vertexBuffer);
	vkdeviceutils::destroyBuffer(m_meshBuffers.aabbBuffer);
	vkdeviceutils::destroyBuffer(m_indirectDrawBuffer);
	vkdeviceutils::destroyBuffer(m_worldMatrixBuffer);
    vkdeviceutils::destroyBuffer(m_drawDataBuffer);
    vkdeviceutils::destroyBuffer(m_materialBuffer);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

	vkDestroyFence(m_device, m_uploadFence, nullptr);

    m_pipelineUtil.destroyPipeline();
    m_depthPipelineUtil.destroyPipeline();

    for (auto& tex : m_scene.dummyTextures) {
        vkimageutils::destroyImage(tex);
    }
    for (auto& obj : m_scene.objects) {
        for (auto& node : obj.nodes) {
            vkdeviceutils::destroyBuffer(node.get()->accelStructureIndexBuffer);
            vkdeviceutils::destroyBuffer(node.get()->accelStructureVertexBuffer);
        }
        for (auto& tex : obj.textures) {
            vkimageutils::destroyImage(tex);
        }
    }

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemas[i], nullptr);
		vkDestroySemaphore(m_device, m_imageFinishedSemas[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);

        vkDestroyCommandPool(m_device, m_frameData[i].commandPool, nullptr);

		vkdeviceutils::destroyBuffer(m_frameData[i].uniformBuffer);

        vkdeviceutils::destroyBuffer(m_shadowHelper.m_uniformBuffers[i]);
	}

    for (int i = 0; i < m_swapChainImages.size(); i++) {
        vkimageutils::destroyImage(m_depthImages[i]);
        vkimageutils::destroyImage(m_depthResolveImages[i]);
        vkimageutils::destroyImage(m_colorImages[i]);
    }

	m_descriptorAllocator.destroyPool();
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_textureSetLayout, nullptr);

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
