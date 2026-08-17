#include "outline.h"

void OutlineHelper::setup(VkDescriptorSetLayout& uniformSetLayout, VkDescriptorSetLayout& gBufferSetLayout, VkExtent2D swapChainExtent, VkFormat depthFormat, std::vector<VulkanImage*>& depthStencilImages, std::vector<VulkanImage*>& amrImages) {
    // CREATE STENCIL SAMPLER DESCRIPTOR
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3.f },
    };

    m_descriptorAllocator.initPool(MAX_FRAMES_IN_FLIGHT, sizes);

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        computeGrowSetLayout = layoutBuilder.buildLayout(nullptr, 0);
    }

    computeGrowSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        computeGrowSets[i] = m_descriptorAllocator.allocate(computeGrowSetLayout);

        VkDescriptorImageInfo* imageInfo = new VkDescriptorImageInfo{};
        imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo->imageView = (*depthStencilImages[i]).stencilImageView;
        vkdescriptorutils::imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet imageWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        imageWrite.dstSet = computeGrowSets[i];
        imageWrite.dstBinding = 0;
        imageWrite.dstArrayElement = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = imageInfo; // add stencil image to binding 0

        vkdescriptorutils::queueWriteImage(computeGrowSets[i], 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, *amrImages[i], VK_IMAGE_LAYOUT_GENERAL); // amr image to binding 1
        vkdescriptorutils::queueWriteImage(computeGrowSets[i], 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, *depthStencilImages[i], VK_IMAGE_LAYOUT_GENERAL); // depth image view to binding 2

        vkdescriptorutils::queuedWrites.push_back(imageWrite);
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
    // layerOutlinePipeline.addShader("shaders/quadVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    // layerOutlinePipeline.addShader("shaders/outlineLayer.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    // 
    // layerOutlinePipeline.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // layerOutlinePipeline.setDefaultAttributes();
    // layerOutlinePipeline.setPolygonMode(VK_POLYGON_MODE_FILL);
    // layerOutlinePipeline.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // layerOutlinePipeline.setColorAttachmentFormat(VK_FORMAT_R8G8B8A8_SRGB, 1);
    // layerOutlinePipeline.setMultisampling(VK_SAMPLE_COUNT_1_BIT);
    // layerOutlinePipeline.disableBlending();
    // 
    // VkViewport viewport{};
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // viewport.width = (float) swapChainExtent.width;
    // viewport.height = (float) swapChainExtent.height;
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;
    // 
    // VkRect2D scissor{};
    // scissor.offset = { 0, 0 };
    // scissor.extent = swapChainExtent;
    // 
    // VkPipelineViewportStateCreateInfo viewportState{};
    // viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // viewportState.viewportCount = 1;
    // viewportState.pViewports = &viewport;
    // viewportState.scissorCount = 1;
    // viewportState.pScissors = &scissor;
    // 
    // layerOutlinePipeline.m_viewportState.pViewports = &viewport;
    // layerOutlinePipeline.m_viewportState.pScissors = &scissor;
    // 
    // std::array<VkDescriptorSetLayout, 1> sets = { gBufferSetLayout };
    // 
    // VkPipelineLayoutCreateInfo pipelineLayoutCInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    // pipelineLayoutCInfo.setLayoutCount = static_cast<uint32_t>(sets.size());
    // pipelineLayoutCInfo.pSetLayouts = sets.data();
    // 
    // VK_CHECK(vkCreatePipelineLayout(vkdeviceutils::device, &pipelineLayoutCInfo, nullptr, &layerOutlinePipeline.m_pipeline.layout));
    // 
    // layerOutlinePipeline.buildPipeline();

    // CREATE COMPUTE GROW AND SOBEL OPERATOR PIPELINES ///////////////////////////////////////////////////////////////////////////////////////////

    std::array<VkDescriptorSetLayout, 2> setLayouts = { computeGrowSetLayout, gBufferSetLayout };
    computeGrowPipeline = vkpipelineutils::createComputePipeline(setLayouts.data(), static_cast<int>(setLayouts.size()), &range, 1, "shaders/growComp.spv");

    // VkPushConstantRange range{};
    // range.offset = 0;
    // range.size = sizeof(computeSkinPushConstant);
    // range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    // sobelEdgesPipeline = vkpipelineutils::createComputePipeline(nullptr, 0, &range, 1, "shaders/compSkin.spv");
}

void OutlineHelper::drawEdges(VkCommandBuffer& cmd, uint32_t imageIndex, VkDescriptorSet& uniformSet, VkDescriptorSet& compositeSet, VulkanImage& depthStencilImage, VulkanImage& amrImage, VkExtent2D swExtent, VkDeviceAddress& renderCallAddress, VkDeviceAddress& materialAddress, std::vector<VkDrawIndexedIndirectCommand>& stencilRenderCalls, VulkanBuffer& skinnedVertexBuffer) {
    // transition image to color attachment optimal
    vkimageutils::transitionImageColorAttachment(cmd, amrImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageColorAttachment(cmd, depthStencilImage, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_STENCIL_BIT);

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
    VkRenderingAttachmentInfo stencilAttachment = vkimageutils::createStencilAttachmentInfo(depthStencilImage.stencilImageView, true);

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, swExtent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &amrattachment;
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

    // TODO: transition the amr image and depth and stencil image to read/write
    vkimageutils::transitionImageGeneral(cmd, amrImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    vkimageutils::transitionImageGeneral(cmd, depthStencilImage, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_IMAGE_ASPECT_STENCIL_BIT);

    // grow the image by a two pixels
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeGrowPipeline.pipeline);
    std::array<VkDescriptorSet, 2> compSets = { computeGrowSets[imageIndex], compositeSet };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeGrowPipeline.layout, 0, static_cast<uint32_t>(compSets.size()), compSets.data(), 0, nullptr);
    vkCmdDispatch(cmd, swExtent.width, swExtent.height, 1);

    // TODO: use sobel operator to determine edges and use threshold to block all noise
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sobelEdgesPipeline.pipeline);

    // TODO: transition the amr image back to shader read only
}

void OutlineHelper::shutdown() {
    stencilDrawPipeline.destroyPipeline();
    layerOutlinePipeline.destroyPipeline();
    computeGrowPipeline.destroy();
    sobelEdgesPipeline.destroy();
}