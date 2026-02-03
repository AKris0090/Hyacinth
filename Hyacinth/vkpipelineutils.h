#pragma once

#include <vulkan/vulkan.h>
#include "vkdeviceutils.h"
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

struct VulkanPipeline {
    VkPipelineLayout    layout;
	VkPipeline          pipeline;

    void destroy() {
        vkDestroyPipeline(vkdeviceutils::device, pipeline, nullptr);
        vkDestroyPipelineLayout(vkdeviceutils::device, layout, nullptr);
	}
};

class VulkanPipelineBuilder {
public:
    VulkanPipeline                                  m_pipeline;
	std::vector<VkPipelineShaderStageCreateInfo>    m_shaderStages;

    VkPipelineInputAssemblyStateCreateInfo          m_inputAssembly;
    VkPipelineRasterizationStateCreateInfo          m_rasterizer;
    VkPipelineColorBlendAttachmentState             m_colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo            m_multisampling;
    VkPipelineDepthStencilStateCreateInfo           m_depthStencil;
    VkPipelineRenderingCreateInfo                   m_renderInfo;
	VkPipelineViewportStateCreateInfo               m_viewportState;
    VkFormat                                        m_colorAttachmentformat;
    VkPipelineVertexInputStateCreateInfo            m_vertexInputInfo;

    void reset();
    VulkanPipelineBuilder() { reset(); };
    void buildPipeline();
	void addShader(std::string shaderFile, VkShaderStageFlagBits stage);

    void setInputTopology(VkPrimitiveTopology topology);
	void setPolygonMode(VkPolygonMode polygonMode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void setMultisamplingNone();
    void setMultisampling(VkSampleCountFlagBits sampleCount);
    void disableBlending();
	void setColorAttachmentFormat(VkFormat format);
	void setDepthAttachmentFormat(VkFormat format);
	void enableDepthTest(bool depthWrite, VkCompareOp op);
    void setDefaultAttributes();
    void setPositionAttribute();
    void destroyPipeline();

private:
    bool depthPass;
};

namespace vkpipelineutils {
    VkPipelineShaderStageCreateInfo createShader(std::string shaderFile, VkShaderStageFlagBits stage);
	VulkanPipeline createComputePipeline(VkDescriptorSetLayout* layouts, int numLayouts, VkPushConstantRange* pcRanges, int numPcRanges, std::string shaderFile);
}