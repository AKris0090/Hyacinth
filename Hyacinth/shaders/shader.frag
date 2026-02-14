#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "probeCommon.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) flat in int colorSamplerIndex;
layout	(location = 1) flat in int normalSamplerIndex;
layout	(location = 2) flat in int metalRoughSamplerIndex;
layout  (location = 3) in vec4 viewPos;
layout  (location = 4) in vec4 inNormal;
layout	(location = 5) in vec4 fragPos;
layout	(location = 6) in mat3 TBNMatrix;
layout	(location = 9) in vec2 inUV;
layout (set = 0, binding = 1) uniform sampler2DArray shadowDepthMap;

layout(set = 2, binding = 0) uniform sampler2DArray irradianceTex;
layout(set = 2, binding = 1) uniform sampler2DArray visibilityTex;

#include "bufferInfo.glsl"

layout( push_constant ) uniform constants
{
	TransformBuffer transformBuffer;
	MaterialBuffer materialBuffer;
	DrawDataBuffer drawDataBuffer;
	ProbePositionBuffer probePosBuffer;
    VolumeDataBuffer volumeDataBuffer;
    int volumeIndex;
} pc;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

layout(set = 1, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outColor;

const vec3 lightColor = vec3(0.99, 0.98, 0.83);

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
    const float biasModifier = 1.0;
    float scaledBias = max(0.005 * (1.0 - dot(normalize(inNormal.xyz), (ubo.lightPos.xyz - fragPos.xyz))), 0.005);
    if (cascadeIndex == SHADOW_MAP_CASCADE_COUNT - 1)
    {
        scaledBias *= 1.0 / (ubo.cascadeSplits[SHADOW_MAP_CASCADE_COUNT - 1] * biasModifier);
    }
    else
    {
     scaledBias *= 1.0 / (ubo.cascadeSplits[cascadeIndex] * biasModifier);
    }

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowDepthMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - scaledBias) {
			shadow = 0.0;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(shadowDepthMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
	 		shadowFactor += textureProj(sc, vec2(dx * x, dy * y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
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
    vec4 sampledColor = texture(globalTextures2D[colorSamplerIndex], inUV);
    vec4 metalRough = texture(globalTextures2D[metalRoughSamplerIndex], inUV);

    vec3 N = texture(globalTextures2D[normalSamplerIndex], inUV).xyz;
    N = normalize(N * 2.0 - 1.0);
    N = normalize(TBNMatrix * N);

    vec3 V    = normalize(ubo.viewPos.xyz - fragPos.xyz);
    vec3 L = normalize(ubo.lightPos.xyz - fragPos.xyz);

    vec3 radiance = lightColor * vec3(3.0);

    float NdotL = max(dot(N, L), 0.0);
    float halfLambert = (NdotL * 0.5) + 0.5;
    vec3 diffuse = (sampledColor.rgb / PI) * halfLambert * radiance;

    vec3 r = reflect(-L, N);
    float specular = max(0.0, dot(r, V));
    specular = pow(specular, 16.0) * metalRough.b;

    vec3 irrad = DDGIGetIrradiance(fragPos.xyz, N, ubo.viewPos.xyz);
    vec3 ambient = (sampledColor.rgb / PI) * irrad;

    uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(viewPos.z < ubo.cascadeSplits[i]) {
			cascadeIndex = i + 1;
		}
	}

    vec4 shadowCoord = (biasMat * ubo.cascadeViewProj[cascadeIndex]) * vec4(fragPos.xyz, 1.0);	 
	float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);

    vec3 color = ambient + ((diffuse + vec3(specular)) * shadow);

    outColor = vec4(color, 1.0f);

    if (ubo.viewPos.w == 1.0) {
        outColor = vec4(irrad, 1.0);
    }
}