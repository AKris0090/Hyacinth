#include "skybox.h"

// skybox image should be loaded
void SkyboxHelper::setup(SWChainImageFormat swapchainImageFormat, VkDescriptorSetLayout& uniformLayout) {
    // create skybox descriptor
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f },
    };
    m_descriptorAllocator.initPool(1.f, sizes);
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        m_skyboxSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }
    m_skyboxSet = m_descriptorAllocator.allocate(m_skyboxSetLayout);
    vkdescriptorutils::queueWriteImage(m_skyboxSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_skyboxImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkdescriptorutils::flushDescriptorWrites();

	// create skybox pipeline
    m_skyboxPipelineUtil.addShader("shaders/skyVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_skyboxPipelineUtil.addShader("shaders/skyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_skyboxPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_skyboxPipelineUtil.setPositionAttribute();
    m_skyboxPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_skyboxPipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    m_skyboxPipelineUtil.setColorAttachmentFormat(swapchainImageFormat.format, 1);
    m_skyboxPipelineUtil.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    m_skyboxPipelineUtil.disableBlending();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainImageFormat.extent.width;
    viewport.height = (float)swapchainImageFormat.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainImageFormat.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    m_skyboxPipelineUtil.m_viewportState.pViewports = &viewport;
    m_skyboxPipelineUtil.m_viewportState.pScissors = &scissor;

    VkPushConstantRange pcRange;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::vec2);
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 2> sets = { uniformLayout, m_skyboxSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = sets.size();
    pipelineLayoutCInfo.pSetLayouts = sets.data();
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &pcRange;

    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &m_skyboxPipelineUtil.m_pipeline.layout));

    m_skyboxPipelineUtil.buildPipeline();
}

void SkyboxHelper::drawSkybox(VkCommandBuffer& cmd) {
    vkCmdDrawIndexed(cmd, UNIT_CUBE_INDEX_COUNT, 1, QUAD_INDEX_COUNT, QUAD_VERTEX_COUNT, 0);
}

void SkyboxHelper::shutdown() {
    vkimageutils::destroyImage(m_skyboxImage);
    m_descriptorAllocator.destroyPool();
    vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_skyboxSetLayout, nullptr);

    m_skyboxPipelineUtil.destroyPipeline();
}