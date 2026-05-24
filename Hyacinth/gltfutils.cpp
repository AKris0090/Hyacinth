#include "gltfutils.h"
#include "vkimageutils.h"
#include <glm/gtc/type_ptr.hpp>
#include "tangentHelper.h"

AABB getBoundingBox(std::vector<Vertex>& vertices) {
    AABB bounds;
    bounds.min = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    bounds.max = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    for (const auto& v : vertices) {
        bounds.grow(v);
    }
    return bounds;
}

AABB getWorldSpaceBoundingBox(gltfNode* node) {
    AABB bounds;
    bounds.min = glm::vec4(glm::vec3(FLT_MAX), 1.f);
    bounds.max = glm::vec4(glm::vec3(FLT_MIN), 1.f);
    glm::mat4 worldMatrix = getNodeMatrix(node);
    for (const auto& p : node->primitives) {
        for (const auto& v : p->vertices) {
            bounds.grow(worldMatrix * glm::vec4(v.pos.x, v.pos.y, v.pos.z, 1.f));
        }
    }
    for (const auto& n : node->children) {
        bounds.grow(getWorldSpaceBoundingBox(n));
    }

    return bounds;
}

void gltfObject::updateJoints(gltfNode* node, void* pMappedJointMatrixBuffer)
{
    if (node->skinIndex > -1)
    {
        glm::mat4              inverseTransform = glm::inverse(getNodeMatrix(node));
        Skin&                  skin = skins[node->skinIndex];
        size_t                 numJoints = (uint32_t)skin.joints.size();
        std::vector<glm::mat4> finalJointMatrices(numJoints);
        for (size_t i = 0; i < numJoints; i++)
        {
            
            finalJointMatrices[i] = inverseTransform * (getNodeMatrix(skin.joints[i]) * skin.inverseBindMatrices[i]);
        }

        memcpy(pMappedJointMatrixBuffer, finalJointMatrices.data(), finalJointMatrices.size() * sizeof(glm::mat4));
    }

    for (auto& child : node->children)
    {
        updateJoints(child, pMappedJointMatrixBuffer);
    }
}

static void loadCachedNode(gltfObject& obj, bool includeInAccel, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, uint32_t nodeIndex, gltfNode* parent,std::vector<gltfNode*>& parentNodes, std::ifstream& file) {
    auto node = new gltfNode();
    node->parent = parent;
    node->index = nodeIndex;
    node->skinIndex = nodeIn.skin;
    node->includeInAccel = includeInAccel;
    node->dynamic = dynamic;
    node->nodeName = nodeIn.name;

    if (nodeIn.matrix.size() == 16) {
        glm::mat4 m = glm::make_mat4x4(nodeIn.matrix.data());
        glm::vec3 skew; glm::vec4 perspective;
        glm::decompose(m, node->localTransform.scale, node->localTransform.rotation, node->localTransform.position, skew, perspective);
    }
    else {
        if (nodeIn.translation.size() == 3) node->localTransform.position = glm::make_vec3(nodeIn.translation.data());
        if (nodeIn.rotation.size() == 4)    node->localTransform.rotation = glm::make_quat(nodeIn.rotation.data());
        if (nodeIn.scale.size() == 3)       node->localTransform.scale = glm::make_vec3(nodeIn.scale.data());
    }

    for (size_t i = 0; i < nodeIn.children.size(); i++) {
        loadCachedNode(obj, includeInAccel, dynamic, model,
            model->nodes[nodeIn.children[i]], nodeIn.children[i],
            node, parentNodes, file);
    }

    if (nodeIn.mesh > -1) {
        const tinygltf::Mesh& mesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            auto p = new gltfPrimitive();
            p->materialIndex = mesh.primitives[i].material;

            std::string line;
            while (std::getline(file, line)) {
                if (line == "p::") continue;
                if (line[0] == 'v') {
                    std::istringstream iss(line.substr(2));
                    Vertex v{};
                    iss >> v.pos.x >> v.pos.y >> v.pos.z
                        >> v.normal.x >> v.normal.y >> v.normal.z
                        >> v.uvs.x >> v.uvs.y >> v.uvs.z >> v.uvs.w
                        >> v.tangent.x >> v.tangent.y >> v.tangent.z >> v.tangent.w
                        >> v.jointIndices.x >> v.jointIndices.y >> v.jointIndices.z >> v.jointIndices.w
                        >> v.jointWeights.x >> v.jointWeights.y >> v.jointWeights.z >> v.jointWeights.w;
                    p->vertices.push_back(v);
                }
                else if (line[0] == 'i') {
                    std::istringstream iss(line.substr(2));
                    uint32_t idx;
                    while (iss >> idx) p->indices.push_back(idx);
                    break;
                }
            }

            node->primitives.push_back(p);
        }
    }

    if (parent) {
        parent->children.push_back(node);
    }
    else {
        parentNodes.push_back(node);
    }
    obj.allNodes.push_back(node);
}

