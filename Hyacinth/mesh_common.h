#pragma once

#include "vkmeshutils.h"
#include "animation.h"

struct HPrimGeomCapsule {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

struct HMeshPrim {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexCount;

	uint32_t meshID;
	uint32_t materialIndex;

	AABB boundingBox;
};

struct HRenderCall {
	glm::mat4 transformMatrix;
	alignas(16) glm::vec3 aaBBMin;
	alignas(16) glm::vec3 aabbMax;

	uint32_t	materialIndex;
	uint32_t    indexCount;
	uint32_t    firstIndex;
	uint32_t    vertexOffset;

	uint32_t meshID;
};