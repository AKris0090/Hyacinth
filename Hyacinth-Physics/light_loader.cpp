#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "light_loader.h"

void LightLoader::loadNode(LightObject* obj, bool dynamic, const tinygltf::Model* model, const tinygltf::Node& nodeIn, int32_t parent) {
    auto node = std::make_unique<LightNode>();
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

    uint32_t nodeIndex = obj->nodeCounter++;
    std::lock_guard<std::mutex> guard(obj->objectMutex);
    obj->nodes.push_back(std::move(node));

    LightNode* nodePtr = obj->nodes[nodeIndex].get();
    if (parent >= 0) {
        obj->nodes[parent]->childrenIndices.push_back(nodeIndex);
        nodePtr->worldTransform = obj->nodes[parent]->worldTransform * nodePtr->worldTransform;
    }

    if (nodeIn.children.size() > 0) {
        for (size_t i = 0; i < nodeIn.children.size(); i++) {
            loadNode(obj, dynamic, model, model->nodes[nodeIn.children[i]], nodeIndex);
        }
    }

    if (nodeIn.mesh > -1) {
        const tinygltf::Mesh mesh = model->meshes[nodeIn.mesh];
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& gltfPrim = mesh.primitives[i];
            auto prim = std::make_unique<LightPrimitive>();
            nodePtr->primitives.push_back(std::move(prim));
            LightPrimitive* p = obj->nodes[nodeIndex]->primitives.back().get();
            p->materialIndex = gltfPrim.material;

            uint32_t currentNumIndices = 0;
            uint32_t currentNumVertices = 0;

            // FOR VERTICES
            const float* positionBuff = nullptr;

            if (gltfPrim.attributes.find("POSITION") != gltfPrim.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("POSITION")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                positionBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                currentNumVertices = static_cast<uint32_t>(accessor.count);
            }

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                glm::vec3 pos = glm::make_vec3(&positionBuff[vert * 3]);
                p->vertices.push_back(pos);
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

LightObject* LightLoader::loadFromFile(const std::string& filename, bool dynamic) {
    std::cout << "[LIGHTLOADER] loading physics object from: " << filename << std::endl;

    LightObject* object = new LightObject();
    object->dynamic = dynamic;
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
    
    std::for_each(std::execution::par, std::begin(scene.nodes), std::end(scene.nodes), [&](int nodeIndex) {
        const tinygltf::Node& node = model->nodes[nodeIndex];
        loadNode(object, dynamic, model, node, -1);
    });

    delete model;
    return object;
}