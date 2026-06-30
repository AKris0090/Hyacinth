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

class GltfObject {
public:
    bool dynamic;
    std::string debugName = "";

    std::vector<gltfNode*> allNodes;
    std::vector<gltfNode*> parentNodes;
    uint32_t nodeCounter;
    gltfNode* attachmentPoint;

    std::vector<VulkanImage> textures;
    std::vector<uint32_t> textureIndices;
    std::vector<MaterialInstance> materials;

    VkDeviceSize drawCommandOffset;
    uint32_t numDrawCommands;
    std::vector<gltfDrawCommand> drawCommands;

    static gltfNode* findNode(GltfObject* obj, std::string nodeName);
    static void setWeaponParentTo(GltfObject* weaponObject, GltfObject* parentObj);
    void loadFromFile(const std::string& filename, std::string name, bool includeInAccel, bool isDynamic = false, tinygltf::Model* pInputModel = nullptr);
};

class AnimatedGltfObject : public GltfObject {
public:
    std::vector<Animation> animations;
    std::vector<Skin> skins;
    size_t skinSize;

    ThirdPersonAnimationStateMachine* thirdPersonAnimStateMachine;
    FirstPersonAnimationStateMachine* firstPersonAnimStateMachine;
    PistolAnimationStateMachine* pistolAnimStateMachine;

    static void loadFromObject(AnimatedGltfObject* animatedObj, tinygltf::Model* model);

    void updateJoints(gltfNode* node, void* pMappedJointMatrixBuffer);
    static void setTPControllerParameters(AnimatedGltfObject* obj, ThirdPersonAnimationController& c, Skin& skin);
    static void setFPControllerParameters(AnimatedGltfObject* obj, FirstPersonAnimationController& c, Skin& skin);
    static void setWeaponControllerParams(AnimatedGltfObject* obj, PistolAnimationController& c, Skin& skin);

    static void updateThirdPersonAnimation(Entity* e, AnimatedGltfObject* obj, ThirdPersonAnimationStateMachine& animMachine, ThirdPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer);
    static void updateFirstPersonAnimation(WEAPON_STATE state, AnimatedGltfObject* armsObject, FirstPersonAnimationStateMachine& animMachine, FirstPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer, bool leftClick, float deltaPitch, float deltaYaw, bool& shootTriggerOut, bool& reloadTriggerOut);
    static void updatePistolAnimation(AnimatedGltfObject* obj, PistolAnimationStateMachine& animMachine, PistolAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer);
    static void updateGrenadeAnimation(AnimatedGltfObject* obj, float deltaTime, void* pMappedJointMatrixBuffer);

    void loadFromFile(const std::string filename, std::string name);
};

class SceneGraph {
public:
    std::vector<GltfObject*> staticObjects;
    std::vector<GltfObject*> dynamicObjects;

    std::vector<glm::mat4> staticTransformMatrices;
    std::vector<glm::mat4> dynamicTransformMatrices;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<VulkanImage> dummyTextures;
    std::vector<VulkanImage> uiTextures;
    uint32_t numTextures = 0;
    uint32_t numNodes = 0;
    uint32_t numAccelNodes = 0;
    uint32_t uiTextureOffset = 0;
    uint32_t worldUITextureOffset = 0;

    AABB sceneBoundingBox;

    std::vector<GltfObject*> combinedObjects;

    std::vector<MaterialInstance> materials;
    std::unordered_map<int32_t, std::vector<gltfDrawCommand>> sortedDrawCalls;
    std::unordered_map<uint32_t, std::vector<VkDrawIndexedIndirectCommand>> sortedCommands;

    std::vector<VkDrawIndexedIndirectCommand> staticDrawCommands;
    std::vector<VkDrawIndexedIndirectCommand> dynamicDrawCommands;

    std::vector<DrawData> drawData;
    std::vector<GPUMaterialIndices> materialObjects;

    std::vector<AABB> boundingBoxes;
    VulkanBuffer boundingBuffer;
    
    SceneGraph();
    void buildNodeBuffers(gltfNode* node);
    void offloadObject(GltfObject* obj);
    void buildSceneGraph();
    void createDummySkyboxTextures(VulkanImage& skyboxImage);
    void createUITextures();
    void loadSkyboxTexture();
    void uploadTextures(VkDescriptorSet& descriptor);
};

namespace gltfutils {
    void loadTexture(GltfObject* object, tinygltf::Model* model, VkFormat format, uint32_t imageIndex);
}