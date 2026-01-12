#pragma once

#include "tiny_gltf.h"
#include "vkmeshutils.h"
#include "vkimageutils.h"
#include <unordered_set>

#define DUMMY_NORMAL_TEX_INDEX 0
#define DUMMY_METALROUGH_TEX_INDEX 1

const std::vector<std::string> DUMMY_PATHS = {
        "./shaders/dummyNormal.png",
        "./shaders/dummyMetallicRoughness.png"
};

static std::string getFilePathExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct gltfDrawCommand {
    uint32_t    indexCount;
    uint32_t    instanceCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    firstInstance;
    uint32_t    vertexCount;
};

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

    std::vector<glm::vec3> vertices;
    VulkanBuffer nodeVertexBuffer;
    std::vector<uint32_t> indices;
    VulkanBuffer nodeIndexBuffer;
};

struct gltfObject {
	std::vector<std::unique_ptr<gltfNode>> nodes;
    uint32_t nodeCounter;

    std::vector<VulkanImage> textures;
    std::vector<uint32_t> textureIndices;
    std::vector<MaterialInstance> materials;

    std::unordered_set<uint32_t>* imageIsSRGB;
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
    std::vector<VulkanImage> dummyTextures;
    uint32_t numTextures;

    std::vector<VkSampler> imageSamplers;

    std::vector<MaterialInstance> materials;
    std::unordered_map<int32_t, std::vector<gltfDrawCommand>> sortedDrawCalls;
    std::vector<VkDrawIndexedIndirectCommand> drawCommands;
    std::vector<uint32_t> vertexCounts; // for acceleration structure building

    std::vector<DrawData> drawData;
    std::vector<GPUMaterialIndices> materialObjects;
    
    void buildNodeBuffers(DeviceContext& ctx, gltfNode* node);
    void buildSceneGraph(DeviceContext& ctx);
    void createDummyTextures(DeviceContext& ctx);
    void uploadTextures(VkDevice& dev, VkDescriptorSet& descriptor);
};

namespace gltfutils {
    void loadTexture(DeviceContext& ctx, gltfObject& node, tinygltf::Model* model, VkFormat format, uint32_t imageIndex);
    gltfObject loadFromFile(const std::string& filename, DeviceContext& ctx);
}