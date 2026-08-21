#include "outline.h"

void OutlineHelper::recreateImageResources(VkExtent2D swapChainExtent) {
    characterDepthImages.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkExtent3D ext3{
            .width = swapChainExtent.width,
            .height = swapChainExtent.height,
            .depth = 1
        };
        characterDepthImages[i] = vkimageutils::createImageandView(ext3, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT, false, "depth_image_outline");
    }
}

void OutlineHelper::setup(VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& gBufferSetLayout, VkExtent2D swapChainExtent, VkFormat depthFormat, std::vector<VulkanImage*>& depthStencilImages, std::vector<VulkanImage*>& amrImages) {
    recreateImageResources(swapChainExtent);

    // CREATE STENCIL SAMPLER DESCRIPTOR
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
    };

    m_descriptorAllocator.initPool(MAX_FRAMES_IN_FLIGHT, sizes);

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
        computeGrowSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    computeGrowSets.resize(MAX_FRAMES_IN_FLIGHT);
    stencilImageSampler.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        computeGrowSets[i] = m_descriptorAllocator.allocate(computeGrowSetLayout);

        // create sampler
        VkSamplerCreateInfo samplerCInfo{};
        samplerCInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCInfo.magFilter = VK_FILTER_NEAREST;
        samplerCInfo.minFilter = VK_FILTER_NEAREST;
        samplerCInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCInfo.mipLodBias = 0.0f;
        samplerCInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCInfo.minLod = 0.0f;
        samplerCInfo.maxLod = 0.f;
        samplerCInfo.anisotropyEnable = VK_TRUE;
        samplerCInfo.maxAnisotropy = vkimageutils::getMaxAnisotropy();
        samplerCInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        vkCreateSampler(vkdeviceutils::device, &samplerCInfo, nullptr, &stencilImageSampler[i]);

        VkDescriptorImageInfo* imageInfo = new VkDescriptorImageInfo{};
        imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo->imageView = (*depthStencilImages[i]).stencilImageView;
        imageInfo->sampler = stencilImageSampler[i];
        vkdescriptorutils::imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet imageWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        imageWrite.dstSet = computeGrowSets[i];
        imageWrite.dstBinding = 0;
        imageWrite.dstArrayElement = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = imageInfo; // add stencil image to binding 0
        vkdescriptorutils::queuedWrites.push_back(imageWrite);

        vkdescriptorutils::queueWriteImage(computeGrowSets[i], 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, *amrImages[i], VK_IMAGE_LAYOUT_GENERAL); // amr image to binding 1

        VkDescriptorImageInfo* depthImageInfo = new VkDescriptorImageInfo{};
        depthImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthImageInfo->imageView = (*depthStencilImages[i]).imageView;
        depthImageInfo->sampler = stencilImageSampler[i];
        vkdescriptorutils::imageInfos.push_back(depthImageInfo);

        VkWriteDescriptorSet imageWriteDepth { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        imageWriteDepth.dstSet = computeGrowSets[i];
        imageWriteDepth.dstBinding = 2;
        imageWriteDepth.dstArrayElement = 0;
        imageWriteDepth.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageWriteDepth.descriptorCount = 1;
        imageWriteDepth.pImageInfo = depthImageInfo;
        vkdescriptorutils::queuedWrites.push_back(imageWriteDepth);

        VkDescriptorImageInfo* charDepthImageInfo = new VkDescriptorImageInfo{};
        charDepthImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        charDepthImageInfo->imageView = characterDepthImages[i].imageView;
        charDepthImageInfo->sampler = stencilImageSampler[i];
        vkdescriptorutils::imageInfos.push_back(charDepthImageInfo);

        VkWriteDescriptorSet charDepthImageWriteDepth{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        charDepthImageWriteDepth.dstSet = computeGrowSets[i];
        charDepthImageWriteDepth.dstBinding = 3;
        charDepthImageWriteDepth.dstArrayElement = 0;
        charDepthImageWriteDepth.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        charDepthImageWriteDepth.descriptorCount = 1;
        charDepthImageWriteDepth.pImageInfo = charDepthImageInfo;
        vkdescriptorutils::queuedWrites.push_back(charDepthImageWriteDepth);
    }
    vkdescriptorutils::flushDescriptorWrites();

    // CREATE OUTLINE STENCIL DRAW PIPELINE ///////////////////////////////////////////////////////////////////////////////////////////
    stencilDrawPipeline.addShader("shaders/outlineVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    stencilDrawPipeline.addShader("shaders/drawOutline.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    stencilDrawPipeline.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    stencilDrawPipeline.setDefaultAttributes();
    stencilDrawPipeline.setPolygonMode(VK_POLYGON_MODE_FILL);
    stencilDrawPipeline.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    stencilDrawPipeline.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 1);
    stencilDrawPipeline.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    stencilDrawPipeline.disableBlending();
    stencilDrawPipeline.enableDepthTest(false, VK_COMPARE_OP_EQUAL);
    stencilDrawPipeline.setDepthAttachmentFormat(depthFormat);
    stencilDrawPipeline.setStencilAttachmentFormat(depthFormat);

    std::vector<VkFormat> m_colorAttachmentformats;
    m_colorAttachmentformats.resize(2);
    m_colorAttachmentformats[0] = VK_FORMAT_R8G8B8A8_UNORM;
    m_colorAttachmentformats[1] = VK_FORMAT_R32_SFLOAT;
    stencilDrawPipeline.m_renderInfo.pColorAttachmentFormats = m_colorAttachmentformats.data();

    stencilDrawPipeline.m_depthStencil.stencilTestEnable = true;
    stencilDrawPipeline.m_depthStencil.front.compareMask = OUTLINE_BIT;
    stencilDrawPipeline.m_depthStencil.front.writeMask = OUTLINE_BIT;
    stencilDrawPipeline.m_depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilDrawPipeline.m_depthStencil.front.reference = 1;
    stencilDrawPipeline.m_depthStencil.front.passOp = VK_STENCIL_OP_REPLACE;
    stencilDrawPipeline.m_depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
    stencilDrawPipeline.m_depthStencil.back = stencilDrawPipeline.m_depthStencil.front;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent.width;
    viewport.height = (float) swapChainExtent.height;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 0.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    stencilDrawPipeline.m_viewportState.pViewports = &viewport;
    stencilDrawPipeline.m_viewportState.pScissors = &scissor;

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(GPUDrawPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayout, 1> sets = { uniformSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCInfo.pushConstantRangeCount = 1;
    pipelineLayoutCInfo.pPushConstantRanges = &range;
    pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    pipelineLayoutCInfo.pSetLayouts = sets.data();

    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &stencilDrawPipeline.m_pipeline.layout));

    stencilDrawPipeline.buildPipeline();

    // CREATE LAYER OVER PIPELINE ///////////////////////////////////////////////////////////////////////////////////////////
    layerOutlinePipeline.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    layerOutlinePipeline.addShader("shaders/outlineLayer.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    
    layerOutlinePipeline.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    layerOutlinePipeline.setDefaultAttributes();
    layerOutlinePipeline.setPolygonMode(VK_POLYGON_MODE_FILL);
    layerOutlinePipeline.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    layerOutlinePipeline.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_SRGB, 1);
    layerOutlinePipeline.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    layerOutlinePipeline.enableBlending();
    layerOutlinePipeline.setStencilAttachmentFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);

    layerOutlinePipeline.m_depthStencil.stencilTestEnable = true;
    layerOutlinePipeline.m_depthStencil.front.compareMask = OUTLINE_BIT;
    layerOutlinePipeline.m_depthStencil.front.writeMask = 0;
    layerOutlinePipeline.m_depthStencil.front.compareOp = VK_COMPARE_OP_EQUAL;
    layerOutlinePipeline.m_depthStencil.front.reference = 0;
    layerOutlinePipeline.m_depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
    layerOutlinePipeline.m_depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
    layerOutlinePipeline.m_depthStencil.back = layerOutlinePipeline.m_depthStencil.front;

    layerOutlinePipeline.m_viewportState.pViewports = &viewport;
    layerOutlinePipeline.m_viewportState.pScissors = &scissor;
    
    std::array<VkDescriptorSetLayout, 1> layerSets = { gBufferSetLayout };
    
    VkPipelineLayoutCreateInfo layerPipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layerPipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(layerSets.size());
    layerPipelineLayoutCInfo.pSetLayouts = layerSets.data();
    
    VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &layerPipelineLayoutCInfo, nullptr, &layerOutlinePipeline.m_pipeline.layout));
    
    layerOutlinePipeline.buildPipeline();

    // CREATE COMPUTE GROW AND SOBEL OPERATOR PIPELINES ///////////////////////////////////////////////////////////////////////////////////////////

    std::array<VkDescriptorSetLayout, 2> setLayouts = { computeGrowSetLayout, gBufferSetLayout };
    VkPushConstantRange growPCRange{};
    growPCRange.offset = 0;
    growPCRange.size = sizeof(OutlineGrowPC);
    growPCRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeGrowPipeline = vkpipelineutils::createComputePipeline(setLayouts.data(), static_cast<int>(setLayouts.size()), &growPCRange, 1, "shaders/growComp.spv");
}

