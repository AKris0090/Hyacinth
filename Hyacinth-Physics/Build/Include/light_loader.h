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
#include "transform.h"
#include "tiny_gltf.h"
#include "glm/gtx/matrix_decompose.hpp"

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
    std::vector<LightPrimitive*> primitives;
    Transform localTransform;
    std::vector<LightNode*> children;
    LightNode* parentNode;
};

static glm::mat4 getWorldMatrix(LightNode* node) {
    glm::mat4 returnMatrix = node->localTransform.getMatrix();
    LightNode* parent = node->parentNode;
    while (parent) {
        returnMatrix = parent->localTransform.getMatrix() * returnMatrix;
        parent = parent->parentNode;
    }

    return returnMatrix;
}

struct LightObject {
    std::vector<LightNode*> parentNodes;
    uint32_t nodeCounter;
};

class LightLoader {
public:
    void loadNode(LightObject* obj, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, LightNode* parent);
    LightObject* loadFromFile(const std::string& filePath, bool dynamic);
};