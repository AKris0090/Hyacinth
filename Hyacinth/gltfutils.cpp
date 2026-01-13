#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "gltfutils.h"
#include "vkimageutils.h"
#include <glm/gtc/type_ptr.hpp>
#include "tangentHelper.h"

static void generateTangents(gltfPrimitive* p, SMikkTSpaceContext& mikktContext) {
    //UNPACK VERTICES
    std::vector<Vertex> unpacked(p->indices.size());
    uint32_t newInd = 0;
    for (uint32_t index : p->indices) {
        unpacked[newInd] = p->vertices[static_cast<std::vector<Vertex, std::allocator<Vertex>>::size_type>(index)];
        newInd++;
    }
    p->vertices = std::move(unpacked);
    p->indices.clear();

    // GEN TANGENT SPACE
    MikkTSpaceHelper::MikkTContext context{ p };
    mikktContext.m_pUserData = &context;
    genTangSpaceDefault(&mikktContext);

    //WELD VERTICES
    p->indices.clear();
    p->indices.reserve(p->vertices.size());
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    size_t oldVertexCount = p->vertices.size();
    uint32_t postTVertexCount = 0;
    for (size_t i = 0; i < oldVertexCount; ++i) {
        Vertex v = p->vertices[i];

        auto index = uniqueVertices.find(v);
        if (index == uniqueVertices.end()) {
            uint32_t vertIndex = postTVertexCount;
            postTVertexCount++;
            uniqueVertices.insert(std::make_pair(v, vertIndex));
            p->vertices[vertIndex] = v;
            p->indices.push_back(vertIndex);
        }
        else {
            p->indices.push_back(index->second);
        }
    }
    p->vertices.resize(postTVertexCount);
}

static void loadGLTFNode(gltfObject& obj, const tinygltf::Model* model, const tinygltf::Node& nodeIn, int32_t parent) {
    SMikkTSpaceContext mikktContext = { .m_pInterface = &MikkTInterface };

    auto node = std::make_unique<gltfNode>();
    node->parentIndex = parent;

    if (nodeIn.matrix.size() == 16) {
        node->worldTransform = glm::make_mat4x4(nodeIn.matrix.data());
    }
    else {
        if (nodeIn.translation.size() == 3) {
            node->worldTransform = glm::translate(node->worldTransform, glm::vec3(glm::make_vec3(nodeIn.translation.data())));
        }
        if (nodeIn.rotation.size() == 4) {
            glm::quat q = glm::make_quat(nodeIn.rotation.data());
            node->worldTransform *= glm::mat4(q);
        }
        if (nodeIn.scale.size() == 3) {
            node->worldTransform = glm::scale(node->worldTransform, glm::vec3(glm::make_vec3(nodeIn.scale.data())));
        }
    }

    uint32_t nodeIndex = obj.nodeCounter++;
    obj.nodes.push_back(std::move(node));

    gltfNode* nodePtr = obj.nodes[nodeIndex].get();
    if (parent >= 0) {
        obj.nodes[parent]->childrenIndices.push_back(nodeIndex);
        nodePtr->worldTransform = obj.nodes[parent]->worldTransform * nodePtr->worldTransform;
    }

    if (nodeIn.children.size() > 0) {
        for (size_t i = 0; i < nodeIn.children.size(); i++) {
            loadGLTFNode(obj, model, model->nodes[nodeIn.children[i]], nodeIndex);
        }
    }

    if (nodeIn.mesh > -1) {
        const tinygltf::Mesh mesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = mesh.primitives[i];
			auto prim = std::make_unique<gltfPrimitive>();
			nodePtr->primitives.push_back(std::move(prim));
			gltfPrimitive* p = obj.nodes[nodeIndex]->primitives.back().get();
            p->materialIndex = gltfPrim.material;

            uint32_t currentNumIndices = 0;
            uint32_t currentNumVertices = 0;

            // FOR VERTICES
            const float* positionBuff = nullptr;
            const float* normalsBuff = nullptr;
            const float* uvBuff = nullptr;
            const float* tangentsBuff = nullptr;

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

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                Vertex v{};
                glm::vec3 normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));

                glm::vec2 uv = uvBuff ? glm::make_vec2(&uvBuff[vert * 2]) : glm::vec3(0.0f);

                v.pos = glm::vec4(glm::make_vec3(&positionBuff[vert * 3]), uv.x);
                v.normal = glm::vec4(normal, uv.y);
                v.tangent = tangentsBuff ? glm::make_vec4(&tangentsBuff[vert * 4]) : glm::vec4(0.0f);
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
}

void gltfutils::loadTexture(DeviceContext& ctx, gltfObject& object, tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
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
    texImage = vkimageutils::createImage(ctx, rgba.data(), imageExtents, format, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
    vkimageutils::createImageSampler(*ctx.device, texImage);
    object.textures.push_back(texImage);
    std::cout << "created image: " << curImage.name << std::endl;
}

gltfObject gltfutils::loadFromFile(const std::string& filename, DeviceContext& ctx) {
	std::cout << "Loading GLTF file: " << filename << std::endl;

	gltfObject object{};
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
        loadGLTFNode(object, model, node, -1);
    }

    object.textureIndices.resize(model->textures.size());
    for (size_t i = 0; i < model->textures.size(); i++) {
        object.textureIndices[i] = model->textures[i].source;
    }

    // + 1 for dummy textures TODO: update when all added
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
        loadTexture(ctx, object, model, format, i);
    }

	delete model;
	return object;
}

