#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

#include "probeCommon.glsl"
#include "bufferInfo.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2) uniform sampler2DArray irradianceTex;
layout(set = 0, binding = 3) uniform sampler2DArray visibilityTex;

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

vec3 DDGIGetSurfaceBias(vec3 surfaceNormal, vec3 cameraDirection)
{
    return (surfaceNormal * 0.1) + (-cameraDirection * 0.3);
}

vec3 DDGIGetIrradiance(vec3 worldPosition, vec3 normal, vec3 rayDir) {
    vec3 surfaceBias = DDGIGetSurfaceBias(normal, rayDir);

    ivec3 irradianceTextureSize = textureSize(irradianceTex, 0);
    ivec3 visibilityTextureSize = textureSize(visibilityTex, 0);
    vec3 biasedWorldPos = (worldPosition + surfaceBias);

    ivec3 baseProbeCoords = getBaseProbeCoords(biasedWorldPos);
    vec3 baseProbeWorldPosition = getProbeWorldPos(baseProbeCoords);

    vec3 alpha = clamp((biasedWorldPos - baseProbeWorldPosition) / probeSpacing, vec3(0.0), vec3(1.0));

    vec3 sumIrradiance = vec3(0.0);
    float accumulatedWeights = 0.0;

    for (int probeIndex = 0; probeIndex < 8; ++probeIndex) {
        ivec3 offset = ivec3(probeIndex, probeIndex >> 1, probeIndex >> 2) & ivec3(1, 1, 1);
        ivec3 adjacentProbeCoords = clamp(baseProbeCoords + offset, ivec3(0), probeCounts - ivec3(1));

        vec3  adjacentProbeWorldPos = getProbeWorldPos(adjacentProbeCoords);

        vec3 worldToAdjProbe = normalize(adjacentProbeWorldPos - worldPosition);
        vec3 biasedToAdjProbe = normalize(adjacentProbeWorldPos - biasedWorldPos);
        float biasedPosToAdjProbeDist = length(adjacentProbeWorldPos - biasedWorldPos);

        vec3 trilinear = max(vec3(0.001f), mix(1.f - alpha, alpha, offset));
        float trilinearWeight = (trilinear.x * trilinear.y * trilinear.z);
        float weight = 1.0;

        float wrapShading = (dot(worldToAdjProbe, normal) + 1.0) * 0.5;
        weight *= (wrapShading * wrapShading) + 0.2;

        vec2 probeUV = oct_encode(-biasedToAdjProbe) * 0.5 + 0.5;
        int altProbeIndex = ProbeCoordsToIndex(adjacentProbeCoords);

        ivec3 visBase = getAtlasPosition(altProbeIndex, VISIBILITY_TILE_WIDTH);
        vec2 visAtlasUV;
        visAtlasUV.x = (float(visBase.x) + probeUV.x * float(VISIBILITY_INNER_RES)) / float(visibilityTextureSize.x);
        visAtlasUV.y = (float(visBase.y) + probeUV.y * float(VISIBILITY_INNER_RES)) / float(visibilityTextureSize.y);

        vec2 filteredDistance = texture(visibilityTex, vec3(visAtlasUV, visBase.z)).rg;
        float variance = abs((filteredDistance.x * filteredDistance.x) - filteredDistance.y);

        float chebyshevWeight = 1.f;
        if (biasedPosToAdjProbeDist > filteredDistance.x)
        {
            float v = biasedPosToAdjProbeDist - filteredDistance.x;
            chebyshevWeight = variance / (variance + (v * v));

            chebyshevWeight = max((chebyshevWeight * chebyshevWeight * chebyshevWeight), 0.f);
        }

        weight *= max(0.05f, chebyshevWeight);

        weight = max(0.000001f, weight);

        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
        {
            weight *= (weight * weight) * (1.f / (crushThreshold * crushThreshold));
        }
        weight *= trilinearWeight;

        probeUV = oct_encode(normal) * 0.5 + 0.5;
        ivec3 base = getAtlasPosition(altProbeIndex, IRRADIANCE_TILE_WIDTH);
        vec2 atlasUV;
        atlasUV.x = (float(base.x) + probeUV.x * float(IRRADIANCE_INNER_RES)) / float(irradianceTextureSize.x);
        atlasUV.y = (float(base.y) + probeUV.y * float(IRRADIANCE_INNER_RES)) / float(irradianceTextureSize.y);
        vec3 probeIrradiance = texture(irradianceTex, vec3(atlasUV, base.z)).rgb;

        sumIrradiance += (weight * probeIrradiance);
        accumulatedWeights += weight;
    }

    vec3 netIrradiance = sumIrradiance / accumulatedWeights;

    vec3 irradiance = 0.5f * PI * netIrradiance * 0.95f;
    return irradiance;
}

const vec3 lightPos = vec3(-2.0, 12.0, -6.0);
const float directLightIntensity = 3.0;

void main()
{
	vec3 radiance = vec3(0.0);
	float distance = 0.0;
	distance = gl_RayTminEXT + gl_HitTEXT;
	if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
		distance *= -0.2;
	} else {
		ivec3 index = ivec3(pc.indexBufferAddress.indices[3 * gl_PrimitiveID], pc.indexBufferAddress.indices[3 * gl_PrimitiveID + 1], pc.indexBufferAddress.indices[3 * gl_PrimitiveID + 2]);
		float b = barycentricWeights.x;
        float c = barycentricWeights.y;
        float a = 1 - b - c;

		Vertex v0 = pc.vertexBufferAddress.vertices[index.x];
		Vertex v1 = pc.vertexBufferAddress.vertices[index.y];
		Vertex v2 = pc.vertexBufferAddress.vertices[index.z];

		vec3 normal = normalize(a * v0.normal.xyz + b * v1.normal.xyz + c * v2.normal.xyz);

		vec2 uv0 = vec2(v0.position.w, v0.normal.w);
		vec2 uv1 = vec2(v1.position.w, v1.normal.w);
		vec2 uv2 = vec2(v2.position.w, v2.normal.w);
		vec2 uv = a * uv0 + b * uv1 + c * uv2;

		vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

		vec3 albedo = texture(globalTextures2D[pc.materialBuffer.mats[gl_PrimitiveID]], uv).rgb;

		vec3 lightVector = normalize(lightPos); // directional light

		uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		uint cullMask = 0xff;
		float tmin = 0.01;
		float tmax = 1000.0;

		shadowed = true;
		traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 1, worldPos, tmin, lightVector, tmax, 2);

		vec3 intensity = vec3(directLightIntensity);

        float NdotL = max(dot(normal, lightVector), 0.0);
        float halfLambert = (NdotL * 0.5) + 0.5;

		if(shadowed) {
		    intensity *= vec3(0.1);
		}

		vec3 directDiffuse = halfLambert * intensity;

        float maxAlbedo = 0.9f;

		vec3 irradiance = DDGIGetIrradiance(worldPos, normal, gl_WorldRayDirectionEXT);

		radiance = directDiffuse + ((min(albedo, vec3(maxAlbedo, maxAlbedo, maxAlbedo)) / PI) * irradiance);
	}
	hitValue = vec4(radiance, distance);
}