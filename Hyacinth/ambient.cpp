#include "ambient.h"

static inline float lerpFloat(float a, float b, float f) {
    return a + f * (b - a);
}

void AmbientHelper::setup(VkDescriptorSetLayout& uboLayout, VkDescriptorSetLayout& compLayout, SWChainImageFormat& swapchainFormat) {
    // generate ambient sample distribution
    std::uniform_real_distribution<float> randomFloats(0.f, 1.f);
    std::default_random_engine generator;
    for (int i = 0; i < 64; i++) {
        glm::vec3 sample(randomFloats(generator) * 2.f - 1.f, randomFloats(generator) * 2.f - 1.f, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        // place larger weight on occlusions closer to fragment
        float scale = (float)i / 64.f;
        scale = lerpFloat(0.1f, 1.f, scale * scale);
        sample *= scale;

        ssaoKernel.push_back(glm::vec4(sample, 1.f));
    }

    // generate noise to offset sample kernels
    for (int i = 0; i < 16; i++) {
        glm::vec3 noise(randomFloats(generator) * 2.f - 1.f, randomFloats(generator) * 2.f - 1.f, 0.f);
        ssaoNoise.push_back(glm::vec4(noise, 1.f));
    }

    // create noise texture
    noiseTexture = vkimageutils::createImageFromFloatData(4, 4, VK_FORMAT_R32G32B32A32_SFLOAT, ssaoNoise.data());
    VkSamplerCreateInfo samplerInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    VK_CHECK(vkCreateSampler(vkdeviceutils::device, &samplerInfo, nullptr, &noiseTexture.imageSampler));

    // create samples buffer
    samplesUBO = vkdeviceutils::createBuffer(sizeof(AmbientPC), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "ambient_sample_buffer");
    AmbientPC pc;
    pc.screenSize = glm::vec2(swapchainFormat.extent.width, swapchainFormat.extent.height);
    memcpy(pc.samples, ssaoKernel.data(), sizeof(glm::vec4) * 64);
    memcpy(samplesUBO.pMappedData, &pc, sizeof(AmbientPC));

    // create noise descriptor
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f }
    };
    descriptorAllocator.initPool(1, sizes);

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        noiseLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    noiseSet = descriptorAllocator.allocate(noiseLayout);
    vkdescriptorutils::queueWriteImage(noiseSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, noiseTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkdescriptorutils::queueWriteBuffer(noiseSet, 1, sizeof(AmbientPC), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, samplesUBO);
    vkdescriptorutils::flushDescriptorWrites();

    m_ambientPipelineUtil.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_ambientPipelineUtil.addShader("shaders/ambientOcclusion.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    m_ambientPipelineUtil.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_ambientPipelineUtil.setDefaultAttributes();
    m_ambientPipelineUtil.setPolygonMode(VK_POLYGON_MODE_FILL);
    m_ambientPipelineUtil.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    m_ambientPipelineUtil.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 1);
    m_ambientPipelineUtil.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    m_ambientPipelineUtil.disableBlending();
    m_ambientPipelineUtil.m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainFormat.extent.width;
    viewport.height = (float)swapchainFormat.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainFormat.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    m_ambientPipelineUtil.m_viewportState.pViewports = &viewport;
    m_ambientPipelineUtil.m_viewportState.pScissors = &scissor;

    std::array<VkDescriptorSetLayout, 3> sets = { uboLayout, compLayout, noiseLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();

    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &m_ambientPipelineUtil.m_pipeline.layout));

    m_ambientPipelineUtil.buildPipeline();
}

void AmbientHelper::drawAO(VkCommandBuffer& cmd, VkDescriptorSet& compSet, VkDescriptorSet& uboSet, VulkanImage ambientImage, SWChainImageFormat& swapchainFormat) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainFormat.extent.width);
    viewport.height = static_cast<float>(swapchainFormat.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainFormat.extent;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ambientPipelineUtil.m_pipeline.pipeline);
    std::array<VkDescriptorSet, 3> ambientSets = { uboSet, compSet, noiseSet };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ambientPipelineUtil.m_pipeline.layout, 0, static_cast<uint32_t>(ambientSets.size()), ambientSets.data(), 0, nullptr);

    VkRenderingAttachmentInfo compositeAttachment = vkimageutils::createColorAttachmentInfo(ambientImage.imageView, { {{0.0f, 0.0f, 0.0f, 1.0f}} }, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
    VkRenderingInfo compositeRenderingInfo = vkdeviceutils::createRenderingInfo(swapchainFormat.extent, 1, &compositeAttachment, nullptr);
    vkCmdBeginRendering(cmd, &compositeRenderingInfo);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDrawIndexed(cmd, QUAD_INDEX_COUNT, 1, 0, 0, 0);
    vkCmdEndRendering(cmd);
}

void AmbientHelper::shutdown() {
    vkimageutils::destroyImage(noiseTexture);
    vkdeviceutils::destroyBuffer(samplesUBO);

    descriptorAllocator.destroyPool();
    vkDestroyDescriptorSetLayout(vkdeviceutils::device, noiseLayout, nullptr);

    m_ambientPipelineUtil.destroyPipeline();
}