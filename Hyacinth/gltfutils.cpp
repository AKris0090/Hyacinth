#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "gltfutils.h"
#include "vkimageutils.h"
#include <glm/gtc/type_ptr.hpp>

static void loadGLTFNode(gltfObject& obj, const tinygltf::Model* model, const tinygltf::Node& nodeIn, int32_t parent) {
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

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                Vertex v{};
                glm::vec3 normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));

                glm::vec2 uv = uvBuff ? glm::make_vec2(&uvBuff[vert * 2]) : glm::vec3(0.0f);

                v.pos = glm::vec4(glm::make_vec3(&positionBuff[vert * 3]), uv.x);
                v.normal = glm::vec4(normal, uv.y);
                v.color = glm::vec4(
                    static_cast<float>(rand()) / RAND_MAX,
                    static_cast<float>(rand()) / RAND_MAX,
                    static_cast<float>(rand()) / RAND_MAX,
                    1.0f
                );
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
        }
    }
}

void gltfutils::loadTexture(DeviceContext& ctx, gltfObject& object, tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
    // TODO: update as adding more dummy textures
    if (imageIndex <= 0) {
        return;
    }
    // TODO: update with num dummy textures as you add more
    tinygltf::Image& curImage = model->images[imageIndex - 1];
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
            rgba[j] = rgb[j];
            rgba[j + 1] = rgb[j + 1];
            rgba[j + 2] = rgb[j + 2];
            rgba[j + 3] = 255;
        }
    }
    case 1:
    {
        std::vector<unsigned char> r;
        r.resize(curImage.width * curImage.height);
        memcpy(r.data(), curImage.image.data(), r.size());
        for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
            rgba[j] = rgba[j + 1] = rgba[j + 2] = r[j];
            rgba[j + 3] = 255;
        }
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

    // + 1 for dummy textures
    object.materials.resize(model->materials.size());
    for (size_t i = 0; i < model->materials.size(); i++) {
        tinygltf::Material gltfMat = model->materials[i];
        if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
            object.materials[i].baseColorIndex = object.textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + 1; // image index
        }
        if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
            object.materials[i].normalIndex = object.textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + 1;
        }
        else { object.materials[i].normalIndex = DUMMY_NORMAL_TEX_INDEX; }
    }

    for (size_t i = 0; i < object.materials.size(); i++) {
        // load color images
        uint32_t colorIndex = object.materials[i].baseColorIndex;
        loadTexture(ctx, object, model, VK_FORMAT_R8G8B8A8_SRGB, colorIndex);

        uint32_t normalIndex = object.materials[i].normalIndex;
        loadTexture(ctx, object, model, VK_FORMAT_R8G8B8A8_UNORM, normalIndex);
    }

	delete model;
	return object;
}

void SceneGraph::buildSceneGraph() {
    uint32_t materialOffset = 0;
    uint32_t textureOffset = 1;
    uint32_t drawID = 0;
    uint32_t matrixID = 0;
    for (const auto& obj : objects) {
        uint32_t currentNumMatrices = static_cast<uint32_t>(transformMatrices.size());

        for (const auto& node: obj.nodes) {
            transformMatrices.push_back(node.get()->worldTransform);
            for (const auto& prim : node.get()->primitives) {
                uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                uint32_t firstIndex = static_cast<uint32_t>(indices.size());

                VkDrawIndexedIndirectCommand drawCmd{};
				drawCmd.firstIndex = firstIndex;
                drawCmd.indexCount = static_cast<uint32_t>(prim.get()->indices.size());
                drawCmd.instanceCount = 1;
                drawCmd.firstInstance = drawID;

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
            }
            matrixID++;
		}
        
        for (const auto& mat : obj.materials) {
            GPUMaterialIndices newMatIndices{};
            newMatIndices.baseColorIndex = mat.baseColorIndex + textureOffset;
            newMatIndices.normalIndex = (mat.normalIndex == DUMMY_NORMAL_TEX_INDEX) ? DUMMY_NORMAL_TEX_INDEX : mat.normalIndex + textureOffset;
            materialObjects.push_back(newMatIndices);
        }
        materialOffset += obj.materials.size();
        textureOffset += obj.textures.size();
    }

    numTextures = textureOffset;
    for (const auto& [key, value] : sortedDrawCalls) {
        drawCommands.insert(drawCommands.end(), value.begin(), value.end());
    }
}

void SceneGraph::createDummyTextures(DeviceContext& ctx) {
    // add dummy textures
    VulkanImage texImage{};
    stbi_uc* pixels = nullptr;
    int texWidth, texHeight, texChannels;
    pixels = stbi_load("./shaders/dummyNormal.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("failed to load normal dummy image!");
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