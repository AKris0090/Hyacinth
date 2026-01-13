#define VMA_IMPLEMENTATION

#ifndef NDEBUG
#define VMA_DEBUG_LOG_FORMAT(format, ...) \
    printf("[VMA] " format "\n", __VA_ARGS__)

#define VMA_LEAK_LOG_FORMAT(format, ...) \
    printf("[VMA-LEAK] " format "\n", __VA_ARGS__)
#endif

#include "vkengine.h"
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

	VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
    deviceFeatures.samplerAnisotropy = true;
    deviceFeatures.depthClamp = true;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{};
    accelFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeature.accelerationStructure = true;

    VkPhysicalDeviceVulkan12Features dev12Features{};
    dev12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    dev12Features.pNext = &accelFeature;
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

    vkGetDeviceQueue(m_device, m_qfIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_qfIndices.presentFamily.value(), 0, &m_presentQueue);

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
        vkimageutils::createImageView(m_device, m_swapChainImages[i], VK_IMAGE_ASPECT_COLOR_BIT);
    }

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
}

void HyacinthEngine::createGraphicsPipeline()
{
    m_pipelineUtil.addShader(m_device, "shaders/vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_pipelineUtil.addShader(m_device, "shaders/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_pipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_pipelineUtil.setDefaultAttributes();
	m_pipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
	m_pipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	m_pipelineUtil.setColorAttachmentFormat(m_swImageFormat.format);
    m_pipelineUtil.setMultisampling(m_msaaSamples);
	m_pipelineUtil.disableBlending();

    m_pipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
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
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutCInfo.setLayoutCount = 1;
	pipelineLayoutCInfo.pSetLayouts = &m_descriptorSetLayout;

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCInfo, nullptr, &m_pipelineUtil.m_pipeline.layout));

	m_pipelineUtil.buildPipeline(m_device);
}

void HyacinthEngine::loadScene() {
    auto path = vkdebugutils::getExeDir() / "objects" / "sponza" / "sponza.gltf";
    // auto path2 = vkdebugutils::getExeDir() / "objects" / "SM_Deccer_Cubes_Textured_Complex.glb";
    // auto path = vkdebugutils::getExeDir() / "objects" / "bistro.glb";

    m_scene.objects.push_back(gltfutils::loadFromFile(path.string(), m_devContext));
    // m_scene.objects.push_back(gltfutils::loadFromFile(path2.string(), m_devContext));
    m_scene.buildSceneGraph(m_devContext);
    m_meshBuffers = vkmeshutils::uploadMesh(m_devContext, m_scene.indices, m_scene.vertices);
    m_scene.createDummyTextures(m_devContext);
}