static void loadGLTFNode(gltfObject& obj, bool includeInAccel, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, uint32_t nodeIndex, gltfNode* parent, std::vector<gltfNode*>& parentNodes) {
    SMikkTSpaceContext mikktContext = { .m_pInterface = &MikkTInterface };

    auto node = new gltfNode();
    node->parent = parent;
    node->index = nodeIndex;
    node->skinIndex = nodeIn.skin;
	node->includeInAccel = includeInAccel;
    node->dynamic = dynamic;
    node->nodeName = nodeIn.name;

    if (nodeIn.matrix.size() == 16) {
        glm::mat4 m = glm::make_mat4x4(nodeIn.matrix.data());

        glm::vec3 skew;
        glm::vec4 perspective;

        glm::decompose(m, node->localTransform.scale, node->localTransform.rotation, node->localTransform.position, skew, perspective);
    }
    else {
        if (nodeIn.translation.size() == 3) {
            node->localTransform.position = glm::make_vec3(nodeIn.translation.data());
        }
        if (nodeIn.rotation.size() == 4) {
            node->localTransform.rotation = glm::make_quat(nodeIn.rotation.data());
        }
        if (nodeIn.scale.size() == 3) {
            node->localTransform.scale = glm::make_vec3(nodeIn.scale.data());
        }
    }

    if (nodeIn.children.size() > 0) {
        for (size_t i = 0; i < nodeIn.children.size(); i++) {
            loadGLTFNode(obj, includeInAccel, dynamic, model, model->nodes[nodeIn.children[i]], nodeIn.children[i], node, parentNodes);
        }
    }

    if (nodeIn.mesh > -1) {
        const tinygltf::Mesh mesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = mesh.primitives[i];
			auto p = new gltfPrimitive();
			node->primitives.push_back(std::move(p));
            p->materialIndex = gltfPrim.material;
            bool hasSkin = false;

            uint32_t currentNumIndices = 0;
            uint32_t currentNumVertices = 0;

            // FOR VERTICES
            const float* positionBuff = nullptr;
            const float* normalsBuff = nullptr;
            const float* uvBuff = nullptr;
            const float* tangentsBuff = nullptr;
            const void* jointIndicesBuffer = nullptr;
            const float* jointWeightsBuffer = nullptr;

            if (gltfPrim.attributes.find("POSITION") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("POSITION")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                positionBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                currentNumVertices = static_cast<uint32_t>(accessor.count);
            }
            if (gltfPrim.attributes.find("NORMAL") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("NORMAL")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                normalsBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (gltfPrim.attributes.find("TEXCOORD_0") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("TEXCOORD_0")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                uvBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (gltfPrim.attributes.find("TANGENT") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("TANGENT")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                tangentsBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            int jointType;
            if (gltfPrim.attributes.find("JOINTS_0") != gltfPrim.attributes.end())
            {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("JOINTS_0")->second];
                jointType = accessor.componentType;
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                jointIndicesBuffer = reinterpret_cast<const void*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (gltfPrim.attributes.find("WEIGHTS_0") != gltfPrim.attributes.end())
            {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("WEIGHTS_0")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                jointWeightsBuffer = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            hasSkin = (jointIndicesBuffer && jointWeightsBuffer);

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                Vertex v{};
                glm::vec3 normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));

                glm::vec2 uv = uvBuff ? glm::make_vec2(&uvBuff[vert * 2]) : glm::vec3(0.0f);

                v.pos = glm::vec4(glm::make_vec3(&positionBuff[vert * 3]), 1.f);
                v.normal = glm::vec4(normal, 1.f);
                v.tangent = tangentsBuff ? glm::make_vec4(&tangentsBuff[vert * 4]) : glm::vec4(0.0f);
                v.uvs = glm::vec4(uv.x, uv.y, 0.f, 0.f);

                if (hasSkin) {
                    switch (jointType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        const uint16_t* buffer = static_cast<const uint16_t*>(jointIndicesBuffer);
                        v.jointIndices = glm::vec4(glm::make_vec4(&buffer[vert * 4]));
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        const uint8_t* buffer = static_cast<const uint8_t*>(jointIndicesBuffer);
                        v.jointIndices = glm::vec4(glm::make_vec4(&buffer[vert * 4]));
                        break;
                    }
                    default:
                        std::cout << "Joint component type not supported" << std::endl;
                        break;
                    }
                }
                else {
                    v.jointIndices = glm::vec4(0.0f);
                }
                v.jointWeights = hasSkin ? glm::make_vec4(&jointWeightsBuffer[vert * 4]) : glm::vec4(0.0f);
                p->vertices.push_back(v);
            }

            const tinygltf::Accessor& accessor = model->accessors[gltfPrim.indices];
            const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model->buffers[view.buffer];

            switch (accessor.componentType) {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    p->indices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    p->indices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    p->indices.push_back(buf[index]);
                }
                break;
            }
            default:
                std::cout << "index component type not supported" << std::endl;
                std::_Xruntime_error("index component type not supported");
            }

            if (!tangentsBuff) {
                generateTangents(p, mikktContext);
            }
        }
    }

    if (parent) {
        parent->children.push_back(node);
    }
    else {
        parentNodes.push_back(node);
    }
    obj.allNodes.push_back(node);
}

