#include "gltfutils.h"
#include "vkimageutils.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tangentHelper.h"
#include <unordered_set>

#include "tiny_gltf.h"

// AABB getBoundingBox(std::vector<Vertex>& vertices) {
//     AABB bounds;
//     bounds.min = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
//     bounds.max = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
//     for (const auto& v : vertices) {
//         bounds.grow(v);
//     }
//     return bounds;
// }
// 
// AABB getWorldSpaceBoundingBox(gltfNode* node) {
//     AABB bounds;
//     bounds.min = glm::vec4(glm::vec3(FLT_MAX), 1.f);
//     bounds.max = glm::vec4(glm::vec3(FLT_MIN), 1.f);
//     glm::mat4 worldMatrix = node);
//     for (const auto& v : node->vertices) {
//         bounds.grow(worldMatrix * glm::vec4(v.pos.x, v.pos.y, v.pos.z, 1.f));
//     }
//     for (const auto& n : node->children) {
//         bounds.grow(getWorldSpaceBoundingBox(n));
//     }
// 
//     return bounds;
// }

VulkanImage loadTexture(tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
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

    if (curImage.name.empty()) {
        curImage.name = "image_" + curImage.uri;
    }
    std::cout << "created image: " << curImage.name << std::endl;

    return texImage;
}

void loadSkins(tinygltf::Model* input, HSkinnedMesh& mesh) {
	tinygltf::Skin glTFSkin = input->skins[0];
	mesh.jointMatrixSize = glTFSkin.joints.size() * sizeof(glm::mat4);
    HSkin& skinOut = mesh.skin;
	
	for (int jointIndex : glTFSkin.joints)
	{
	    HSkinnedMeshNode* node = mesh.getNodeByIndex(jointIndex);
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
}

void loadAnimations(tinygltf::Model* input, HSkinnedMesh& mesh)
{
    mesh.animations.resize(input->animations.size());

    for (size_t i = 0; i < input->animations.size(); i++)
    {
        tinygltf::Animation glTFAnimation = input->animations[i];

        mesh.animations[i].samplers.resize(glTFAnimation.samplers.size());
        for (size_t j = 0; j < glTFAnimation.samplers.size(); j++)
        {
            tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
            HAnimSampler& dstSampler = mesh.animations[i].samplers[j];
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
                for (auto input : mesh.animations[i].samplers[j].inputs)
                {
                    if (input < mesh.animations[i].start)
                    {
                        mesh.animations[i].start = input;
                    };
                    if (input > mesh.animations[i].end)
                    {
                        mesh.animations[i].end = input;
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
                    std::cout << "unknown type" << std::endl;
                    break;
                }
                }
            }
        }

        mesh.animations[i].channels.resize(glTFAnimation.channels.size());
        for (size_t j = 0; j < glTFAnimation.channels.size(); j++)
        {
            tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
            HAnimChannel& dstChannel = mesh.animations[i].channels[j];
            dstChannel.path = glTFChannel.target_path;
            dstChannel.samplerIndex = glTFChannel.sampler;
            dstChannel.node = mesh.getNodeByIndex(glTFChannel.target_node);
        }
    }
}

