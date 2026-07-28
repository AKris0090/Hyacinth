#include "pch.h"
#include "framework.h"
#include "light_loader.h"

#define LPRINT(x) std::cout << "[LIGHTLOADER] " << x << std::endl;

uint32_t numLoadingTasks = 0;
uint32_t loaded = 0;
constexpr int PRINT_BAR_WIDTH = 50;

void printProgress() {
    float loadProgress = (float) loaded / (float) numLoadingTasks;
    std::cout << "[";
    int pos = PRINT_BAR_WIDTH * (int) loadProgress;
    for (int i = 0; i < PRINT_BAR_WIDTH; i++) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << (int)(loadProgress * 100.f) << "%\r";
    std::cout.flush();
}

LightAABB getBoundingBox(std::vector<LightVertex>& vertices) {
    LightAABB bounds;
    bounds.min = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    bounds.max = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    for (const auto& v : vertices) {
        bounds.grow(glm::vec4(v.pos.x, v.pos.y, v.pos.z, 1.f));
    }
    return bounds;
}

LightTexture loadTexture(tinygltf::Model* model, L_TEXTURE_TYPE format, uint32_t imageIndex) {
    tinygltf::Image& curImage = model->images[imageIndex];
    LightTexture tex{};
    tex.texType = format;
    tex.component = curImage.component;
    tex.textureData.resize(curImage.width * curImage.height * 4);

    switch (curImage.component) {
    case 4:
        memcpy(tex.textureData.data(), curImage.image.data(), tex.textureData.size());
        break;
    case 3:
    {
        std::vector<unsigned char> rgb;
        rgb.resize(curImage.width * curImage.height * 3);
        memcpy(rgb.data(), curImage.image.data(), rgb.size());
        for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
            tex.textureData[j * 4] = rgb[j * 3];
            tex.textureData[j * 4 + 1] = rgb[j * 3 + 1];
            tex.textureData[j * 4 + 2] = rgb[j * 3 + 2];
            tex.textureData[j * 4 + 3] = 255;
        }
        break;
    }
    case 1:
    {
        std::vector<unsigned char> r;
        r.resize(curImage.width * curImage.height);
        memcpy(r.data(), curImage.image.data(), r.size());
        for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
            tex.textureData[j * 4] = tex.textureData[j * 4 + 1] = tex.textureData[j * 4 + 2] = tex.textureData[j * 4 + 3] = r[j];
        }
        break;
    }
    }

    tex.width = curImage.width;
    tex.height = curImage.height;

    if (curImage.name.empty()) {
        tex.texName = "image_" + curImage.uri;
    }
    else {
        tex.texName = curImage.name;
    }

    return tex;
}

void loadSkins(tinygltf::Model* input, LightMesh* mesh) {
    tinygltf::Skin glTFSkin = input->skins[0];
    mesh->jointMatrixSize = glTFSkin.joints.size() * sizeof(glm::mat4);
    LightSkin& skinOut = mesh->skin;

    for (int jointIndex : glTFSkin.joints)
    {
        LightNode* node = mesh->getNodeByIndex(jointIndex);
        if (node)
        {
            skinOut.joints.push_back(node);
        }
    }

    if (glTFSkin.inverseBindMatrices > -1)
    {
        const tinygltf::Accessor& accessor = input->accessors[glTFSkin.inverseBindMatrices];
        const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
        skinOut.inverseBindMatrices.resize(accessor.count);
        memcpy(skinOut.inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
    }

    loaded++;
    printProgress();
}

void loadAnimations(tinygltf::Model* input, LightMesh* mesh)
{
    mesh->animations.resize(input->animations.size());

    for (size_t i = 0; i < input->animations.size(); i++)
    {
        tinygltf::Animation glTFAnimation = input->animations[i];

        mesh->animations[i].samplers.resize(glTFAnimation.samplers.size());
        for (size_t j = 0; j < glTFAnimation.samplers.size(); j++)
        {
            tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
            LightAnimSampler& dstSampler = mesh->animations[i].samplers[j];
            dstSampler.interpolation = glTFSampler.interpolation;

            {
                const tinygltf::Accessor& accessor = input->accessors[glTFSampler.input];
                const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                const float* buf = static_cast<const float*>(dataPtr);
                for (size_t index = 0; index < accessor.count; index++)
                {
                    dstSampler.inputs.push_back(buf[index]);
                }
                for (auto input : mesh->animations[i].samplers[j].inputs)
                {
                    if (input < mesh->animations[i].start)
                    {
                        mesh->animations[i].start = input;
                    };
                    if (input > mesh->animations[i].end)
                    {
                        mesh->animations[i].end = input;
                    }
                }
            }

            {
                const tinygltf::Accessor& accessor = input->accessors[glTFSampler.output];
                const tinygltf::BufferView& bufferView = input->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input->buffers[bufferView.buffer];
                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                switch (accessor.type)
                {
                case TINYGLTF_TYPE_VEC3: {
                    const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++)
                    {
                        dstSampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
                    }
                    break;
                }
                case TINYGLTF_TYPE_VEC4: {
                    const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++)
                    {
                        dstSampler.outputsVec4.push_back(buf[index]);
                    }
                    break;
                }
                default: {
                    LPRINT("Unknown accessor type");
                    break;
                }
                }
            }
        }

        mesh->animations[i].channels.resize(glTFAnimation.channels.size());
        for (size_t j = 0; j < glTFAnimation.channels.size(); j++)
        {
            tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
            LightAnimChannel& dstChannel = mesh->animations[i].channels[j];
            dstChannel.path = glTFChannel.target_path;
            dstChannel.samplerIndex = glTFChannel.sampler;
            dstChannel.node = mesh->getNodeByIndex(glTFChannel.target_node);
        }

        loaded++;
        printProgress();
    }
}

