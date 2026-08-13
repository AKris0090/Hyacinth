#pragma once

#include "transform.h"
#include "glm/gtx/hash.hpp"

struct LightVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec4 tangent;

    // animated members
    glm::vec4 jointIndices;
    glm::vec4 jointWeights;

    bool operator==(const LightVertex& other) const {
        return pos == other.pos && uv == other.uv && normal == other.normal && tangent == other.tangent && jointIndices == other.jointIndices && jointWeights == other.jointWeights;
    }
};

namespace std {
    template<> struct hash<LightVertex> {
        size_t operator()(LightVertex const& vertex) const {
            return (hash<glm::vec3>()(vertex.pos) ^ hash<glm::vec2>()(vertex.uv) ^ hash<glm::vec3>()(vertex.normal) ^ hash<glm::vec4>()(vertex.tangent) ^ hash<glm::vec4>()(vertex.jointIndices) ^ hash<glm::vec4>()(vertex.jointWeights));
        }
    };
}

struct LightGeomCapsule {
    std::vector<LightVertex>& v;
    std::vector<uint32_t>& i;
};