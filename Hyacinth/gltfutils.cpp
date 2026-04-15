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

glm::mat4 gltfObject::getNodeMatrix(gltfNode* node)
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

void gltfObject::updateJoints(gltfNode* node)
{
    if (node->skinIndex > -1)
    {
        // Update the joint matrices
        glm::mat4              inverseTransform = glm::inverse(getNodeMatrix(node));
        Skin&                  skin = skins[node->skinIndex];
        size_t                 numJoints = (uint32_t)skin.joints.size();
        std::vector<glm::mat4> finalJointMatrices(numJoints);
        for (size_t i = 0; i < numJoints; i++)
        {
            finalJointMatrices[i] = inverseTransform * (getNodeMatrix(skin.joints[i]) * skin.inverseBindMatrices[i]);
        }

        memcpy(skin.jointMatrixBuffer.pMappedData, finalJointMatrices.data(), finalJointMatrices.size() * sizeof(glm::mat4));
    }

    for (auto& child : node->children)
    {
        updateJoints(child);
    }
}

static void loadGLTFNode(gltfObject& obj, bool includeInAccel, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, uint32_t nodeIndex, gltfNode* parent, std::vector<gltfNode*>& parentNodes) {
    SMikkTSpaceContext mikktContext = { .m_pInterface = &MikkTInterface };

    auto node = new gltfNode();
    node->parent = parent;
    node->index = nodeIndex;
    node->skinIndex = nodeIn.skin;
	node->includeInAccel = includeInAccel;
    node->dynamic = dynamic;

    if (nodeIn.matrix.size() == 16) {
        glm::mat4 m = glm::make_mat4x4(nodeIn.matrix.data());
        node->worldTransform = m;
        
        // decompose for matComponents
        glm::vec3 translation = glm::vec3(m[3]);
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(m[0]));
        scale.y = glm::length(glm::vec3(m[1]));
        scale.z = glm::length(glm::vec3(m[2]));

        glm::mat4 rotMat = m;
        if (scale.x != 0.0f) rotMat[0] /= scale.x;
        if (scale.y != 0.0f) rotMat[1] /= scale.y;
        if (scale.z != 0.0f) rotMat[2] /= scale.z;

        glm::quat rotation = glm::quat_cast(rotMat);

        node->matComponents.position = translation;
        node->matComponents.rotation = rotation;
        node->matComponents.scale = scale;
    }
    else {
        if (nodeIn.translation.size() == 3) {
            glm::vec3 translation = glm::make_vec3(nodeIn.translation.data());
            node->worldTransform = glm::translate(node->worldTransform, translation);
            node->matComponents.position = translation;
        }
        if (nodeIn.rotation.size() == 4) {
            glm::quat q = glm::make_quat(nodeIn.rotation.data());
            node->worldTransform *= glm::mat4(q);
            node->matComponents.rotation = q;
        }
        if (nodeIn.scale.size() == 3) {
            glm::vec3 scale = glm::make_vec3(nodeIn.scale.data());
            node->worldTransform = glm::scale(node->worldTransform, scale);
            node->matComponents.scale = scale;
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

gltfObject gltfutils::loadFromFile(const std::string& filename, bool includeInAccel, bool dynamic, bool isCharacter) {
	std::cout << "Loading GLTF file: " << filename << std::endl;

	gltfObject object{};
    object.dynamic = dynamic;
    object.isCharacter = isCharacter;
    object.imageIsSRGB = new std::unordered_set<uint32_t>();
    tinygltf::Model* model;
    model = new tinygltf::Model();
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

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
        loadGLTFNode(object, includeInAccel, dynamic, model, node, -1, nullptr, object.parentNodes);
    }

    object.textureIndices.resize(model->textures.size());
    for (size_t i = 0; i < model->textures.size(); i++) {
        object.textureIndices[i] = model->textures[i].source;
    }

    object.materials.resize(model->materials.size());
    for (size_t i = 0; i < model->materials.size(); i++) {
        tinygltf::Material gltfMat = model->materials[i];
        if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
            object.materials[i].baseColorIndex = object.textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + 2; // image index
            object.imageIsSRGB->insert(object.materials[i].baseColorIndex - 2);
        }
        if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
            object.materials[i].normalIndex = object.textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + 2;
        }
        else { object.materials[i].normalIndex = DUMMY_NORMAL_TEX_INDEX; }
        if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
            object.materials[i].metallicRoughnessIndex = object.textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + 2;
        }
        else { object.materials[i].metallicRoughnessIndex = DUMMY_METALROUGH_TEX_INDEX; }
    }

    for (uint32_t i = 0; i < model->images.size(); i++) {
        VkFormat format = (object.imageIsSRGB->find(i) == object.imageIsSRGB->end()) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
        loadTexture(object, model, format, i);
    }

    Skin::loadSkins(model, object.parentNodes, object.skins);
    Animation::loadAnimations(model, object.parentNodes, object.animations);

    for (auto& skin : object.skins) {
        size_t skinJointMatrixSize = skin.joints.size() * sizeof(glm::mat4);
        skin.jointMatrixBuffer = vkdeviceutils::createBuffer(skinJointMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
    }
     
    for (auto node : object.parentNodes)
    {
        object.updateJoints(node);
    }

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

    // TODO: REALLY FUCKING INEFFICIENT, find some other way
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
                dynamicTransformMatrices.push_back(node->worldTransform);
            }
            else {
                staticTransformMatrices.push_back(node->worldTransform);
            }
            obj->numMatrices++;
            for (const auto& prim : node->primitives) {
                uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                uint32_t firstIndex = static_cast<uint32_t>(indices.size());
				uint32_t matIndex = prim->materialIndex + mat_offset;

                gltfDrawCommand draw{};
                draw.isCharacter = obj->isCharacter;
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
            }

            // for acceleration structures
            buildNodeBuffers(node);
		}
        
        for (const auto& mat : obj->materials) {
            GPUMaterialIndices newMatIndices{};
            newMatIndices.baseColorIndex = mat.baseColorIndex + numTextures;
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
        texImage = vkimageutils::createTextureImage((void*)pixels, imageExtents, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
        vkimageutils::createImageSampler(texImage);
        dummyTextures.push_back(texImage);
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
	vkdescriptorutils::flushDescriptorWrites();
}

void gltfObject::updateAnimation(float deltaTime, uint32_t currentBuffer)
{
    this->currentBuffer = currentBuffer;

    if (activeAnimation > static_cast<uint32_t>(animations.size()) - 1)
    {
        std::cout << "No animation with index " << activeAnimation << std::endl;
        return;
    }
    Animation& animation = animations[activeAnimation];
    animation.currentTime += deltaTime;
    if (animation.currentTime > animation.end)
    {
        animation.currentTime -= animation.end;
    }

    for (auto& channel : animation.channels)
    {
        AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
        for (size_t i = 0; i < sampler.inputs.size() - 1; i++)
        {
            if ((animation.currentTime >= sampler.inputs[i]) && (animation.currentTime <= sampler.inputs[i + 1]))
            {
                float a = (animation.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
                if (channel.path == "translation")
                {
                    channel.node->matComponents.position = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
                }
                if (channel.path == "rotation")
                {
                    glm::quat q1;
                    q1.x = sampler.outputsVec4[i].x;
                    q1.y = sampler.outputsVec4[i].y;
                    q1.z = sampler.outputsVec4[i].z;
                    q1.w = sampler.outputsVec4[i].w;

                    glm::quat q2;
                    q2.x = sampler.outputsVec4[i + 1].x;
                    q2.y = sampler.outputsVec4[i + 1].y;
                    q2.z = sampler.outputsVec4[i + 1].z;
                    q2.w = sampler.outputsVec4[i + 1].w;

                    channel.node->matComponents.rotation = glm::normalize(glm::slerp(q1, q2, a));
                }
                if (channel.path == "scale")
                {
                    channel.node->matComponents.scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
                }
            }
        }
    }
    for (auto& node : parentNodes)
    {
        updateJoints(node);
    }
}