void loadNode(const tinygltf::Model* model, const tinygltf::Node& nodeIn, LightMesh* mesh, uint32_t nodeIndex, LightNode* parent, uint32_t materialOffset) {
    LightNode* node = new LightNode();
    node->parent = parent;
    node->nodeIndex = nodeIndex;
    node->nodeName = nodeIn.name;
    if (parent) node->transform.parent = &parent->transform;

    if (nodeIn.skin > -1) {
        mesh->meshOwnerNode = node;
    }

    if (nodeIn.matrix.size() == 16) {
        glm::mat4 m = glm::make_mat4x4(nodeIn.matrix.data());

        glm::vec3 skew;
        glm::vec4 perspective;

        glm::decompose(m, node->transform.scale, node->transform.rotation, node->transform.position, skew, perspective);
    }
    else {
        if (nodeIn.translation.size() == 3) {
            node->transform.position = glm::make_vec3(nodeIn.translation.data());
        }
        if (nodeIn.rotation.size() == 4) {
            node->transform.rotation = glm::make_quat(nodeIn.rotation.data());
        }
        if (nodeIn.scale.size() == 3) {
            node->transform.scale = glm::make_vec3(nodeIn.scale.data());
        }
    }

    if (nodeIn.children.size() > 0) {
        for (size_t i = 0; i < nodeIn.children.size(); i++) {
            mesh->numNodes++;
            loadNode(model, model->nodes[nodeIn.children[i]], mesh, nodeIn.children[i], node, materialOffset);
        }
    }

    if (nodeIn.mesh > -1) {
        mesh->numMeshedNodes++;
        const tinygltf::Mesh gltfMesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < gltfMesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = gltfMesh.primitives[i];
            std::vector<LightVertex> primVertices;
            std::vector<uint32_t> primIndices;
            LightPrimitive p;
            p.materialIndex = (gltfPrim.material < 0 ? 0 : gltfPrim.material) + materialOffset;
            if (gltfPrim.material < 0) p.materialIndex = 0; // first material should ALWAYS be a dummy mat
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
                LightVertex v{};
                glm::vec3 normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));

                glm::vec2 uv = uvBuff ? glm::make_vec2(&uvBuff[vert * 2]) : glm::vec3(0.0f);

                v.pos = glm::vec4(glm::make_vec3(&positionBuff[vert * 3]), uv.x);
                v.normal = glm::vec4(normal, uv.y);
                v.tangent = tangentsBuff ? glm::make_vec4(&tangentsBuff[vert * 4]) : glm::vec4(0.0f);

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
                primVertices.push_back(v);
            }

            const tinygltf::Accessor& accessor = model->accessors[gltfPrim.indices];
            const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model->buffers[view.buffer];

            switch (accessor.componentType) {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primIndices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primIndices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primIndices.push_back(buf[index]);
                }
                break;
            }
            default:
                std::cout << "index component type not supported" << std::endl;
                throw std::runtime_error("index component type not supported");
            }

            p.firstIndex = static_cast<uint32_t>(mesh->indices.size());
            p.indexCount = static_cast<uint32_t>(primIndices.size());
            p.firstVertex = static_cast<uint32_t>(mesh->vertices.size());
            p.vertexCount = static_cast<uint32_t>(primVertices.size());
            mesh->numVertices += p.vertexCount;

            p.bounds = getBoundingBox(primVertices);
            mesh->bounds.grow(p.bounds);

            for (auto& v : primVertices) {
                mesh->vertices.push_back(v);
            }

            for (auto& i : primIndices) {
                mesh->indices.push_back(i);
            }

            node->primitives.push_back(p);
        }
    }

    if (parent) {
        parent->children.push_back(node);
        if (nodeIn.mesh > -1) mesh->meshedNodes.push_back(parent->children[parent->children.size() - 1]);
    }
    else {
        mesh->parentNodes.push_back(node);
        if (nodeIn.mesh > -1) mesh->meshedNodes.push_back(mesh->parentNodes[mesh->parentNodes.size() - 1]);
    }

    loaded++;
    printProgress();
}