template<typename MESH, typename MESHNODE>
void loadGLTFNode(const tinygltf::Model* model, const tinygltf::Node& nodeIn, MESH& mesh, uint32_t nodeIndex, MESHNODE* parent, HAssetDrawer* scene, uint32_t materialOffset) {
    SMikkTSpaceContext mikktContext = { .m_pInterface = &MikkTInterface };

    MESHNODE* node = new MESHNODE();
    node->parent = parent;
    node->nodeIndex = nodeIndex;
    node->nodeName = nodeIn.name;

    if (nodeIn.skin > 0) {
        mesh.meshOwnerNode = node;
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
            mesh.numNodes++;
            loadGLTFNode<MESH, MESHNODE>(model, model->nodes[nodeIn.children[i]], mesh, nodeIn.children[i], node, scene, materialOffset);
        }
    }

    if (nodeIn.mesh > -1) {
        mesh.numMeshedNodes++;
        const tinygltf::Mesh gltfMesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < gltfMesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = gltfMesh.primitives[i];
            HPrimGeomCapsule primCap;
            HMeshPrim p;
            p.materialIndex = gltfPrim.material + materialOffset;
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
                primCap.vertices.push_back(v);
            }

            const tinygltf::Accessor& accessor = model->accessors[gltfPrim.indices];
            const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model->buffers[view.buffer];

            switch (accessor.componentType) {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primCap.indices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primCap.indices.push_back(buf[index]);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                for (size_t index = 0; index < accessor.count; index++) {
                    primCap.indices.push_back(buf[index]);
                }    
                break;
            }
            default:
                std::cout << "index component type not supported" << std::endl;
                throw std::runtime_error("index component type not supported");
            }

            p.firstIndex = scene->indices.size();
            p.indexCount = primCap.indices.size();
            p.firstVertex = scene->vertices.size();
            p.vertexCount = primCap.vertices.size();
            p.meshID = mesh.meshID;

            if (!tangentsBuff) {
                generateTangents(&primCap, mikktContext);
            }

            for (auto& v : primCap.vertices) {
                scene->vertices.push_back(v);
            }

            for (auto& i : primCap.indices) {
                scene->indices.push_back(i);
            }

            node->primtives.push_back(p);
        }
    }

    if (parent) {
        parent->children.push_back(node);
        if (nodeIn.mesh > -1) mesh.meshedNodes.push_back(parent->children[parent->children.size() - 1]);
    }
    else {
        mesh.parentNodes.push_back(node);
        if (nodeIn.mesh > -1) mesh.meshedNodes.push_back(mesh.parentNodes[mesh.parentNodes.size() - 1]);
    }
}
namespace gltfutils {
    void gltfutils::loadAnimatedMesh(HAssetDrawer& scene, std::string path, std::string meshName) {
        HSkinnedMesh mesh;
        mesh.firstIndex = scene.indices.size();
        mesh.vertexOffset = scene.vertices.size();
        mesh.meshID = scene.meshCount;
        mesh.meshName = meshName;

        tinygltf::Model* model = new tinygltf::Model();
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        bool loaded = false;
        if (getFilePathExtension(path) == "glb") {
            loaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, path);
        }
        else {
            loaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, path);
        }

        std::cout << "ERRORS: " << error.c_str() << std::endl;
        std::cout << "WARNINGS: " << warning.c_str() << std::endl;

        if (!loaded) {
            throw std::runtime_error("Failed to load glTF file: " + path);
        }

        uint32_t materialOffset = static_cast<uint32_t>(scene.materials.size());
        const tinygltf::Scene& gltfScene = model->scenes[model->defaultScene];
        for (size_t i = 0; i < gltfScene.nodes.size(); i++) {
            const tinygltf::Node node = model->nodes[gltfScene.nodes[i]];
            loadGLTFNode<HSkinnedMesh, HSkinnedMeshNode>(model, node, mesh, -1, nullptr, &scene, materialOffset);
        }

        uint32_t textureOffset = scene.textures.size();

        std::vector<uint32_t> textureIndices(model->textures.size());
        for (size_t i = 0; i < model->textures.size(); i++) {
            textureIndices[i] = model->textures[i].source;
        }

        std::unordered_set<uint32_t> imageIsSRGB;
        for (size_t i = 0; i < model->materials.size(); i++) {
            MaterialInstance material;
            tinygltf::Material gltfMat = model->materials[i];
            if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
                material.baseColorIndex = textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + textureOffset; // 3 for all dummy textures
                imageIsSRGB.insert(material.baseColorIndex - textureOffset);
            }
            else { material.baseColorIndex = DUMMY_COLOR_TEX_INDEX; }
            if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
                material.normalIndex = textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + textureOffset;
            }
            else { material.normalIndex = DUMMY_NORMAL_TEX_INDEX; }
            if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
                material.metallicRoughnessIndex = textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + textureOffset;
            }
            else { material.metallicRoughnessIndex = DUMMY_METALROUGH_TEX_INDEX; }
            material.alphaCutoff = gltfMat.alphaCutoff;

            scene.materials.push_back(material);
        }

        for (uint32_t i = 0; i < model->images.size(); i++) {
            VkFormat format = (imageIsSRGB.find(i) == imageIsSRGB.end()) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
            scene.textures.push_back(loadTexture(model, format, i));
        }

        loadSkins(model, mesh);
        loadAnimations(model, mesh);

        scene.skinnedMeshes[meshName] = mesh;
        scene.meshCount++;

        delete model;
    }

    void gltfutils::loadStaticMesh(HAssetDrawer& scene, std::string path, std::string meshName) {
        HMesh mesh;
        mesh.firstIndex = scene.indices.size();
        mesh.vertexOffset = scene.vertices.size();
        mesh.meshID = scene.meshCount;
        mesh.meshName = meshName;

        tinygltf::Model* model = new tinygltf::Model();
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        bool loaded = false;
        if (getFilePathExtension(path) == "glb") {
            loaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, path);
        }
        else {
            loaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, path);
        }

        std::cout << "ERRORS: " << error.c_str() << std::endl;
        std::cout << "WARNINGS: " << warning.c_str() << std::endl;

        if (!loaded) {
            throw std::runtime_error("Failed to load glTF file: " + path);
        }

        uint32_t materialOffset = static_cast<uint32_t>(scene.materials.size());
        const tinygltf::Scene& gltfScene = model->scenes[model->defaultScene];
        for (size_t i = 0; i < gltfScene.nodes.size(); i++) {
            const tinygltf::Node node = model->nodes[gltfScene.nodes[i]];
            loadGLTFNode<HMesh, HMeshNode>(model, node, mesh, -1, nullptr, &scene, materialOffset);
        }

        uint32_t textureOffset = scene.textures.size();

        std::vector<uint32_t> textureIndices(model->textures.size());
        for (size_t i = 0; i < model->textures.size(); i++) {
            textureIndices[i] = model->textures[i].source;
        }

        std::unordered_set<uint32_t> imageIsSRGB;
        for (size_t i = 0; i < model->materials.size(); i++) {
            MaterialInstance material;
            tinygltf::Material gltfMat = model->materials[i];
            if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
                material.baseColorIndex = textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()] + textureOffset;
                imageIsSRGB.insert(material.baseColorIndex - textureOffset);
            }
            else { material.baseColorIndex = DUMMY_COLOR_TEX_INDEX; }
            if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
                material.normalIndex = textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + textureOffset;
            }
            else { material.normalIndex = DUMMY_NORMAL_TEX_INDEX; }
            if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
                material.metallicRoughnessIndex = textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + textureOffset;
            }
            else { material.metallicRoughnessIndex = DUMMY_METALROUGH_TEX_INDEX; }
            material.alphaCutoff = gltfMat.alphaCutoff;

            scene.materials.push_back(material);
        }

        for (uint32_t i = 0; i < model->images.size(); i++) {
            VkFormat format = (imageIsSRGB.find(i) == imageIsSRGB.end()) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
            scene.textures.push_back(loadTexture(model, format, i));
        }

        scene.meshes[meshName] = mesh;
        scene.meshCount++;

        delete model;
    }
}