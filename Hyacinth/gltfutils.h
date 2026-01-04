#pragma once

#include "tiny_gltf.h"
#include "vkmeshutils.h"

static std::string getFilePathExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct gltfPrimitive {
    uint32_t indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct gltfNode {
    std::vector<std::unique_ptr<gltfPrimitive>> primitives;
	glm::mat4 worldTransform = glm::mat4(1.0f);
    std::vector<uint32_t> childrenIndices;
    uint32_t parentIndex = -1;
};

struct gltfObject {
	std::vector<std::unique_ptr<gltfNode>> nodes;
    uint32_t nodeCounter;
};

struct sceneGraph {
    std::vector<gltfObject> objects;
    std::vector<glm::mat4> transformMatrices;
    std::vector<uint32_t> transformIndices;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<VkDrawIndexedIndirectCommand> drawCommands;

    void buildSceneGraph();
};

namespace gltfutils {
    gltfObject loadFromFile(const std::string& filename);
}