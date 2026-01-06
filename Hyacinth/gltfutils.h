#pragma once

#include "tiny_gltf.h"
#include "vkmeshutils.h"
#include "vkimageutils.h"

static std::string getFilePathExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct gltfPrimitive {
    uint32_t indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

struct gltfNode {
    std::vector<std::unique_ptr<gltfPrimitive>> primitives;
	glm::mat4 worldTransform = glm::mat4(1.0f);
    std::vector<uint32_t> childrenIndices;
    int32_t parentIndex = -1;
};

struct gltfObject {
	std::vector<std::unique_ptr<gltfNode>> nodes;
    uint32_t nodeCounter;

    std::vector<VulkanImage> textures;
    std::vector<uint32_t> textureIndices;
    std::vector<MaterialInstance> materials;
};

struct DrawData {
    uint32_t transformIndex;
    uint32_t materialIndex;
};

struct SceneGraph {
    std::vector<gltfObject> objects;
    std::vector<glm::mat4> transformMatrices;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t numTextures;

    std::vector<VkSampler> imageSamplers;

    std::vector<MaterialInstance> materials;
    std::unordered_map<int32_t, std::vector<VkDrawIndexedIndirectCommand>> sortedDrawCalls;
    std::vector<VkDrawIndexedIndirectCommand> drawCommands;

    std::vector<DrawData> drawData;
    std::vector<GPUMaterialIndices> materialObjects;

    void buildSceneGraph();
    void uploadTextures(VkDevice& dev, VkDescriptorSet& descriptor);
};

namespace gltfutils {
    gltfObject loadFromFile(const std::string& filename, DeviceContext& ctx);
}