void HyacinthEngine::createBuffers() {
    size_t drawDataBufferSize = sizeof(DrawData) * m_scene.drawData.size();
    m_drawDataBuffer = vkdeviceutils::createBuffer(m_device, m_allocator, drawDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    vkdeviceutils::uploadToBuffer(m_devContext, m_drawDataBuffer, drawDataBufferSize, m_scene.drawData.data());

    size_t materialDataBufferSize = sizeof(GPUMaterialIndices) * m_scene.materialObjects.size();
    m_materialBuffer = vkdeviceutils::createBuffer(m_device, m_allocator, materialDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    vkdeviceutils::uploadToBuffer(m_devContext, m_materialBuffer, materialDataBufferSize, m_scene.materialObjects.data());

	size_t drawCmdBufferSize = sizeof(VkDrawIndexedIndirectCommand) * m_scene.drawCommands.size();
    m_indirectDrawBuffer = vkdeviceutils::createBuffer(m_device, m_allocator, drawCmdBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	vkdeviceutils::uploadToBuffer(m_devContext, m_indirectDrawBuffer, drawCmdBufferSize, m_scene.drawCommands.data());

	size_t matrixBuffSize = sizeof(glm::mat4) * m_scene.transformMatrices.size();
    m_worldMatrixBuffer = vkdeviceutils::createBuffer(m_device, m_allocator, matrixBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
	vkdeviceutils::uploadToBuffer(m_devContext, m_worldMatrixBuffer, matrixBuffSize, m_scene.transformMatrices.data());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].uniformBuffer = vkdeviceutils::createBuffer(m_device, m_allocator, sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        m_frameData[i].mappedUniformBuffer = m_frameData[i].uniformBuffer.info.pMappedData;
    }
}

void HyacinthEngine::createDescriptorSets()
{
    // all +1 are for dummyTextures
    // TODO: update when adding rest of dummies
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (((float) (m_scene.numTextures + 2)) / (float) MAX_FRAMES_IN_FLIGHT) }, // divided because multiplied by maxSets later. we only want 1 descriptor per texture, not 3
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (float) (1.f / MAX_FRAMES_IN_FLIGHT) } // for shadows, only 1 (not 3)
    };

    m_descriptorAllocator.initPool(m_device, MAX_FRAMES_IN_FLIGHT, sizes);

    {
		DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutBuilder.addBinding(1, m_scene.numTextures + 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // shadow map
		m_descriptorSetLayout = layoutBuilder.buildLayout(m_device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frameData[i].descriptorSet = m_descriptorAllocator.allocate(m_device, m_descriptorSetLayout);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_frameData[i].uniformBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UBO);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_frameData[i].descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

        m_scene.uploadTextures(m_device, m_frameData[i].descriptorSet);

        VkDescriptorImageInfo depthImageInfo{};
        depthImageInfo.imageView = m_shadowHelper.completeView;
        depthImageInfo.sampler = m_shadowHelper.sampler;
        depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptorShadowWrite{};
        descriptorShadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorShadowWrite.dstSet = m_frameData[i].descriptorSet;
        descriptorShadowWrite.dstBinding = 2;
        descriptorShadowWrite.dstArrayElement = 0;
        descriptorShadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorShadowWrite.descriptorCount = 1;
        descriptorShadowWrite.pImageInfo = &depthImageInfo;
        vkUpdateDescriptorSets(m_device, 1, &descriptorShadowWrite, 0, nullptr);
    }
}

static void imgui_check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void HyacinthEngine::setupImGUI() 
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL3_InitForVulkan(m_window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physicalDevice;
    init_info.Device = m_device;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = m_pipelineUtil.m_renderInfo;
    init_info.PipelineInfoMain.MSAASamples = m_msaaSamples;
    init_info.QueueFamily = m_qfIndices.graphicsFamily.value();
    init_info.Queue = m_graphicsQueue;
    init_info.UseDynamicRendering = true;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    init_info.MinImageCount = m_swapChainImages.size();
    init_info.ImageCount = m_swapChainImages.size();
    init_info.CheckVkResultFn = imgui_check_vk_result;
    init_info.Allocator = nullptr;
    ImGui_ImplVulkan_Init(&init_info);
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

    m_devContext.device = &m_device;
    m_devContext.allocator = &m_allocator;
    m_devContext.graphicsQueue = &m_graphicsQueue;
    m_devContext.commandBuffer = &m_uploadFrame.commandBuffer;
	m_devContext.uploadFence = &m_uploadFence;
    m_camera.aspectRatio = (float)m_swImageFormat.extent.width / (float)m_swImageFormat.extent.height;

    VkExtent3D extent{
        .width = m_swImageFormat.extent.width,
        .height = m_swImageFormat.extent.height,
        .depth = 1
    };
    int numImages = m_swapChainImages.size();
    m_depthImages.resize(numImages);
    m_colorImages.resize(numImages);
    m_depthResolveImages.resize(numImages);
    for (uint32_t i = 0; i < numImages; i++) {
        m_depthImages[i] = vkimageutils::createImage(m_devContext, extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_msaaSamples, false);
        m_depthResolveImages[i] = vkimageutils::createImage(m_devContext, extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, false);
        m_colorImages[i] = vkimageutils::createImage(m_devContext, extent, m_swImageFormat.format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_msaaSamples, false);
    }

    loadScene();

    createBuffers();

    m_shadowHelper.setup(m_devContext, MAX_FRAMES_IN_FLIGHT);

    createDescriptorSets();

    createGraphicsPipeline();

    setupImGUI();

    m_rtHelper.setup(m_devContext, m_scene);

    m_initialized = true;
}

void HyacinthEngine::update() {
    m_shadowHelper.update(m_camera, m_frameIndex);

    UBO newuniform{};
    newuniform.proj = m_camera.proj;
    newuniform.view = m_camera.view;
    newuniform.viewPos = glm::vec4(m_camera.transform.position, Input::mouseDown() ? 0.f : 1.f);
    newuniform.lightPos = glm::vec4(m_shadowHelper.transform.position, 1.f);
    newuniform.cascadeSplits = glm::vec4(m_shadowHelper.m_cascades[0].splitDepth, m_shadowHelper.m_cascades[1].splitDepth, m_shadowHelper.m_cascades[2].splitDepth, m_camera.farClip);
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
    if (res2 == VK_ERROR_OUT_OF_DATE_KHR || res2 == VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Swapchain out of date!");
    }

    m_camera.update(Time::getDeltaTime());
    update();

    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    vkdeviceutils::beginCommandBuffer(cmd);

    vkimageutils::transitionImage(cmd, m_colorImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkimageutils::transitionImage(cmd, m_swapChainImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkimageutils::transitionImage(cmd, m_depthImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    vkimageutils::transitionImage(cmd, m_depthResolveImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_meshBuffers.vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, m_meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void HyacinthEngine::drawShadowMaps(VkCommandBuffer& cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowHelper.m_shadowPipelineUtil.m_pipeline.pipeline);

    vkimageutils::transitionImage(cmd, m_shadowHelper.shadowImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, m_shadowHelper.extent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pStencilAttachment = nullptr;

    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        VkRenderingAttachmentInfo depthAttachment = m_shadowHelper.getAttachmentInfo(i);
        renderingInfo.pDepthAttachment = &depthAttachment;
        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_shadowHelper.m_shadowPipelineUtil.m_pipeline.layout,
            0,
            1,
            &m_shadowHelper.m_cascades[m_frameIndex].uniformDescriptorSet,
            0,
            nullptr
        );

        shadowGPUPushConstant pushConstants{};
        pushConstants.cascadeIndex = i;
        pushConstants.transformAddress = m_worldMatrixBuffer.gpuAddress;
        pushConstants.drawDataAddress = m_drawDataBuffer.gpuAddress;
        vkCmdPushConstants(cmd, m_shadowHelper.m_shadowPipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadowGPUPushConstant), &pushConstants);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_shadowHelper.extent.width);
        viewport.height = static_cast<float>(m_shadowHelper.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_shadowHelper.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer.buffer, 0, static_cast<uint32_t>(m_scene.drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));
        vkCmdEndRendering(cmd);
    }

    vkimageutils::transitionImage(cmd, m_shadowHelper.shadowImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
}

void HyacinthEngine::drawImGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("FPS MENU");
    auto framesPerSecond = 1.0f / Time::getDeltaTime();
    ImGui::Text("rfps: %.0f", framesPerSecond);
    ImGui::Text("ft: %.2f ms", Time::getDeltaTime() * 1000.0f);
    ImGui::End();

    ImGui::Begin("Var Editor");
    ImGui::DragFloat("light position x", &m_shadowHelper.transform.position.x, 0.1f );
    ImGui::DragFloat("light position y", &m_shadowHelper.transform.position.y, 0.1f);
    ImGui::DragFloat("light position z", &m_shadowHelper.transform.position.z, 0.1f);
    ImGui::End();
}

void HyacinthEngine::draw()
{
    drawImGui();

    setupDraw();
    VkCommandBuffer cmd = getCurrentFrame().commandBuffer;

    // shadows
    drawShadowMaps(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineUtil.m_pipeline.pipeline);
    VkRenderingAttachmentInfo colorAttachment = vkimageutils::createColorAttachmentInfo(m_colorImages[m_swImageIndex].imageView, m_swapChainImages[m_swImageIndex].imageView, clearColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkimageutils::createDepthAttachmentInfo(m_depthImages[m_swImageIndex].imageView, m_depthResolveImages[m_swImageIndex].imageView);
	VkRenderingInfo renderingInfo = vkdeviceutils::createRenderingInfo(m_swImageFormat.extent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineUtil.m_pipeline.layout,
        0,
        1,
        &m_frameData[m_frameIndex].descriptorSet,
        0,
        nullptr
    );

    GPUDrawPushConstants pushConstants{};
    pushConstants.transformAddress = m_worldMatrixBuffer.gpuAddress;
    pushConstants.materialAddress = m_materialBuffer.gpuAddress;
    pushConstants.drawDataAddress = m_drawDataBuffer.gpuAddress;

    vkCmdPushConstants(cmd, m_pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

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

	vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer.buffer, 0, static_cast<uint32_t>(m_scene.drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));

    endDraw();
}

void HyacinthEngine::endDraw()
{
    VkCommandBuffer& cmd = getCurrentFrame().commandBuffer;

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
    vkimageutils::transitionImage(cmd, m_swapChainImages[m_swImageIndex].image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

    VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HyacinthEngine::cleanup()
{
    if (!m_initialized) {
        return;
	}

	vkDeviceWaitIdle(m_device);

	vkdeviceutils::destroyBuffer(m_allocator, m_meshBuffers.indexBuffer);
	vkdeviceutils::destroyBuffer(m_allocator, m_meshBuffers.vertexBuffer);
	vkdeviceutils::destroyBuffer(m_allocator, m_indirectDrawBuffer);
	vkdeviceutils::destroyBuffer(m_allocator, m_worldMatrixBuffer);
    vkdeviceutils::destroyBuffer(m_allocator, m_drawDataBuffer);
    vkdeviceutils::destroyBuffer(m_allocator, m_materialBuffer);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

	vkDestroyFence(m_device, m_uploadFence, nullptr);

	vkDestroyPipeline(m_device, m_pipelineUtil.m_pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineUtil.m_pipeline.layout, nullptr);

    for (auto& tex : m_scene.dummyTextures) {
        vkDestroySampler(m_device, tex.imageSampler, nullptr);
        vkDestroyImageView(m_device, tex.imageView, nullptr);
        vmaDestroyImage(m_allocator, tex.image, tex.imageAllocation);
    }
    for (auto& obj : m_scene.objects) {
        for (auto& tex : obj.textures) {
            vkDestroySampler(m_device, tex.imageSampler, nullptr);
            vkDestroyImageView(m_device, tex.imageView, nullptr);
            vmaDestroyImage(m_allocator, tex.image, tex.imageAllocation);
        }
    }

    // shadow stuff
    vkDestroySampler(m_device, m_shadowHelper.sampler, nullptr);
    vkDestroyImageView(m_device, m_shadowHelper.completeView, nullptr);
    for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
        vkDestroyImageView(m_device, m_shadowHelper.m_cascades[i].cascadeImageView, nullptr);
    }
    vmaDestroyImage(m_allocator, m_shadowHelper.shadowImage, m_shadowHelper.shadowAllocation);
    vkDestroyPipelineLayout(m_device, m_shadowHelper.m_shadowPipelineUtil.m_pipeline.layout, nullptr);
    vkDestroyPipeline(m_device, m_shadowHelper.m_shadowPipelineUtil.m_pipeline.pipeline, nullptr);

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemas[i], nullptr);
		vkDestroySemaphore(m_device, m_imageFinishedSemas[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);

        vkDestroyCommandPool(m_device, m_frameData[i].commandPool, nullptr);

		vkdeviceutils::destroyBuffer(m_allocator, m_frameData[i].uniformBuffer);

        vkdeviceutils::destroyBuffer(m_allocator, m_shadowHelper.m_uniformBuffers[i]);
	}

    for (int i = 0; i < m_swapChainImages.size(); i++) {
        vkDestroyImageView(m_device, m_depthImages[i].imageView, nullptr);
        vmaDestroyImage(m_allocator, m_depthImages[i].image, m_depthImages[i].imageAllocation);

        vkDestroyImageView(m_device, m_depthResolveImages[i].imageView, nullptr);
        vmaDestroyImage(m_allocator, m_depthResolveImages[i].image, m_depthResolveImages[i].imageAllocation);

        vkDestroyImageView(m_device, m_colorImages[i].imageView, nullptr);
        vmaDestroyImage(m_allocator, m_colorImages[i].image, m_colorImages[i].imageAllocation);
    }

	m_descriptorAllocator.destroyPool(m_device);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    m_shadowHelper.m_descriptorAllocator.destroyPool(m_device);
    vkDestroyDescriptorSetLayout(m_device, m_shadowHelper.m_descriptorSetLayout, nullptr);

    vmaDestroyAllocator(m_allocator);

	vkDestroyCommandPool(m_device, m_uploadFrame.commandPool, nullptr);

    for (VulkanImage& img : m_swapChainImages) {
        vkDestroyImageView(m_device, img.imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

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