namespace LightLoader {
    LightMesh* loadFromFile(const std::string& filename, LightLoaderOptions options) {
        LPRINT("Loading from : " + filename);

        LightMesh* mesh = new LightMesh();
        tinygltf::Model* model;
        model = new tinygltf::Model();
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        bool fileLoaded = false;
        if (getFileExtension(filename) == "glb") {
            fileLoaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, filename);
        }
        else {
            fileLoaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, filename);
        }

        if (error.size() > 0) {
            LPRINT("Loading Errors: " + error);
        }
        if (warning.size() > 0) {
            LPRINT("Loading Warnings: " + warning);
        }

        if (!fileLoaded) {
            LPRINT("Failed to load glTF file: " + filename);
            throw std::runtime_error("[LIGHTLOADER] Failed to load glTF file: " + filename);
        }

        numLoadingTasks = static_cast<uint32_t>(model->nodes.size() + model->materials.size() + model->images.size() + model->skins.size() + model->animations.size());

        const tinygltf::Scene& scene = model->scenes[model->defaultScene];
        for (const auto& nodeIndex : scene.nodes) {
            const tinygltf::Node& node = model->nodes[nodeIndex];
            loadNode(model, node, mesh, -1, nullptr, options.materialOffset);
        }

        if (options.loadMaterials) {
            std::vector<uint32_t> textureIndices(model->textures.size());
            for (size_t i = 0; i < model->textures.size(); i++) {
                textureIndices[i] = model->textures[i].source;
            }

            std::unordered_set<uint32_t> imageIsSRGB;
            for (size_t i = 0; i < model->materials.size(); i++) {
                LightMaterialInstance material;
                tinygltf::Material gltfMat = model->materials[i];
                if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
                    material.baseColorIndex = textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + options.textureOffset;
                }
                else { material.baseColorIndex = DUMMY_COLOR_TEX_INDEX; }
                if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
                    material.normalIndex = textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + options.textureOffset;
                }
                else { material.normalIndex = DUMMY_NORMAL_TEX_INDEX; }
                if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
                    material.metallicRoughnessIndex = textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + options.textureOffset;
                }
                else { material.metallicRoughnessIndex = DUMMY_METALROUGH_TEX_INDEX; }
                material.alphaCutoff = (float) gltfMat.alphaCutoff;

                mesh->materials.push_back(material);
                loaded++;
                printProgress();
            }

            for (uint32_t i = 0; i < model->images.size(); i++) {
                L_TEXTURE_TYPE format = (imageIsSRGB.find(i) == imageIsSRGB.end()) ? L_UNORM : L_RGB;
                mesh->textures.push_back(loadTexture(model, format, i));
                loaded++;
                printProgress();
            }
        }

        if (options.loadAnimations) {
            loadSkins(model, mesh);
            loadAnimations(model, mesh);
        }

        delete model;
        return mesh;
    }
}

// NODE SEARCH FUNCTIONS /////////////////////////////

LightNode* findNodeIndex(uint32_t index, LightNode* current) {
    if (current->nodeIndex == index) {
        return current;
    }

    LightNode* found = nullptr;
    if (current->children.size() > 0) {
        for (auto& child : current->children) {
            found = findNodeIndex(index, child);
            if (found != nullptr) {
                return found;
            }
        }
    }
    return found;
}

LightNode* LightMesh::getNodeByIndex(uint32_t index) {
    LightNode* found = nullptr;
    for (auto& n : parentNodes) {
        found = findNodeIndex(index, n);
        if (found != nullptr) {
            break;
        }
    }
    if (found == nullptr) {
        LPRINT("TS does NOT have the node ur looking for :sob:");
        throw std::runtime_error("TS does NOT have the node ur looking for :sob:");
    }

    return found;
}

LightNode* findNodeName(std::string search, LightNode* current) {
    if (current->nodeName == search) {
        return current;
    }

    LightNode* found = nullptr;
    if (current->children.size() > 0) {
        for (auto& child : current->children) {
            found = findNodeName(search, child);
            if (found != nullptr) {
                return found;
            }
        }
    }
    return found;
}

LightNode* LightMesh::getNodeByName(std::string nodeName) {
    LightNode* found = nullptr;
    for (auto& n : parentNodes) {
        found = findNodeName(nodeName, n);
        if (found != nullptr) {
            break;
        }
    }
    if (found == nullptr) {
        LPRINT("TS does NOT have the node ur looking for :sob:");
        throw std::runtime_error("TS does NOT have the node ur looking for :sob:");
    }

    return found;
}

bool LightMesh::isParentOf(LightNode* search, LightNode* target) {
    if (search == nullptr) {
        return false;
    }
    if (search == target) {
        return true;
    }
    return isParentOf(search->parent, target);
}