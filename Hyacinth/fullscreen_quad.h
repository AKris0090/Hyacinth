#pragma once

#include "vkmeshutils.h"

constexpr int QUAD_INDEX_COUNT = 6;
constexpr int QUAD_VERTEX_COUNT = 4;

const std::array<glm::vec3, QUAD_VERTEX_COUNT> fullscreenQuadPositions = {
	glm::vec3(-1.0f, -1.0f, 0.0f),		// 0: bottom-left
	glm::vec3(1.0f, -1.0f, 0.0f),		// 1: bottom-right
	glm::vec3(1.0f,  1.0f, 0.0f),		// 2: top-right
	glm::vec3(-1.0f,  1.0f, 0.0f),		// 3: top-left
};

const std::array<glm::vec2, 4> fullscreenQuadUVs = {
	glm::vec2(0.0f, 0.0f), // 0
	glm::vec2(1.0f, 0.0f), // 1
	glm::vec2(1.0f, 1.0f), // 2
	glm::vec2(0.0f, 1.0f), // 3
};

const std::array<uint32_t, 6> fullscreenQuadIndices = {
	0, 1, 2,
	0, 2, 3,
};

class FullscreenQuad {
public: 
	static void addFullscreenQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
		for (int i = 0; i < fullscreenQuadPositions.size(); i++) {
			Vertex vert{};
			vert.pos = glm::vec4(fullscreenQuadPositions[i], 1.f);
			vert.uvs = glm::vec4(fullscreenQuadUVs[i].x, fullscreenQuadUVs[i].y, 0.f, 0.f);
			vertices.push_back(vert);
		}
		for (auto& i : fullscreenQuadIndices) {
			indices.push_back(i);
		}
	}
};