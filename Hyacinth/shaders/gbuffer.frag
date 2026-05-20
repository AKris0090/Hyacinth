#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require

#include "shadowCommon.glsl"
#include "bufferInfo.glsl"

layout	(location = 0) flat in int matIndex;
layout  (location = 1) in vec4 viewPos;
layout  (location = 2) in vec4 inNormal;
layout	(location = 3) in vec4 fragPos;
layout	(location = 4) in mat3 TBNMatrix;
layout	(location = 7) in vec2 inUV;

layout (set = 1, binding = 0) uniform sampler2DArrayShadow shadowDepthMap;

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

layout( push_constant ) uniform constants
{
	mat4 entityTransformMatrix;
	TransformBuffer transformBuffer;
	DrawDataBuffer drawDataBuffer;
	JointMatricesBuffer jmBuffer;
	MaterialBuffer materialBuffer;
    VolumeDataBuffer volumeDataBuffer;
} pc;

layout(set = 2, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

float sampleShadowMap(vec2 baseUV, float u, float v, vec2 shadowMapSizeInv, uint cascadeIndex, float depth, vec2 receiverPlaneDepthBias) {
	vec2 uv = baseUV + vec2(u, v) * shadowMapSizeInv;
	float z = depth + dot(vec2(u, v) * shadowMapSizeInv, receiverPlaneDepthBias);
	return texture(shadowDepthMap, vec4(uv, cascadeIndex, z));
}

float sampleCascadeMap(vec3 shadowPos, vec3 shadowPosDx, vec3 shadowPosDy, uint cascadeIndex) {
	shadowPos += ubo.cascadeOffsets[cascadeIndex].xyz;
	shadowPos *= ubo.cascadeScales[cascadeIndex].xyz;

	shadowPosDx *= ubo.cascadeScales[cascadeIndex].xyz;
	shadowPosDy *= ubo.cascadeScales[cascadeIndex].xyz;

	// sample shadow optimized pcf 
	vec3 shadowMapSize = vec3(textureSize(shadowDepthMap, 0).xyz);

	float lightDepth = shadowPos.z - ubo.ABOD.y;
	vec2 receiverPlaneDepthBias = vec2(0.0, 0.0);

	vec2 uv = shadowPos.xy * shadowMapSize.xy;
	vec2 shadowMapSizeInv = 1.0 / shadowMapSize.xy;

	vec2 baseUV;
	baseUV.x = floor(uv.x + 0.5);
	baseUV.y = floor(uv.y + 0.5);

	float s = (uv.x + 0.5 - baseUV.x);
    float t = (uv.y + 0.5 - baseUV.y);

	baseUV -= vec2(0.5);
    baseUV *= shadowMapSizeInv;

	float sum = 0;

	float uw0 = (5 * s - 6);
    float uw1 = (11 * s - 28);
    float uw2 = -(11 * s + 17);
    float uw3 = -(5 * s + 1);

    float u0 = (4 * s - 5) / uw0 - 3;
    float u1 = (4 * s - 16) / uw1 - 1;
    float u2 = -(7 * s + 5) / uw2 + 1;
    float u3 = -s / uw3 + 3;

    float vw0 = (5 * t - 6);
    float vw1 = (11 * t - 28);
    float vw2 = -(11 * t + 17);
    float vw3 = -(5 * t + 1);

    float v0 = (4 * t - 5) / vw0 - 3;
    float v1 = (4 * t - 16) / vw1 - 1;
    float v2 = -(7 * t + 5) / vw2 + 1;
    float v3 = -t / vw3 + 3;

    sum += uw0 * vw0 * sampleShadowMap(baseUV, u0, v0, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw0 * sampleShadowMap(baseUV, u1, v0, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw0 * sampleShadowMap(baseUV, u2, v0, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw0 * sampleShadowMap(baseUV, u3, v0, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw1 * sampleShadowMap(baseUV, u0, v1, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw1 * sampleShadowMap(baseUV, u1, v1, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw1 * sampleShadowMap(baseUV, u2, v1, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw1 * sampleShadowMap(baseUV, u3, v1, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw2 * sampleShadowMap(baseUV, u0, v2, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw2 * sampleShadowMap(baseUV, u1, v2, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw2 * sampleShadowMap(baseUV, u2, v2, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw2 * sampleShadowMap(baseUV, u3, v2, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw3 * sampleShadowMap(baseUV, u0, v3, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw3 * sampleShadowMap(baseUV, u1, v3, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw3 * sampleShadowMap(baseUV, u2, v3, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw3 * sampleShadowMap(baseUV, u3, v3, shadowMapSizeInv, cascadeIndex, lightDepth, receiverPlaneDepthBias);

    return sum * 1.0f / 2704;
}

// offset sampling the shadow map based on the surface normal
vec3 getShadowSamplePosOffset(float nDotL, vec3 normal) {
	vec3 shadowMapSize = vec3(textureSize(shadowDepthMap, 0).xyz);
	float texelSize = 2.0 / shadowMapSize.x;
	float normalOffsetScale = clamp(1.0 - nDotL, 0.0, 1.0);
	return texelSize * ubo.ABOD.z * normalOffsetScale * normal;
}

float shadowTest(vec3 worldPos, float viewSpaceDepth, float nDotL, vec3 normal) {
	float shadowVis = 1.0;

	// select shadow cascade level based on view space depth
	uint cascadeIndex = SHADOW_MAP_CASCADE_COUNT - 1;
	for(int i = SHADOW_MAP_CASCADE_COUNT - 1; i >= 0; --i) {
		if(viewSpaceDepth <= ubo.cascadeSplits[i]) {
			cascadeIndex = i;
		}
	}

	vec3 offset = getShadowSamplePosOffset(nDotL, normal) / abs(ubo.cascadeScales[cascadeIndex].z);

	vec3 samplePos = worldPos + offset;
	vec3 shadowPosition = (ubo.globalShadowMatrix * vec4(samplePos, 1.0)).xyz;
	vec3 shadowPosDx = dFdxFine(shadowPosition);
	vec3 shadowPosDy = dFdyFine(shadowPosition);

	shadowVis = sampleCascadeMap(shadowPosition, shadowPosDx, shadowPosDy, cascadeIndex);

	return shadowVis;
}

void main() {
	Material m = pc.materialBuffer.mats[matIndex];
    vec4 sampledColor = texture(globalTextures2D[m.baseColorIndex], inUV);
    vec4 metalRough = texture(globalTextures2D[m.metalRoughIndex], inUV);

    vec3 N = texture(globalTextures2D[m.normalIndex], inUV).xyz;
    N = normalize(N * 2.0 - 1.0);
	N = normalize(TBNMatrix * N);

	float nDotL = clamp(dot(N, normalize(ubo.lightPos.xyz)), 0.0, 1.0);
	float shadow = shadowTest(fragPos.xyz, -viewPos.z, nDotL, N);

    outAlbedo = vec4(sampledColor.rgb, viewPos.z);

	N = N * 0.5 + 0.5; // packing the normal
    outNormal = vec4(N, shadow);
}