void OutlineHelper::drawEdges(VkCommandBuffer& cmd, uint32_t imageIndex, VkDescriptorSet& uniformSet, VkDescriptorSet& compositeSet, VulkanImage& depthStencilImage, VulkanImage& amrImage, VkExtent2D swExtent, VkDeviceAddress& renderCallAddress, VkDeviceAddress& materialAddress, std::vector<VkDrawIndexedIndirectCommand>& stencilRenderCalls, VulkanBuffer& skinnedVertexBuffer) {
    // transition image to color attachment optimal
    vkimageutils::transitionImageColorAttachment(cmd, amrImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, characterDepthImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, false);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_STENCIL_BIT);

    // draw network characters to the stencil and the amr image
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swExtent.width);
    viewport.height = static_cast<float>(swExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swExtent;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilDrawPipeline.m_pipeline.pipeline);
    VkRenderingAttachmentInfo amrattachment = vkimageutils::createColorAttachmentInfo(amrImage.imageView, fullClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
    VkRenderingAttachmentInfo depthRepAttachment = vkimageutils::createColorAttachmentInfo(characterDepthImages[imageIndex].imageView, fullClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
    VkRenderingAttachmentInfo stencilAttachment = vkimageutils::createStencilAttachmentInfo(depthStencilImage.stencilImageView, true);

    std::array<VkRenderingAttachmentInfo, 2> attachments = { amrattachment, depthRepAttachment };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, swExtent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 2;
    renderingInfo.pColorAttachments = attachments.data();
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = &stencilAttachment;

    VK_LABEL(cmd, "Outline Stencil Pass");
    vkCmdBeginRendering(cmd, &renderingInfo);

    std::array<VkDescriptorSet, 1> sets = { uniformSet };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilDrawPipeline.m_pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    OutlinePushConstant pc{
        .renderCallBuffer = renderCallAddress,
        .materialCallBuffer = materialAddress
    };
    vkCmdPushConstants(cmd, stencilDrawPipeline.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(OutlinePushConstant), &pc);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &skinnedVertexBuffer.buffer, offsets);

    // draw the network characters
    for (const auto& call : stencilRenderCalls) {
        vkCmdDrawIndexed(cmd, call.indexCount, call.instanceCount, call.firstIndex, call.vertexOffset, call.firstInstance);
    }

    vkCmdEndRendering(cmd);
    VK_LABEL_END(cmd);

    // transition the amr image and depth and stencil image to read/write
    vkimageutils::transitionImageGeneral(cmd, amrImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    vkimageutils::transitionImageGeneral(cmd, characterDepthImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_STENCIL_BIT);

    // grow the image by a two pixels
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeGrowPipeline.pipeline);
    OutlineGrowPC growPC;
    growPC.screenSize = glm::vec2(swExtent.width - 1, swExtent.height - 1);
    std::array<VkDescriptorSet, 2> compSets = { computeGrowSets[imageIndex], compositeSet };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeGrowPipeline.layout, 0, static_cast<uint32_t>(compSets.size()), compSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, computeGrowPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OutlineGrowPC), &growPC);
    vkCmdDispatch(cmd, swExtent.width, swExtent.height, 1);

    // transition the amr image back to shader read only and stencil to stencil attachment
    vkimageutils::transitionImageGeneral(cmd, amrImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_STENCIL_BIT);
}

void OutlineHelper::shutdown() {
    stencilDrawPipeline.destroyPipeline();
    layerOutlinePipeline.destroyPipeline();
    computeGrowPipeline.destroy();
}