#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "shadowCommon.glsl"
#include "bufferInfo.glsl"

layout	(location = 0) flat in int colorSamplerIndex;
layout	(location = 1) flat in int normalSamplerIndex;
layout	(location = 2) flat in int metalRoughSamplerIndex;
layout  (location = 3) in vec4 viewPos;
layout  (location = 4) in vec4 inNormal;
layout	(location = 5) in vec4 fragPos;
layout	(location = 6) in mat3 TBNMatrix;
layout	(location = 9) in vec2 inUV;

layout (set = 1, binding = 0) uniform sampler2DArray shadowDepthMap;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout( push_constant ) uniform constants
{
	TransformBuffer transformBuffer;
	MaterialBuffer materialBuffer;
	DrawDataBuffer drawDataBuffer;
    VolumeDataBuffer volumeDataBuffer;
	JointMatricesBuffer jmBuffer;
    int volumeIndex;
	uint isAnimated;
} pc;

layout(set = 2, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

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

void main() {
    vec4 sampledColor = texture(globalTextures2D[colorSamplerIndex], inUV);
    vec4 metalRough = texture(globalTextures2D[metalRoughSamplerIndex], inUV);

    vec3 N = texture(globalTextures2D[normalSamplerIndex], inUV).xyz;
    N = normalize(N * 2.0 - 1.0);
    N = normalize(TBNMatrix * N) * 0.5 + 0.5;

    uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(viewPos.z < ubo.cascadeSplits[i]) {
			cascadeIndex = i + 1;
		}
	}

    vec4 shadowCoord = (biasMat * ubo.cascadeViewProj[cascadeIndex]) * vec4(fragPos.xyz, 1.0);	 
	float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);

    outAlbedo = vec4(sampledColor.rgb, metalRough.b);
    outNormal = vec4(N, shadow);
}