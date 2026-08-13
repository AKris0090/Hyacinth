#pragma once

#include "mikktspace.h"
#include "light_common.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

// MIKKTSPACE TANGENT FUNCTIONS, REFERENCED OFF OF: https://github.com/Eearslya/glTFView. SUCH AN AMAZING PERSON
class MikkTSpaceHelper {
public:

	struct MikkTContext {
		LightGeomCapsule* mesh;
	};

	static int MikkTGetNumFaces(const SMikkTSpaceContext* context) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		return static_cast<int>(data->mesh->v.size() / 3);
	}

	static int MikkTGetNumVerticesOfFace(const SMikkTSpaceContext* context, const int face) {
		return 3;
	}

	static void MikkTGetPosition(const SMikkTSpaceContext* context, float fvPosOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		const glm::vec3 pos = data->mesh->v[face * 3 + vert].pos;
		fvPosOut[0] = pos.x;
		fvPosOut[1] = pos.y;
		fvPosOut[2] = pos.z;
	}

	static void MikkTGetNormal(const SMikkTSpaceContext* context, float fvNormOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		const glm::vec3 norm = data->mesh->v[face * 3 + vert].normal;
		fvNormOut[0] = norm.x;
		fvNormOut[1] = norm.y;
		fvNormOut[2] = norm.z;
	}

	static void MikkTGetTexCoord(const SMikkTSpaceContext* context, float fvTexcOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		glm::vec2 uv = data->mesh->v[face * 3 + vert].uv;
		fvTexcOut[0] = uv.x;
		fvTexcOut[1] = 1.f - uv.y;
	}

	static void MikkTSetTSpaceBasic(
		const SMikkTSpaceContext* context, const float fvTangent[], const float fSign, const int face, const int vert) {
		auto data = reinterpret_cast<MikkTContext*>(context->m_pUserData);

		data->mesh->v[static_cast<std::vector<LightVertex, std::allocator<LightVertex>>::size_type>(face) * 3 + vert].tangent = glm::vec4(glm::make_vec3(fvTangent), fSign);
	}
};

static SMikkTSpaceInterface MikkTInterface = { .m_getNumFaces = MikkTSpaceHelper::MikkTGetNumFaces,
											   .m_getNumVerticesOfFace = MikkTSpaceHelper::MikkTGetNumVerticesOfFace,
											   .m_getPosition = MikkTSpaceHelper::MikkTGetPosition,
											   .m_getNormal = MikkTSpaceHelper::MikkTGetNormal,
											   .m_getTexCoord = MikkTSpaceHelper::MikkTGetTexCoord,
											   .m_setTSpaceBasic = MikkTSpaceHelper::MikkTSetTSpaceBasic,
											   .m_setTSpace = nullptr };

static void generateTangents(LightGeomCapsule* p, SMikkTSpaceContext& mikktContext) {
	//UNPACK VERTICES
	std::vector<LightVertex> unpacked(p->i.size());
	uint32_t newInd = 0;
	for (uint32_t index : p->i) {
		unpacked[newInd] = p->v[static_cast<std::vector<LightVertex, std::allocator<LightVertex>>::size_type>(index)];
		newInd++;
	}
	p->v = std::move(unpacked);
	p->i.clear();

	// GEN TANGENT SPACE
	MikkTSpaceHelper::MikkTContext context{ p };
	mikktContext.m_pUserData = &context;
	genTangSpaceDefault(&mikktContext);

	//WELD VERTICES
	p->i.clear();
	p->i.reserve(p->v.size());
	std::unordered_map<LightVertex, uint32_t> uniqueVertices;

	size_t oldVertexCount = p->v.size();
	uint32_t postTVertexCount = 0;
	for (size_t i = 0; i < oldVertexCount; ++i) {
		LightVertex v = p->v[i];

		auto index = uniqueVertices.find(v);
		if (index == uniqueVertices.end()) {
			uint32_t vertIndex = postTVertexCount;
			postTVertexCount++;
			uniqueVertices.insert(std::make_pair(v, vertIndex));
			p->v[vertIndex] = v;
			p->i.push_back(vertIndex);
		}
		else {
			p->i.push_back(index->second);
		}
	}
	p->v.resize(postTVertexCount);
}