void gltfutils::loadTexture(gltfObject& object, tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
    tinygltf::Image& curImage = model->images[imageIndex];
    VulkanImage texImage{};
    std::vector<unsigned char> rgba;
    rgba.resize(curImage.width * curImage.height * 4);

    switch (curImage.component) {
    case 4:
        memcpy(rgba.data(), curImage.image.data(), rgba.size());
        break;
    case 3:
    {
        std::vector<unsigned char> rgb;
        rgb.resize(curImage.width * curImage.height * 3);
        memcpy(rgb.data(), curImage.image.data(), rgb.size());
        for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
            rgba[j * 4] = rgb[j * 3];
            rgba[j * 4 + 1] = rgb[j * 3 + 1];
            rgba[j * 4 + 2] = rgb[j * 3 + 2];
            rgba[j * 4 + 3] = 255;
        }
        break;
    }
    case 1:
    {
        std::vector<unsigned char> r;
        r.resize(curImage.width * curImage.height);
        memcpy(r.data(), curImage.image.data(), r.size());
        for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
            rgba[j * 4] = rgba[j * 4 + 1] = rgba[j * 4 + 2] = rgba[j * 4 + 3] = r[j];
        }
        break;
    }
    }
    VkExtent3D imageExtents{};
    imageExtents.width = curImage.width;
    imageExtents.height = curImage.height;
    imageExtents.depth = 1;
    texImage = vkimageutils::createTextureImage(rgba.data(), imageExtents, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    vkimageutils::createImageSampler(texImage);
    object.textures.push_back(texImage);
    if (curImage.name.empty()) {
        curImage.name = "image_" + curImage.uri;
    }
    std::cout << "created image: " << curImage.name << std::endl;
}

void gltfObject::setTPControllerParameters(ThirdPersonAnimationController& c, Skin& skin) {
    c.idleAnimation = &animations[0];
    c.runningAnimation = &animations[1];
    c.leftTurnAnimation = &animations[2];
    c.rightTurnAnimation = &animations[3];

    c.currentUpperBodyAnim = c.currentLowerBodyAnim = c.idleAnimation;
    c.currentUpperTime = c.currentLowerTime = c.idleAnimation->start;

    c.previousAnimationTransforms.resize(skin.joints.size());

    for (int j = 0; j < skin.joints.size(); j++) {
        gltfNode* joint = skin.joints[j];
        if (joint->nodeName == "spine.005") {
            c.spine005 = joint;
        }
        else if (joint->nodeName == "upper_arm.L") {
            c.upperArmL = joint;
        }
        else if (joint->nodeName == "upper_arm.R") {
            c.upperArmR = joint;
        }
        else if (joint->nodeName == "spine.007") {
            c.spine007 = joint;
        }
        else if (joint->nodeName == "spine.003") {
            c.spine003 = joint;
        }
        else if (joint->nodeName == "spine") {
            c.spine = joint;
            c.spine->lowerBody = true;
        }
    }
    for (auto& node : allNodes) {
        if (isParentOf(node, c.spine003)) {
            node->upperBody = true;
        }
        else if (isParentOf(node, c.spine007)) {
            node->lowerBody = true;
        }
    }
}

