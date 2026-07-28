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
#include <unordered_set>

constexpr int DUMMY_NORMAL_TEX_INDEX = 0;
constexpr int DUMMY_METALROUGH_TEX_INDEX = 1;
constexpr int DUMMY_COLOR_TEX_INDEX = 2;

static std::string getFileExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

enum L_TEXTURE_TYPE {
    L_RGB,
    L_UNORM
};

struct LightAABB {
    glm::vec4 min = glm::vec4(0.f), max = glm::vec4(0.f);
    void grow(glm::vec4 p) { min = (glm::min)(min, glm::vec4(glm::vec3(p), 1.f)), max = (glm::max)(max, glm::vec4(glm::vec3(p), 1.f)); }
    void grow(LightVertex p) { min = (glm::min)(min, glm::vec4(glm::vec3(p.pos), 1.f)), max = (glm::max)(max, glm::vec4(glm::vec3(p.pos), 1.f)); }
    void grow(LightAABB other) { min = (glm::min)(min, other.min), max = (glm::max)(max, other.max); }
    void scale(glm::mat4 scaleMatrix) {
        min = scaleMatrix * glm::vec4(glm::vec3(min), 1.0);
        max = scaleMatrix * glm::vec4(glm::vec3(max), 1.0);
    }
};

struct LightVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec4 tangent;

    // animated members
    glm::vec4 jointIndices;
    glm::vec4 jointWeights;
};

struct LightPrimitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t firstVertex;
    uint32_t vertexCount;

    uint32_t materialIndex;
    LightAABB bounds;
};

struct LightNode {
    uint32_t nodeIndex;
    std::string nodeName;
    Transform transform;

    std::vector<LightPrimitive> primitives;
    std::vector<LightNode*> children;
    LightNode* parent;

    glm::mat4 getMatrix() {
        return transform.getMatrix();
    }
};

struct LightSkin {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<LightNode*> joints;
};

struct LightAnimSampler {
    std::string interpolation;
    std::vector<float> inputs;
    std::vector<glm::vec4> outputsVec4;
};

struct LightAnimChannel {
    std::string	path;
    LightNode* node;
    uint32_t samplerIndex;
};

struct LightAnimation {
    std::vector<LightAnimSampler> samplers;
    std::vector<LightAnimChannel> channels;
    float start = (std::numeric_limits<float>::max)();
    float end = (std::numeric_limits<float>::min)();
};

struct LightTexture {
    L_TEXTURE_TYPE texType;
    uint32_t component;
    uint32_t width;
    uint32_t height;

    std::string texName;

    std::vector<unsigned char> textureData;
};

struct LightMaterialInstance {
    uint32_t baseColorIndex;
    uint32_t normalIndex;
    uint32_t metallicRoughnessIndex;
    float alphaCutoff = 0.5f;
};

struct LightMesh {
    std::string meshName;
    uint32_t numNodes = 0;
    uint32_t numMeshedNodes = 0;
    uint32_t numVertices = 0;

    std::vector<LightVertex> vertices;
    std::vector<uint32_t> indices;

    LightAABB bounds;

    std::vector<LightNode*> parentNodes;
    std::vector<LightNode*> meshedNodes;
    std::unordered_map<uint32_t, LightNode> nodes;

    // materials and textures
    std::vector<LightMaterialInstance> materials;
    std::vector<LightTexture> textures;

    // animated mesh members
    size_t jointMatrixSize;
    std::vector<LightAnimation> animations;
    LightSkin skin;
    LightNode* meshOwnerNode;

    LightNode* getNodeByIndex(uint32_t index);
    LightNode* getNodeByName(std::string nodeName);
    bool isParentOf(LightNode* search, LightNode* target);
};

struct LightLoaderOptions {
    bool loadMaterials = false;
    bool loadAnimations = false;
    uint32_t indexOffset = 0;
    uint32_t vertexOffset = 0;
    uint32_t textureOffset = 0;
    uint32_t materialOffset = 0;
};

namespace LightLoader {
    LightMesh* loadFromFile(const std::string& filepath, LightLoaderOptions options);
}