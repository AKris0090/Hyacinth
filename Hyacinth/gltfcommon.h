#pragma once

#include "tiny_gltf.h"
#include "stb_image.h"
#include "vkimageutils.h"
#include "vkdescriptorutils.h"
#include "fullscreen_quad.h"
#include "transform.h"
#include <unordered_set>

struct gltfPrimitive {
    uint32_t indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

struct gltfNode {
    std::vector<gltfPrimitive*> primitives;
    Transform matComponents;
    int32_t skinIndex = -1;
    glm::mat4 worldTransform = glm::mat4(1.0f);
    std::vector<gltfNode*> children;
    gltfNode* parent;
    bool includeInAccel = false;
    bool dynamic = false;
    uint32_t index;
    bool upperBody = false;
    bool lowerBody = false;
    std::string nodeName;

    std::vector<Vertex> vertices;
    VulkanBuffer accelStructureVertexBuffer;
    std::vector<uint32_t> indices;
    VulkanBuffer accelStructureIndexBuffer;

    std::vector<float> queuedYawShifts;
    std::vector<float> queuedPitchShifts;
    glm::quat queuedQuatRotation;
};

static gltfNode* findNode(std::vector<gltfNode*>& nodes, gltfNode* parent, uint32_t index)
{
    gltfNode* nodeFound = nullptr;
    if (parent->index == index)
    {
        return parent;
    }
    for (auto& child : parent->children)
    {
        nodeFound = findNode(nodes, child, index);
        if (nodeFound)
        {
            break;
        }
    }
    return nodeFound;
}

static gltfNode* nodeFromIndex(std::vector<gltfNode*>& nodes, uint32_t index)
{
    gltfNode* nodeFound = nullptr;
    for (auto& node : nodes)
    {
        nodeFound = findNode(nodes, node, index);
        if (nodeFound)
        {
            break;
        }
    }
    return nodeFound;
}

static glm::mat4 getAnimatedMatrix(Transform& matP, glm::mat4& matrix) {
    return glm::translate(glm::mat4(1.0f), matP.position) * glm::mat4(matP.rotation) * glm::scale(glm::mat4(1.0f), matP.scale);// *matrix;
}

static glm::mat4 getNodeMatrix(gltfNode* node)
{
    glm::mat4 nodeMatrix = getAnimatedMatrix(node->matComponents, node->worldTransform);
    gltfNode* currentParent = node->parent;
    while (currentParent)
    {
        nodeMatrix = getAnimatedMatrix(currentParent->matComponents, currentParent->worldTransform) * nodeMatrix;
        currentParent = currentParent->parent;
    }
    return nodeMatrix;
}

static bool isParentOf(gltfNode* search, gltfNode* target) {
    if (search == nullptr) {
        return false;
    }
    if (search == target) {
        return true;
    }
    return isParentOf(search->parent, target);
}

// joint matrix buffer stored in the animation controller
struct Skin
{
    std::string					name;
    gltfNode* skeletonRoot = nullptr;
    std::vector<glm::mat4>		inverseBindMatrices;
    std::vector<gltfNode*>		joints;

    static void loadSkins(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Skin>& skinsOut);
};

struct AnimationSampler
{
    std::string            interpolation;
    std::vector<float>     inputs;
    std::vector<glm::vec4> outputsVec4;
};

struct AnimationChannel
{
    std::string path;
    gltfNode* node;
    uint32_t    samplerIndex;
};

struct Animation
{
    std::string                   name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float                         start = (std::numeric_limits<float>::max)();
    float                         end = (std::numeric_limits<float>::min)();

    static void loadAnimations(tinygltf::Model* input, std::vector<gltfNode*>& nodes, std::vector<Animation>& animsOut);
};