void gltfObject::setFPControllerParameters(FirstPersonAnimationController& c, Skin& skin) {
    c.idleAnimation = &animations[0];
    c.shootAnimation = &animations[1];
    c.spinningAnimation = &animations[2];
    c.currentAnim = c.idleAnimation;
    c.currentTime = c.currentAnim->start;

    for (int j = 0; j < skin.joints.size(); j++) {
        gltfNode* joint = skin.joints[j];
        if (joint->nodeName == "gun") {
            c.gunBone = joint;
        }
        else if (joint->nodeName == "hand.L") {
            c.leftWrist = joint;
        }
        else if (joint->nodeName == "hand.R") {
            c.rightWrist = joint;
        }
    }
}

void gltfObject::setWeaponControllerParams(PistolAnimationController& c, Skin& skin) {
    c.idleAnimation = &animations[0];
    c.shootAnimation = &animations[1];
    c.currentAnim = c.idleAnimation;
    c.currentTime = c.currentAnim->start;
}

void gltfObject::setWeaponParentTo(gltfObject* parentObj) {
    if (parentObj->attachmentPoint == nullptr) {
        std::cout << "no attachment node" << std::endl;
        throw std::runtime_error("attachment node not there");
    }

    gltfNode* gunBaseNode = nullptr;
    for (int j = 0; j < skins[0].joints.size(); j++) {
        if (skins[0].joints[j]->nodeName == "base") {
            gunBaseNode = skins[0].joints[j];
        }
    }
    if (gunBaseNode == nullptr) {
        std::cout << "no gun base node" << std::endl;
        throw std::runtime_error("base node not there");
    }

    gunBaseNode->parent = parentObj->attachmentPoint;
}

gltfObject gltfutils::loadFromFile(const std::string& filename, bool includeInAccel, bool dynamic, bool isCharacter, bool isWeapon, const std::string& cachedNodes) {
	std::cout << "Loading GLTF file: " << filename << std::endl;

	gltfObject object{};
    object.dynamic = dynamic;
    object.isCharacter = isCharacter;
    object.isWeapon = isWeapon;
    object.imageIsSRGB = new std::unordered_set<uint32_t>();
    tinygltf::Model* model;
    model = new tinygltf::Model();
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

    std::ifstream cacheFile;
    if (cachedNodes.length() > 0) {
        cacheFile.open(cachedNodes);
    }

    bool loaded = false;
    if (getFilePathExtension(filename) == "glb") {
        loaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, filename);
    }
    else {
        loaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, filename);
    }

    std::cout << "ERRORS: " << error.c_str() << std::endl;
    std::cout << "WARNINGS: " << warning.c_str() << std::endl;

    if (!loaded) {
		throw std::runtime_error("Failed to load glTF file: " + filename);
	}

    const tinygltf::Scene& scene = model->scenes[model->defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        const tinygltf::Node node = model->nodes[scene.nodes[i]];
        if (cachedNodes.length() == 0) {
            loadGLTFNode(object, includeInAccel, dynamic, model, node, -1, nullptr, object.parentNodes);
        }
        else {
            loadCachedNode(object, includeInAccel, dynamic, model, node, -1, nullptr, object.parentNodes, cacheFile);
        }
    }

    object.textureIndices.resize(model->textures.size());
    for (size_t i = 0; i < model->textures.size(); i++) {
        object.textureIndices[i] = model->textures[i].source;
    }

    object.materials.resize(model->materials.size());
    for (size_t i = 0; i < model->materials.size(); i++) {
        tinygltf::Material gltfMat = model->materials[i];
        if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
            object.materials[i].baseColorIndex = object.textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + 3; // 3 for all dummy textures
            object.imageIsSRGB->insert(object.materials[i].baseColorIndex - 2);
        }
        else { object.materials[i].baseColorIndex = DUMMY_COLOR_TEX_INDEX; }
        if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
            object.materials[i].normalIndex = object.textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + 3;
        }
        else { object.materials[i].normalIndex = DUMMY_NORMAL_TEX_INDEX; }
        if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
            object.materials[i].metallicRoughnessIndex = object.textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + 3;
        }
        else { object.materials[i].metallicRoughnessIndex = DUMMY_METALROUGH_TEX_INDEX; }
    }

    for (uint32_t i = 0; i < model->images.size(); i++) {
        VkFormat format = (object.imageIsSRGB->find(i) == object.imageIsSRGB->end()) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
        loadTexture(object, model, format, i);
    }

    bool skinned = Skin::loadSkins(model, object.parentNodes, object.skins);
    if (skinned) {
        for (const auto& n : object.skins[0].joints) {
            if (n->nodeName == "gun") {
                object.attachmentPoint = n;
            }
        }
        object.skinSize = sizeof(glm::mat4) * object.skins[0].joints.size();
    }
    Animation::loadAnimations(model, object.parentNodes, object.animations);

    object.thirdPersonAnimStateMachine = new ThirdPersonAnimationStateMachine();
    object.firstPersonAnimStateMachine = new FirstPersonAnimationStateMachine();
    object.pistolAnimStateMachine = new PistolAnimationStateMachine();

	delete model;
	return object;
}