void SceneGraph::buildNodeBuffers(DeviceContext& ctx, gltfNode* node) {
    // for building acceleration structures
    VkDeviceSize vertexBufferSize = node->vertices.size() * sizeof(glm::vec3);
    VkDeviceSize indexBufferSize = node->indices.size() * sizeof(uint32_t);
    node->nodeVertexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    node->nodeIndexBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0);

    VulkanBuffer staging = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(staging.info.pMappedData, node->vertices.data(), vertexBufferSize);
    memcpy((char*)staging.info.pMappedData + vertexBufferSize, node->indices.data(), indexBufferSize);

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.srcOffset = 0;
    vertexCopyRegion.dstOffset = 0;
    vertexCopyRegion.size = vertexBufferSize;

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.srcOffset = vertexBufferSize;
    indexCopyRegion.dstOffset = 0;
    indexCopyRegion.size = indexBufferSize;

    vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
        vkCmdCopyBuffer(cmd, staging.buffer, node->nodeVertexBuffer.buffer, 1, &vertexCopyRegion);
        vkCmdCopyBuffer(cmd, staging.buffer, node->nodeIndexBuffer.buffer, 1, &indexCopyRegion);
        });

    vkdeviceutils::destroyBuffer(*ctx.allocator, staging);
}

void SceneGraph::buildSceneGraph(DeviceContext& ctx) {
    uint32_t materialOffset = 0;
    uint32_t textureOffset = 0;
    uint32_t drawID = 0;
    uint32_t matrixID = 0;
    for (const auto& obj : objects) {
        uint32_t currentNumMatrices = static_cast<uint32_t>(transformMatrices.size());

        for (const auto& node: obj.nodes) {
            transformMatrices.push_back(node.get()->worldTransform);
            for (const auto& prim : node.get()->primitives) {
                uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                uint32_t firstIndex = static_cast<uint32_t>(indices.size());

                gltfDrawCommand drawCmd{};
				drawCmd.firstIndex = firstIndex;
                drawCmd.indexCount = static_cast<uint32_t>(prim.get()->indices.size());
                drawCmd.instanceCount = 1;
                drawCmd.firstInstance = drawID;
                drawCmd.vertexCount = prim.get()->vertices.size();

                for (const auto& v : prim.get()->vertices) {
                    vertices.push_back(v);
                }
                for (const auto& index : prim.get()->indices) {
                    indices.push_back(index + firstVertex);
				}

                sortedDrawCalls[prim.get()->materialIndex + materialOffset].push_back(drawCmd);

                DrawData primDrawData{};
                primDrawData.materialIndex = prim.get()->materialIndex + materialOffset;
                primDrawData.transformIndex = matrixID;
                drawData.push_back(primDrawData);

                drawID++;

                // acceleration structure-specific
                uint32_t nodeVertOffset = node.get()->vertices.size();
                for (const auto& v : prim.get()->vertices) {
                    node.get()->vertices.push_back(glm::vec3(v.pos));
                }
                for (const auto& i : prim.get()->indices) {
                    node.get()->indices.push_back(i + nodeVertOffset);
                }
            }
            matrixID++;

            // for acceleration structures
            buildNodeBuffers(ctx, node.get());
		}
        
        for (const auto& mat : obj.materials) {
            GPUMaterialIndices newMatIndices{};
            newMatIndices.baseColorIndex = mat.baseColorIndex + textureOffset;
            newMatIndices.normalIndex = (mat.normalIndex == DUMMY_NORMAL_TEX_INDEX) ? DUMMY_NORMAL_TEX_INDEX : mat.normalIndex + textureOffset;
            newMatIndices.metallicRoughnessIndex = (mat.metallicRoughnessIndex == DUMMY_METALROUGH_TEX_INDEX) ? DUMMY_METALROUGH_TEX_INDEX : mat.metallicRoughnessIndex + textureOffset;
            materialObjects.push_back(newMatIndices);
        }
        materialOffset += obj.materials.size();
        textureOffset += obj.textures.size();
        numNodes++;
    }

    numTextures = textureOffset + 1;
    for (const auto& [key, value] : sortedDrawCalls) {
        for (const auto& gltfDraw : value) {
            VkDrawIndexedIndirectCommand drawCmd{};
            drawCmd.firstIndex = gltfDraw.firstIndex;
            drawCmd.indexCount = gltfDraw.indexCount;
            drawCmd.instanceCount = gltfDraw.instanceCount;
            drawCmd.firstInstance = gltfDraw.firstInstance;

            drawCommands.push_back(drawCmd);
        }
    }
}

void SceneGraph::createDummyTextures(DeviceContext& ctx) {
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
        texImage = vkimageutils::createImage(ctx, (void*)pixels, imageExtents, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true); // also creates imageView
        vkimageutils::createImageSampler(*ctx.device, texImage);
        dummyTextures.push_back(texImage);
    }
}

void SceneGraph::uploadTextures(VkDevice& dev, VkDescriptorSet& descriptor) {
    uint32_t textureOffset = 0;
    for (const auto& tex : dummyTextures) {
        vkimageutils::storeTexture(dev, descriptor, tex, textureOffset);
        textureOffset++;
    }
    for (const auto& obj : objects) {
        for (const auto& tex : obj.textures) {
            vkimageutils::storeTexture(dev, descriptor, tex, textureOffset);
            textureOffset++;
        }
    }
}