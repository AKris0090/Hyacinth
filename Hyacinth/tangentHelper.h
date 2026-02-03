#pragma once

#include "mikktspace/mikktspace.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "gltfutils.h"

// MIKKTSPACE TANGENT FUNCTIONS, REFERENCED OFF OF: https://github.com/Eearslya/glTFView. SUCH AN AMAZING PERSON
class MikkTSpaceHelper {
public:

	struct MikkTContext {
		gltfPrimitive* mesh;
	};

	static int MikkTGetNumFaces(const SMikkTSpaceContext* context) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		return static_cast<int>(data->mesh->vertices.size() / 3);
	}

	static int MikkTGetNumVerticesOfFace(const SMikkTSpaceContext* context, const int face) {
		return 3;
	}

	static void MikkTGetPosition(const SMikkTSpaceContext* context, float fvPosOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		const glm::vec3 pos = data->mesh->vertices[face * 3 + vert].pos;
		fvPosOut[0] = pos.x;
		fvPosOut[1] = pos.y;
		fvPosOut[2] = pos.z;
	}

	static void MikkTGetNormal(const SMikkTSpaceContext* context, float fvNormOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		const glm::vec3 norm = data->mesh->vertices[face * 3 + vert].normal;
		fvNormOut[0] = norm.x;
		fvNormOut[1] = norm.y;
		fvNormOut[2] = norm.z;
	}

	static void MikkTGetTexCoord(const SMikkTSpaceContext* context, float fvTexcOut[], const int face, const int vert) {
		const auto data = reinterpret_cast<const MikkTContext*>(context->m_pUserData);
		glm::vec2 uv = glm::vec2(data->mesh->vertices[face * 3 + vert].pos.w, data->mesh->vertices[face * 3 + vert].normal.w);
		fvTexcOut[0] = uv.x;
		fvTexcOut[1] = 1.f - uv.y;
	}

	static void MikkTSetTSpaceBasic(
		const SMikkTSpaceContext* context, const float fvTangent[], const float fSign, const int face, const int vert) {
		auto data = reinterpret_cast<MikkTContext*>(context->m_pUserData);

		data->mesh->vertices[static_cast<std::vector<Vertex, std::allocator<Vertex>>::size_type>(face) * 3 + vert].tangent = glm::vec4(glm::make_vec3(fvTangent), fSign);
	}
};

static SMikkTSpaceInterface MikkTInterface = { .m_getNumFaces = MikkTSpaceHelper::MikkTGetNumFaces,
											   .m_getNumVerticesOfFace = MikkTSpaceHelper::MikkTGetNumVerticesOfFace,
											   .m_getPosition = MikkTSpaceHelper::MikkTGetPosition,
											   .m_getNormal = MikkTSpaceHelper::MikkTGetNormal,
											   .m_getTexCoord = MikkTSpaceHelper::MikkTGetTexCoord,
											   .m_setTSpaceBasic = MikkTSpaceHelper::MikkTSetTSpaceBasic,
											   .m_setTSpace = nullptr };