void SceneGraph::buildNodeBuffers(gltfNode* node) {
    if (node->primitives.size() == 0 || node->includeInAccel == false) {
        return;
	}

    // for building acceleration structures
    std::vector<glm::vec3> positions;
    for (const auto& v : node->vertices) {
        positions.push_back(v.pos);
    }
    VkDeviceSize vertexBufferSize = positions.size() * sizeof(glm::vec3);
    VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
    node->accelStructureVertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0, "accel_vertex");
    node->accelStructureIndexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0, "accel_index");

    VulkanBuffer staging = vkdeviceutils::createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(staging.info.pMappedData, positions.data(), vertexBufferSize);
    memcpy((char*)staging.info.pMappedData + vertexBufferSize, node->indices.data(), indexBufferSize);

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.srcOffset = 0;
    vertexCopyRegion.dstOffset = 0;
    vertexCopyRegion.size = vertexBufferSize;

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.srcOffset = vertexBufferSize;
    indexCopyRegion.dstOffset = 0;
    indexCopyRegion.size = indexBufferSize;

    vkdeviceutils::executeSingleTimeCommands([&](VkCommandBuffer& cmd) {
        vkCmdCopyBuffer(cmd, staging.buffer, node->accelStructureVertexBuffer.buffer, 1, &vertexCopyRegion);
        vkCmdCopyBuffer(cmd, staging.buffer, node->accelStructureIndexBuffer.buffer, 1, &indexCopyRegion);
        });

    vkdeviceutils::destroyBuffer(staging);
}

void addUnitCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    auto cubePath = vkdebugutils::getExeDir() / "objects" / "cube.glb";
    gltfObject boxObject = gltfutils::loadFromFile(cubePath.string(), false);
    gltfNode* node = boxObject.allNodes[0];
    for (const auto& p : node->primitives) {
        for (const auto& v : p->vertices) {
            node->vertices.push_back(v);
        }
        for (const auto& index : p->indices) {
            node->indices.push_back(index);
        }
    }

    for (int i = 0; i < node->vertices.size(); i++) {
        Vertex vert{};
        vert.pos = glm::vec4(glm::vec3(node->vertices[i].pos), 0.f);
        vertices.push_back(vert);
    }
    for (auto& i : node->indices) {
        indices.push_back(i);
    }
}

