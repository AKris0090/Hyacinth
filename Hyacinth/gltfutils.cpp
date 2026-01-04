#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "gltfutils.h"
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
    if (parent > 0) {
        obj.nodes[parent]->childrenIndices.push_back(obj.nodeCounter);
    }
    obj.nodes.push_back(std::move(node));

	gltfNode* nodePtr = obj.nodes[nodeIndex].get();
    if (parent > 0) {
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
            const tinygltf::Primitive& gltfPrims = mesh.primitives[i];
			auto prim = std::make_unique<gltfPrimitive>();
			nodePtr->primitives.push_back(std::move(prim));
			gltfPrimitive* p = obj.nodes[nodeIndex]->primitives[i].get();

            uint32_t currentNumIndices = 0;
            uint32_t currentNumVertices = 0;

            // FOR VERTICES
            const float* positionBuff = nullptr;
            const float* normalsBuff = nullptr;

            if (gltfPrims.attributes.find("POSITION") != gltfPrims.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrims.attributes.find("POSITION")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                positionBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                currentNumVertices = static_cast<uint32_t>(accessor.count);
            }
            if (gltfPrims.attributes.find("NORMAL") != gltfPrims.attributes.end()) {
                const tinygltf::Accessor& accessor = model->accessors[gltfPrims.attributes.find("NORMAL")->second];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                normalsBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            for (size_t vert = 0; vert < currentNumVertices; vert++) {
                Vertex v{};
                glm::vec3 normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));

                v.pos = glm::vec4(glm::make_vec3(&positionBuff[vert * 3]), 0.0f);
                v.normal = glm::vec4(normal, 0.0f);
				v.color = glm::vec4(0.5f);
                p->vertices.push_back(v);
            }

            const tinygltf::Accessor& accessor = model->accessors[gltfPrims.indices];
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

gltfObject gltfutils::loadFromFile(const std::string& filename) {
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
	delete model;
	return object;
}

void sceneGraph::buildSceneGraph() {
    for (const auto& obj : objects) {
        uint32_t currentNumMatrices = static_cast<uint32_t>(transformMatrices.size());

        for (const auto& node: obj.nodes) {
            transformMatrices.push_back(node.get()->worldTransform);
            for (const auto& prim : node.get()->primitives) {
                uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                uint32_t firstIndex = static_cast<uint32_t>(indices.size());

                VkDrawIndexedIndirectCommand drawCmd{};
				drawCmd.firstIndex = firstIndex;
                drawCmd.indexCount = prim.get()->indices.size();

                for (const auto& v : prim.get()->vertices) {
                    vertices.push_back(v);
                }
                for (const auto& index : prim.get()->indices) {
                    indices.push_back(index + firstVertex);
				}

                transformIndices.push_back(transformMatrices.size() - 1);
				drawCommands.push_back(drawCmd);
            }
		}
    }
}