#pragma once

#include "animation.h"
#include "vkmeshutils.h"
#include "hyacinth_ui.h"
#include "skybox.h"
#include "worldspace_health.h"
#include "entity.h"

constexpr int DUMMY_NORMAL_TEX_INDEX = 0;
constexpr int DUMMY_METALROUGH_TEX_INDEX = 1;
constexpr int DUMMY_COLOR_TEX_INDEX = 2;

const std::vector<std::string> DUMMY_PATHS = {
        "./shaders/dummyNormal.png",
        "./shaders/dummyMetallicRoughness.png",
        "./shaders/dummyColor.png"
};

static std::string getFilePathExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct DrawData {
    uint32_t transformIndex;
    uint32_t materialIndex;
};

struct gltfDrawCommand {
    bool dynamic;
    uint32_t    matIndex;
    uint32_t    objectIndex;
    uint32_t    indexCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    vertexCount;
	uint32_t    transformIndex;
    AABB        boundingBox;
};

struct gltfObject {
    bool dynamic;
    std::string debugName = "";
    uint32_t firstMatrix = 0;
    uint32_t numMatrices = 0;
    uint32_t activeAnimation = 0;
    uint32_t currentBuffer = 0;
    std::vector<gltfNode*> allNodes;
    std::vector<gltfNode*> parentNodes;
    uint32_t nodeCounter;
    std::vector<Animation> animations;
    std::vector<Skin> skins;
    size_t skinSize;
    gltfNode* attachmentPoint;

    std::vector<VulkanImage> textures;
    std::vector<uint32_t> textureIndices;
    std::vector<MaterialInstance> materials;

    std::unordered_set<uint32_t>* imageIsSRGB;

    VkDeviceSize drawCommandOffset;
    uint32_t numDrawCommands;
    std::vector<gltfDrawCommand> drawCommands;

    ThirdPersonAnimationStateMachine* thirdPersonAnimStateMachine;
    FirstPersonAnimationStateMachine* firstPersonAnimStateMachine;
    PistolAnimationStateMachine* pistolAnimStateMachine;

    void updateJoints(gltfNode* node, void* pMappedJointMatrixBuffer);
    void setTPControllerParameters(ThirdPersonAnimationController& c, Skin& skin);
    void setFPControllerParameters(FirstPersonAnimationController& c, Skin& skin);
    void setWeaponControllerParams(PistolAnimationController& c, Skin& skin);
    static void updateThirdPersonAnimation(Entity* e, gltfObject* obj, ThirdPersonAnimationStateMachine& animMachine, ThirdPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer);
    static void updateFirstPersonAnimation(WEAPON_STATE state, gltfObject* obj, FirstPersonAnimationStateMachine& animMachine, FirstPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer, bool leftClick, float deltaPitch, float deltaYaw, bool& shootTriggerOut, bool& reloadTriggerOut);
    static void updatePistolAnimation(gltfObject* obj, PistolAnimationStateMachine& animMachine, PistolAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer);
    static void updateGrenadeAnimation(gltfObject* obj, float deltaTime, void* pMappedJointMatrixBuffer);
    void setWeaponParentTo(gltfObject* parentObj);
};

struct SceneGraph {
    std::vector<gltfObject> staticObjects;
    std::vector<gltfObject> dynamicObjects;

    std::vector<gltfObject*> combinedObjects;

    std::vector<glm::mat4> staticTransformMatrices;
    std::vector<glm::mat4> dynamicTransformMatrices;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<VulkanImage> dummyTextures;
    std::vector<VulkanImage> uiTextures;
    uint32_t numTextures;
    uint32_t numNodes = 0;
    uint32_t numAccelNodes = 0;
    uint32_t uiTextureOffset = 0;
    uint32_t worldUITextureOffset = 0;

    AABB sceneBoundingBox;

    std::vector<VkSampler> imageSamplers;

    std::vector<MaterialInstance> materials;
    std::unordered_map<int32_t, std::vector<gltfDrawCommand>> sortedDrawCalls;
    std::unordered_map<uint32_t, std::vector<VkDrawIndexedIndirectCommand>> sortedCommands;

    std::vector<VkDrawIndexedIndirectCommand> staticDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> dynamicDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> characterDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> pistolDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> flashDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> tracerCommands;

    std::vector<DrawData> drawData;
    std::vector<GPUMaterialIndices> materialObjects;

    std::vector<AABB> boundingBoxes;
    VulkanBuffer boundingBuffer;
    
    void buildNodeBuffers(gltfNode* node);
    void buildSceneGraph();
    void createDummySkyboxTextures(VulkanImage& skyboxImage);
    void createUITextures();
    void loadSkyboxTexture();
    void uploadTextures(VkDescriptorSet& descriptor);
};

namespace gltfutils {
    void loadTexture(gltfObject& node, tinygltf::Model* model, VkFormat format, uint32_t imageIndex);
    gltfObject loadFromFile(const std::string& filename, std::string debugName, bool includeInAccel, bool dynamic = false);
}