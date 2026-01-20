#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

#include "probeCommon.glsl"
#include "bufferInfo.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) rayPayloadInEXT vec4 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 barycentricWeights;

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosAddress;
	VertexBuffer vertexBufferAddress;
	IndexBuffer indexBufferAddress;
	MaterialIntBuffer materialBuffer;
} pc;

const vec3 lightPos = vec3(-2.0, 12.0, -6.0);
const float lightIntensity = 8.0;

Vertex unpack(uint index)
{
	// Unpack the vertices from the SSBO using the glTF vertex structure
	// The multiplier is the size of the vertex divided by four float components (=16 bytes)
	const int m = ubo.vertexSize / 16;

	vec4 d0 = vertices.v[m * index + 0];
	vec4 d1 = vertices.v[m * index + 1];
	vec4 d2 = vertices.v[m * index + 2];

	Vertex v;
	v.pos = d0.xyz;
	v.normal = vec3(d0.w, d1.x, d1.y);
	v.color = vec4(d2.x, d2.y, d2.z, 1.0);

	return v;
}

void main()
{
	vec3 radiance = vec3(0.0);
	float distance = 0.0;
	if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
		distance = gl_RayTminEXT + gl_HitTEXT;
	} else {
		float b = barycentricWeights.x;
        float c = barycentricWeights.y;
        float a = 1 - b - c;

		uvec3 tri = pc.indexBufferAddress.triangles[gl_PrimitiveID];
		Vertex v0 = pc.vertexBufferAddress.vertices[tri.x];
		Vertex v1 = pc.vertexBufferAddress.vertices[tri.y];
		Vertex v2 = pc.vertexBufferAddress.vertices[tri.z];
		vec3 normal = normalize(a * v0.normal.xyz + b * v1.normal.xyz + c * v2.normal.xyz);

		vec2 uv0 = vec2(v0.position.w, v0.normal.w);
		vec2 uv1 = vec2(v1.position.w, v1.normal.w);
		vec2 uv2 = vec2(v2.position.w, v2.normal.w);
		vec2 uv = a * uv0 + b * uv1 + c * uv2;

		vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
		distance = gl_RayTminEXT + gl_HitTEXT;

		vec3 albedo = texture(globalTextures2D[pc.materialBuffer.mats[gl_PrimitiveID]], uv).rgb;

		vec3 lightVector = normalize(lightPos); // directional light

		const float nDotL = clamp(dot(normal, lightVector), 0.0, 1.0);

		uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		uint cullMask = 0xff;
		float tmin = 0.01;
		float tmax = 1000.0;

		shadowed = true;
		traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 1, worldPos, tmin, lightVector, tmax, 2);

		vec3 intensity = vec3(0.0);
		if(nDotL > 0.001 && !shadowed) {
			//intensity += lightIntensity * nDotL;
		}
		vec3 diffuse = albedo;

		radiance = normal.xyz;
		distance = gl_RayTminEXT + gl_HitTEXT;
	}
	hitValue = vec4(radiance, distance);
}