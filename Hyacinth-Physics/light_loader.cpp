#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "light_loader.h"

void LightLoader::loadNode(LightObject* obj, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, LightNode* parent) {
    LightNode* node = new LightNode();
    node->parentNode = parent;

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
            loadNode(obj, dynamic, model, model->nodes[nodeIn.children[i]], node);
        }
    }

    if (nodeIn.mesh > -1) {
        const tinygltf::Mesh mesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = mesh.primitives[i];
            LightPrimitive* p = new LightPrimitive();
            node->primitives.push_back(p);
            p->materialIndex = gltfPrim.material;

            uint32_t currentNumIndices = 0;
            uint32_t currentNumVertices = 0;

            // FOR VERTICES
            const float* positionBuff = nullptr;
            const float* normalBuff = nullptr;
            const float* texCoordBuff = nullptr;
            const float* tangentBuff = nullptr;

            if (gltfPrim.attributes.find("POSITION") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("POSITION")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                positionBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                currentNumVertices = static_cast<uint32_t>(accessor.count);
            }
            if (gltfPrim.attributes.find("NORMAL") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("NORMAL")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                normalBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (gltfPrim.attributes.find("TEXCOORD_0") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("TEXCOORD_0")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                texCoordBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (gltfPrim.attributes.find("TANGENT") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("TANGENT")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                tangentBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                glm::vec3 pos = glm::make_vec3(&positionBuff[vert * 3]);
                glm::vec3 normal = glm::normalize(glm::vec3(normalBuff ? glm::make_vec3(&normalBuff[vert * 3]) : glm::vec3(0.0f)));
                glm::vec2 uv = texCoordBuff ? glm::make_vec2(&texCoordBuff[vert * 2]) : glm::vec3(0.0f);
                glm::vec4 tangent = tangentBuff ? glm::make_vec4(&tangentBuff[vert * 4]) : glm::vec4(0.0f);

                p->vertices.push_back(pos);
                p->normals.push_back(normal);
                p->uvs.push_back(uv);
                p->tangents.push_back(tangent);
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

    if (parent == nullptr) {
        obj->parentNodes.push_back(node);
    }
    else {
        parent->children.push_back(node);
    }
}

LightObject* LightLoader::loadFromFile(const std::string& filename, bool dynamic) {
    std::cout << "[LIGHTLOADER] loading physics object from: " << filename << std::endl;

    LightObject* object = new LightObject();
    tinygltf::Model* model;
    model = new tinygltf::Model();
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

    bool loaded = false;
    if (getFileExtension(filename) == "glb") {
        loaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, filename);
    }
    else {
        loaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, filename);
    }

    std::cout << "[LIGHTLOADER] ERRORS: " << error.c_str() << std::endl;
    std::cout << "[LIGHTLOADER] WARNINGS: " << warning.c_str() << std::endl;

    std::cout << std::endl;

    if (!loaded) {
        throw std::runtime_error("[LIGHTLOADER] Failed to load glTF file: " + filename);
    }

    const tinygltf::Scene& scene = model->scenes[model->defaultScene];
    
    for(const auto& nodeIndex : scene.nodes) {
        const tinygltf::Node& node = model->nodes[nodeIndex];
        loadNode(object, dynamic, model, node, nullptr);
    }

    delete model;
    return object;
}