void SceneGraph::buildSceneGraph() {
    FullscreenQuad::addFullscreenQuad(vertices, indices);
    addUnitCube(vertices, indices);

    for (auto& o : staticObjects) {
        combinedObjects.push_back(&o);
    }
    for (auto& o : dynamicObjects) {
        combinedObjects.push_back(&o);
    }

    for (const auto& obj : combinedObjects) {
        obj->firstMatrix = obj->dynamic ? dynamicTransformMatrices.size() : staticTransformMatrices.size();
        uint32_t mat_offset = static_cast<uint32_t>(materialObjects.size());
        for (const auto& node: obj->allNodes) {
            if (obj->dynamic) {
                dynamicTransformMatrices.push_back(node->localTransform.getMatrix());
            }
            else {
                staticTransformMatrices.push_back(node->localTransform.getMatrix());
            }
            obj->numMatrices++;
            for (const auto& prim : node->primitives) {
                uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                uint32_t firstIndex = static_cast<uint32_t>(indices.size());
				uint32_t matIndex = prim->materialIndex + mat_offset;

                gltfDrawCommand draw{};
                draw.isCharacter = obj->isCharacter;
                draw.isWeapon = obj->isWeapon;
                draw.dynamic = obj->dynamic;
                draw.firstIndex = firstIndex;
                draw.indexCount = static_cast<uint32_t>(prim->indices.size());
                draw.vertexCount = prim->vertices.size();
                draw.boundingBox = getBoundingBox(prim->vertices);
                if (obj->dynamic) {
                    draw.transformIndex = dynamicTransformMatrices.size() - 1;
                }
                else {
                    draw.transformIndex = staticTransformMatrices.size() - 1;
                }

                for (const auto& v : prim->vertices) {
                    vertices.push_back(v);
                }
                for (const auto& index : prim->indices) {
                    indices.push_back(index + firstVertex);
				}

                // acceleration structure-specific
                uint32_t nodeVertOffset = node->vertices.size();
                for (const auto& v : prim->vertices) {
                    node->vertices.push_back(v);
                }
                for (const auto& i : prim->indices) {
                    node->indices.push_back(i + nodeVertOffset);
                }

                sortedDrawCalls[matIndex].push_back(draw);
            }

            numNodes++;
            if (node->includeInAccel && node->vertices.size() > 0 && node->indices.size() > 0) {
				numAccelNodes++;

                sceneBoundingBox.grow(getWorldSpaceBoundingBox(node));
            }

            // for acceleration structures
            buildNodeBuffers(node);
		}
        
        for (const auto& mat : obj->materials) {
            GPUMaterialIndices newMatIndices{};
            newMatIndices.baseColorIndex = (mat.baseColorIndex == DUMMY_COLOR_TEX_INDEX) ? DUMMY_COLOR_TEX_INDEX : mat.baseColorIndex + numTextures;
            newMatIndices.normalIndex = (mat.normalIndex == DUMMY_NORMAL_TEX_INDEX) ? DUMMY_NORMAL_TEX_INDEX : mat.normalIndex + numTextures;
            newMatIndices.metallicRoughnessIndex = (mat.metallicRoughnessIndex == DUMMY_METALROUGH_TEX_INDEX) ? DUMMY_METALROUGH_TEX_INDEX : mat.metallicRoughnessIndex + numTextures;
            materialObjects.push_back(newMatIndices);
        }

		numTextures += static_cast<uint32_t>(obj->textures.size());
    }

    uint32_t drawCounter = 0;
    for (int matIndex = 0; matIndex < materialObjects.size(); matIndex++) {
        auto& draws = sortedDrawCalls[matIndex];
        for (const auto& gltfDraw : draws) {
            VkDrawIndexedIndirectCommand drawCmd{};
            drawCmd.firstIndex = gltfDraw.firstIndex;
            drawCmd.indexCount = gltfDraw.indexCount;
            drawCmd.instanceCount = 1;
            drawCmd.firstInstance = drawCounter;

            DrawData primDrawData{};
            primDrawData.materialIndex = matIndex;
            primDrawData.transformIndex = gltfDraw.transformIndex;

            drawData.push_back(primDrawData);
            if (gltfDraw.isCharacter) {
                characterDrawCommands.push_back(drawCmd);
            }
            else if (gltfDraw.isWeapon) {
                pistolDrawCommands.push_back(drawCmd);
            }
            else {
                gltfDraw.dynamic ? dynamicDrawCommands.push_back(drawCmd) : staticDrawCommands.push_back(drawCmd);
            }
            boundingBoxes.push_back(gltfDraw.boundingBox);

            drawCounter++;
        }
    }
}

