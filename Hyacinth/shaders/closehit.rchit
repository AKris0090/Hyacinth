#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

#include "probeCommon.glsl"
#include "bufferInfo.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 barycentricWeights;

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosAddress;
	VertexBuffer vertexBufferAddress;
	IndexBuffer indexBufferAddress;
} pc;

const vec3 lightColor = vec3(1.0); // vec3(0.99, 0.98, 0.83);
const vec3 lightPos = vec3(-2.0, 12.0, -6.0);

void main()
{
	if (payload.depth == 0) {
		payload.distance = gl_RayTminEXT + gl_HitTEXT;
		if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
			payload.distance *= -1.0;
			return;
		}
	}
	vec3 radiance = vec3(0.0);
	if (gl_HitKindEXT != gl_HitKindBackFacingTriangleEXT) {
		ivec3 index = ivec3(pc.indexBufferAddress.indices[3 * gl_PrimitiveID], pc.indexBufferAddress.indices[3 * gl_PrimitiveID + 1], pc.indexBufferAddress.indices[3 * gl_PrimitiveID + 2]);
		float b = barycentricWeights.x;
        float c = barycentricWeights.y;
        float a = 1 - b - c;

		Vertex v0 = pc.vertexBufferAddress.vertices[index.x];
		Vertex v1 = pc.vertexBufferAddress.vertices[index.y];
		Vertex v2 = pc.vertexBufferAddress.vertices[index.z];

		vec3 normal = normalize(a * v0.normal.xyz + b * v1.normal.xyz + c * v2.normal.xyz);

		vec3 lightVector = normalize(lightPos); // directional light

		uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		uint cullMask = 0xff;
		float tmin = 0.01;
		float tmax = 1000.0;
		float epsilon = 0.001;
		vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * epsilon;

		shadowed = true;
		traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 1, origin, tmin, lightVector, tmax, 2);

        float NdotL = max(dot(normal, lightVector), 0.0);
        float halfLambert = (NdotL * 0.5) + 0.5;
		vec3 directDiffuse = halfLambert * lightColor * 1.25;

        if(shadowed) {
		    directDiffuse *= vec3(0.3);
		}

		payload.radiance = payload.radiance + directDiffuse;

		if (payload.depth < MAX_DEPTH) {
			rayFlags = gl_RayFlagsOpaqueEXT;
		    payload.depth++;
		    vec3 newDir = reflect(gl_WorldRayDirectionEXT, normal);
		    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, newDir, tmax, 0);
		}
	}
}