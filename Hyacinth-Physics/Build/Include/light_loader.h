#pragma once

#include <vector>
#include <memory>
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <string>
#include <iostream>
#include <algorithm>
#include <execution>
#include <mutex>
#include "tiny_gltf.h"

static std::string getFileExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct LightPrimitive {
    uint32_t indexCount = 0;
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

struct LightNode {
    std::vector<std::unique_ptr<LightPrimitive>> primitives;
    glm::mat4 worldTransform = glm::mat4(1.0f);
    std::vector<uint32_t> childrenIndices;
    int32_t parentIndex = -1;
};

struct LightObject {
    std::mutex objectMutex;
    bool dynamic = false;
    std::vector<std::unique_ptr<LightNode>> nodes;
    uint32_t nodeCounter;
};

class LightLoader {
public:
    void loadNode(LightObject* obj, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, int32_t parent);
    LightObject* loadFromFile(const std::string& filePath, bool dynamic);
};