void SceneGraph::createDummyTextures() {
    // add dummy textures
    int index = 0;
    for (const auto& path : DUMMY_PATHS) {
        VulkanImage texImage{};
        stbi_uc* pixels = nullptr;
        int texWidth, texHeight, texChannels;
        pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to load dummy image " + path + "!");
        }
        texImage.extent.width = texWidth;
        texImage.extent.height = texHeight;

        VkExtent3D imageExtents{};
        imageExtents.width = texImage.extent.width;
        imageExtents.height = texImage.extent.height;
        imageExtents.depth = 1;
        if (index != 2) {
            texImage = vkimageutils::createTextureImage((void*)pixels, imageExtents, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
        }
        else {
            texImage = vkimageutils::createTextureImage((void*)pixels, imageExtents, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
        }
        vkimageutils::createImageSampler(texImage);
        dummyTextures.push_back(texImage);
        index++;
        numTextures++;
    }
}

void SceneGraph::createUITextures() {
    uiTextureOffset = numTextures;
    for (const auto& str : UI_TEXTURE_NAMES) {
        auto p = vkdebugutils::getExeDir() / "ui" / str;
        VulkanImage texImage{};
        stbi_uc* pixels = nullptr;
        int texWidth, texHeight, texChannels;
        pixels = stbi_load(p.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to load dummy image " + str + "!");
        }
        texImage.extent.width = texWidth;
        texImage.extent.height = texHeight;

        VkExtent3D imageExtents{};
        imageExtents.width = texImage.extent.width;
        imageExtents.height = texImage.extent.height;
        imageExtents.depth = 1;

        texImage = vkimageutils::createTextureImage((void*)pixels, imageExtents, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
        vkimageutils::createImageSampler(texImage);

        uiTextures.push_back(texImage);
        numTextures++;
    }
}

void SceneGraph::uploadTextures(VkDescriptorSet& descriptor) {
    uint32_t textureOffset = 0;
    for (auto& tex : dummyTextures) {
        vkdescriptorutils::queueWriteImage(descriptor, 0, textureOffset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        textureOffset++;
    }
    for (auto& obj : staticObjects) {
        for (auto& tex : obj.textures) {
            vkdescriptorutils::queueWriteImage(descriptor, 0, textureOffset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            textureOffset++;
        }
    }
    for (auto& obj : dynamicObjects) {
        for (auto& tex : obj.textures) {
            vkdescriptorutils::queueWriteImage(descriptor, 0, textureOffset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            textureOffset++;
        }
    }
    for (auto& tex : uiTextures) {
        vkdescriptorutils::queueWriteImage(descriptor, 0, textureOffset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        textureOffset++;
    }
	vkdescriptorutils::flushDescriptorWrites();
}

void gltfObject::updateThirdPersonAnimation(Entity* e, gltfObject* obj, ThirdPersonAnimationStateMachine& animMachine, ThirdPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer)
{
    animMachine.updateAnimationState(c, deltaTime, e->isMoving ? 1.f : 0.f, 0.f, e->transform.pitch, e->transform.yaw);

    for (auto& node : obj->parentNodes)
    {
        obj->updateJoints(node, pMappedJointMatrixBuffer);
    }
}

void gltfObject::updateFirstPersonAnimation(FIRSTPERSON_STATE state, gltfObject* obj, FirstPersonAnimationStateMachine& animMachine, FirstPersonAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer, bool leftClick, float deltaPitch, float deltaYaw, bool& shootTriggerOut) {
    animMachine.updateAnimationState(c, state, deltaTime, deltaPitch, deltaYaw, shootTriggerOut);

    for (auto& node : obj->parentNodes) {
        obj->updateJoints(node, pMappedJointMatrixBuffer);
    }
}

void gltfObject::updatePistolAnimation(gltfObject* obj, PistolAnimationStateMachine& animMachine, PistolAnimationController& c, float deltaTime, void* pMappedJointMatrixBuffer) {
    animMachine.updateAnimationState(c, deltaTime);

    for (auto& node : obj->parentNodes) {
        obj->updateJoints(node, pMappedJointMatrixBuffer);
    }
}