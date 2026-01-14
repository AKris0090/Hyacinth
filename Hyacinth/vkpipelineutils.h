#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

struct VulkanPipeline {
    VkPipelineLayout    layout;
	VkPipeline          pipeline;
};

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

static VkShaderModule createShaderModule(VkDevice& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

static VkPipelineShaderStageCreateInfo createShader(VkDevice& device, std::string shaderFile, VkShaderStageFlagBits stage) {
    auto shaderCode = readFile(shaderFile);

    VkShaderModule shaderModule = createShaderModule(device, shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

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
    void buildPipeline(VkDevice& dev);
	void addShader(VkDevice& device, std::string shaderFile, VkShaderStageFlagBits stage);

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

private:
    bool depthPass;
};