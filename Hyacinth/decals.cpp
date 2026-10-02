#include "decals.h"

void DecalManager::setup(SWChainImageFormat swImageFormat, VkFormat depthImageFormat, VkDescriptorSetLayout uniformSetLayout, VkDescriptorSetLayout textureSetLayout) {
    decalPipelineUtil.addShader("shaders/decalVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    decalPipelineUtil.addShader("shaders/decalFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    decalPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    decalPipelineUtil.setDefaultAttributes();
    decalPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    decalPipelineUtil.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    decalPipelineUtil.setColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 1);
    decalPipelineUtil.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    decalPipelineUtil.enableBlending();
    decalPipelineUtil.enableDepthTest(true, VK_COMPARE_OP_LESS);
    decalPipelineUtil.setDepthAttachmentFormat(depthImageFormat);
    decalPipelineUtil.numColorAttachments = 1;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swImageFormat.extent.width;
    viewport.height = (float)swImageFormat.extent.height;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 0.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swImageFormat.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    decalPipelineUtil.m_viewportState.pViewports = &viewport;
    decalPipelineUtil.m_viewportState.pScissors = &scissor;

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(decalPushConstant);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 2> sets = { uniformSetLayout, textureSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &range;
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();

    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &decalPipelineUtil.m_pipeline.layout));

    decalPipelineUtil.buildPipeline();
}

void DecalManager::addDecal(glm::vec3 pos, glm::vec3 normal) {
    glm::quat rotation = glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), normal);

	glm::mat4 mat = glm::translate(glm::mat4(1.f), pos);
    mat *= glm::toMat4(rotation);
	mat = glm::scale(mat, bulletHoleDecaleSize);

    {
        std::unique_lock<std::shared_mutex> lock(instanceLock);
        if (decalInstances.size() >= MAX_BULLET_DECALS) {
            decalInstances.pop_front();
        }
        decalInstances.push_back(mat);
    }
}

void DecalManager::shutdown() {
    decalPipelineUtil.destroyPipeline();
}