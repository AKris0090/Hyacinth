#include "frustumcull.h"

void FrustumCullHelper::setup() {
    m_computeSets.resize(MAX_FRAMES_IN_FLIGHT);

    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
    };

    m_computeDescAlloc.initPool(MAX_FRAMES_IN_FLIGHT, sizes);

    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
        m_computeLayout = layoutBuilder.buildLayout(nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformPlaneBuffers[i] = vkdeviceutils::createBuffer(sizeof(CameraFrustumPlanes), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "frustum_uniform");

        m_computeSets[i] = m_computeDescAlloc.allocate(m_computeLayout);

		vkdescriptorutils::queueWriteBuffer(m_computeSets[i], 0, sizeof(CameraFrustumPlanes), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_uniformPlaneBuffers[i]);
    }
	vkdescriptorutils::flushDescriptorWrites();

    VkPushConstantRange cullPCRange{};
    cullPCRange.offset = 0;
    cullPCRange.size = sizeof(ComputeCullPushConstant);
    cullPCRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    m_computeCullPipeline = vkpipelineutils::createComputePipeline(&m_computeLayout, 1, &cullPCRange, 1, "shaders/frustumCull.spv");
}

void FrustumCullHelper::update(CameraFrustumPlanes& planes, int index) {
    memcpy(m_uniformPlaneBuffers[index].pMappedData, &planes, sizeof(CameraFrustumPlanes));
}

void FrustumCullHelper::executeCull(VkCommandBuffer& cmd, VkDescriptorSet& set, VkDeviceAddress& drawBufferAddress, VkDeviceAddress& bbAddress, VkDeviceAddress& matrixAddress, VkDeviceAddress& drawDataAddress, uint32_t numDraws) {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computeCullPipeline.layout, 0, 1, &set, 0, nullptr);

    ComputeCullPushConstant pc{};
    pc.drawBufferAddress = drawBufferAddress;
    pc.bbAddress = bbAddress;
    pc.matrixAddress = matrixAddress;
    pc.drawDataAddress = drawDataAddress;
    pc.numDraws = numDraws;

    vkCmdPushConstants(cmd, m_computeCullPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeCullPushConstant), &pc);

    uint32_t groupCount = (numDraws + 255) / 256;
    vkCmdDispatch(cmd, groupCount, 1, 1);
}

void FrustumCullHelper::shutdown() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkdeviceutils::destroyBuffer(m_uniformPlaneBuffers[i]);
    }
    vkDestroyPipelineLayout(vkdeviceutils::device, m_computeCullPipeline.layout, nullptr);
    vkDestroyPipeline(vkdeviceutils::device, m_computeCullPipeline.pipeline, nullptr);

    m_computeDescAlloc.destroyPool();
    vkDestroyDescriptorSetLayout(vkdeviceutils::device, m_computeLayout, nullptr);
}