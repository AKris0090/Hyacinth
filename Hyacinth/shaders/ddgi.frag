#version 460

#include "probeCommon.glsl"
#include "shadowCommon.glsl"
#include "bufferInfo.glsl"

layout 	(set = 0, binding = 1) uniform sampler2D normalMap;
layout	(set = 0, binding = 2) uniform sampler2D depthMap;
layout  (set = 0, binding = 3) uniform sampler2D ddgiImage;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout (set = 2, binding = 0) uniform sampler2DArray irradianceTex;
layout (set = 2, binding = 1) uniform sampler2DArray visibilityTex;

layout( push_constant ) uniform constants
{
    VolumeDataBuffer volumeDataBuffer;
    int volumeIndex;
} pc;

layout 	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

const vec3 lightColor = vec3(0.99, 0.98, 0.83);

vec3 worldPosFromDepth(float depth) {
    vec2 ndcXY = inUV * 2.0 - 1.0;
    vec4 ndc   = vec4(ndcXY, depth, 1.0);

    vec4 viewSpace = inverse(ubo.proj) * ndc;
    viewSpace /= viewSpace.w;

    return (inverse(ubo.view) * viewSpace).xyz;
}

vec3 DDGIGetIrradiance(vec3 worldPosition, vec3 normal, vec3 cameraPos) {
    VolumeData volume = pc.volumeDataBuffer.data[pc.volumeIndex];

    ivec3 probeCounts = ivec3(volume.width, volume.height, volume.depth);
    const vec3 Wo = normalize(cameraPos.xyz - worldPosition);
    const float minimum_distance_between_probes = 1.0;
    vec3 surfaceBias = (normal * 0.2f + Wo * 0.8f) * (0.75f * minimum_distance_between_probes) * 0.3; // last is self-shadow bias

    ivec3 irradianceTextureSize = textureSize(irradianceTex, 0);
    ivec3 visibilityTextureSize = textureSize(visibilityTex, 0);
    vec3 biasedWorldPos = (worldPosition + surfaceBias);

    ivec3 baseProbeCoords = getBaseProbeCoords(biasedWorldPos, volume.inverseSpacing, volume.pos, probeCounts);
    vec3 baseProbeWorldPosition = getProbeWorldPos(baseProbeCoords, volume.spacing, volume.pos);

    vec3 alpha = clamp((biasedWorldPos - baseProbeWorldPosition) / volume.spacing, vec3(0.0), vec3(1.0));

    vec3 sumIrradiance = vec3(0.0);
    float accumulatedWeights = 0.0;

    for (int probeIndex = 0; probeIndex < 8; ++probeIndex) {
        ivec3 offset = ivec3(probeIndex, probeIndex >> 1, probeIndex >> 2) & ivec3(1, 1, 1);
        ivec3 adjacentProbeCoords = clamp(baseProbeCoords + offset, ivec3(0), probeCounts - ivec3(1));

        vec3  adjacentProbeWorldPos = getProbeWorldPos(adjacentProbeCoords, volume.spacing, volume.pos);

        vec3 worldToAdjProbe = normalize(adjacentProbeWorldPos - worldPosition);
        vec3 biasedToAdjProbe = normalize(adjacentProbeWorldPos - biasedWorldPos);
        float biasedPosToAdjProbeDist = length(adjacentProbeWorldPos - biasedWorldPos);

        vec3 trilinear = max(vec3(0.001f), mix(1.f - alpha, alpha, offset));
        float trilinearWeight = (trilinear.x * trilinear.y * trilinear.z);
        float weight = 1.0;

        float wrapShading = (dot(worldToAdjProbe, normal) + 1.0) * 0.5;
        weight *= (wrapShading * wrapShading) + 0.2;

        vec2 probeUV = oct_encode(-biasedToAdjProbe) * 0.5 + 0.5;
        int altProbeIndex = ProbeCoordsToIndex(adjacentProbeCoords, probeCounts);

        ivec3 visBase = getAtlasPosition(altProbeIndex, VISIBILITY_INNER + 2, volume.width, volume.depth);
        vec2 visAtlasUV;
        visAtlasUV.x = (float(visBase.x) + probeUV.x * float(VISIBILITY_INNER)) / float(visibilityTextureSize.x);
        visAtlasUV.y = (float(visBase.y) + probeUV.y * float(VISIBILITY_INNER)) / float(visibilityTextureSize.y);

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
        ivec3 base = getAtlasPosition(altProbeIndex, IRRADIANCE_INNER + 2, volume.width, volume.depth);
        vec2 atlasUV;
        atlasUV.x = (float(base.x) + probeUV.x * float(IRRADIANCE_INNER)) / float(irradianceTextureSize.x);
        atlasUV.y = (float(base.y) + probeUV.y * float(IRRADIANCE_INNER)) / float(irradianceTextureSize.y);
        vec3 probeIrradiance = texture(irradianceTex, vec3(atlasUV, base.z)).rgb;

        sumIrradiance += (weight * probeIrradiance);
        accumulatedWeights += weight;
    }

    vec3 netIrradiance = sumIrradiance / accumulatedWeights;

    vec3 irradiance = 0.5f * PI * netIrradiance * 0.95f;

    return irradiance;
}

void main() {
	vec3 fragPos = worldPosFromDepth(texture(depthMap, inUV).r);
	vec4 Nshadow = texture(normalMap, inUV);
    vec3 N = Nshadow.xyz * 2.0 - 1.0;
	vec3 irrad = DDGIGetIrradiance(fragPos, N, ubo.viewPos.xyz);

	outColor = vec4(irrad, 1.0);
}  