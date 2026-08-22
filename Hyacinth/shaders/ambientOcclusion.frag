#version 460

#include "probeCommon.glsl"
#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
	vec4 ABOD; // ambient toggle, bias, offset scale, ddgi intensity
	mat4 globalShadowMatrix;
	vec4 cascadeSplits;
	mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
	vec4 cascadeOffsets[SHADOW_MAP_CASCADE_COUNT];
	vec4 cascadeScales[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout 	(set = 1, binding = 0) uniform sampler2D albedoMap;
layout 	(set = 1, binding = 1) uniform sampler2D normalMap;
layout	(set = 1, binding = 3) uniform sampler2D depthMap;

layout  (set = 2, binding = 0) uniform sampler2D noiseImage;

const uint kernelSize = 16;
layout	(set = 2, binding = 1) uniform NoiseBufferObject {
	mat4 inverseProj;
	vec4 samples[kernelSize];
	vec2 screenSize;
} noiseUBO;

layout	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

const float radius = 0.5;

vec3 viewPosFromDepth(float depth) {
    vec2 ndcXY = inUV * 2.0 - 1.0;
    vec4 ndc   = vec4(ndcXY, depth, 1.0);

    vec4 viewSpace = noiseUBO.inverseProj * ndc;
    viewSpace /= viewSpace.w;

    return viewSpace.xyz;
}

void main() {
	vec2 noiseScale = noiseUBO.screenSize / vec2(4.0);

	vec3 fragPos = viewPosFromDepth(texture(depthMap, inUV).r);
	vec3 normal = normalize(ubo.view * vec4((texture(normalMap, inUV).xyz * 2.f - 1.f), 0.0)).xyz; // should be view pos
	vec3 randomVec = texture(noiseImage, inUV * noiseScale).xyz;

	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);  

	float occlusion = 0.0;
	for (int i = 0; i < kernelSize; ++i) {
		vec3 samplePos = TBN * noiseUBO.samples[i].xyz;
		samplePos = fragPos + samplePos * radius;

		vec4 offset = vec4(samplePos, 1.0);
		offset      = ubo.proj * offset;				// from view to clip-space
		offset.xyz /= offset.w;							// perspective divide
		offset.xy = offset.xy * 0.5 + 0.5;

		float sampleDepth = viewPosFromDepth(texture(depthMap, offset.xy).r).z;
		float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z ? 1.0 : 0.0) * rangeCheck;  
	}

	occlusion = 1.0 - (occlusion / kernelSize);
	outColor = vec4(occlusion, 0.0, 0.0, 1.0);
}  