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
};

struct HRenderCall {
	glm::mat4 transformMatrix;
	uint32_t materialIndex;

	uint32_t    indexCount;
	uint32_t    firstIndex;
	uint32_t    vertexOffset;

	alignas(16) uint32_t meshID;
};