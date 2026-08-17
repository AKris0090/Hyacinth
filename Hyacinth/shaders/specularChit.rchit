#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

#include "probeCommon.glsl"
#include "bufferInfo.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 3, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 barycentricWeights;

layout( push_constant ) uniform constants
{
	VertexBuffer vertexBufferAddress;
	IndexBuffer indexBufferAddress;
	RenderCallBuffer renderBufferAddress;
	MaterialBuffer materialBufferAddress;
	vec3 camPos;
} pc;

void main()
{
	RenderCall r = pc.renderBufferAddress.calls[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
	uint idx0 = r.firstIndex + 3 * gl_PrimitiveID;
	ivec3 index = ivec3(pc.indexBufferAddress.indices[idx0], pc.indexBufferAddress.indices[idx0 + 1], pc.indexBufferAddress.indices[idx0 + 2]);
	float b = barycentricWeights.x;
    float c = barycentricWeights.y;
    float a = 1 - b - c;

	Vertex v0 = pc.vertexBufferAddress.vertices[index.x + r.vertexOffset];
	Vertex v1 = pc.vertexBufferAddress.vertices[index.y + r.vertexOffset];
	Vertex v2 = pc.vertexBufferAddress.vertices[index.z + r.vertexOffset];

	vec2 uv0 = vec2(v0.position.w, v0.normal.w);
	vec2 uv1 = vec2(v1.position.w, v1.normal.w);
	vec2 uv2 = vec2(v2.position.w, v2.normal.w);
	vec2 uv = a * uv0 + b * uv1 + c * uv2;

	Material mat = pc.materialBufferAddress.mats[r.materialIndex];
	vec3 albedo = texture(globalTextures2D[mat.baseColorIndex], uv).xyz;

	payload.radiance = albedo;
}