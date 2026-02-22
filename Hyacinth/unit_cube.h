#pragma once

#include "vkmeshutils.h"

constexpr int UNIT_CUBE_INDEX_COUNT = 36;

const std::array<glm::vec3, 8> unitCubePositions = {
	glm::vec3(-0.5f, -0.5f,  0.5f),
	glm::vec3(-0.5f,  0.5f,  0.5f),
	glm::vec3(-0.5f, -0.5f, -0.5f),
	glm::vec3(-0.5f,  0.5f, -0.5f),
	glm::vec3(0.5f, -0.5f,  0.5f),
	glm::vec3(0.5f,  0.5f,  0.5f),
	glm::vec3(0.5f, -0.5f, -0.5f),
	glm::vec3(0.5f,  0.5f, -0.5f)
};

const std::array<uint32_t, UNIT_CUBE_INDEX_COUNT> unitCubeIndices = {
	0, 4, 5, // +Z (front)
	0, 5, 1, // +Z (front)
	2, 3, 7, // -Z (back)
	2, 7, 6, // -Z (back)
	0, 1, 3, // -X (left)
	0, 3, 2, // -X (left)
	4, 6, 7, // +X (right)
	4, 7, 5, // +X (right)
	1, 5, 7, // +Y (top)
	1, 7, 3, // +Y (top)
	0, 2, 6, // -Y (bottom)
	0, 6, 4  // -Y (bottom)
};

class UnitCube {
public:
	static void addUnitCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
		for (int i = 0; i < unitCubePositions.size(); i++) {
			Vertex vert{};
			vert.pos = glm::vec4(unitCubePositions[i], 0.f);
			vert.normal = glm::vec4(glm::vec3(0.f), 0.f);
			vertices.push_back(vert);
		}
		for (auto& i : unitCubeIndices) {
			indices.push_back(i);
		}
	}
};