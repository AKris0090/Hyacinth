#version 460
#extension GL_EXT_buffer_reference : require

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

layout	(set = 1, binding = 2) uniform sampler2D depthMap;

layout(buffer_reference, std430) readonly buffer VolumeTransformBuffer { 
	mat4 transforms[];
};

layout( push_constant ) uniform constants
{
	VolumeTransformBuffer volumeTransforms;
	int volumeIndex;
} pc;

vec3 worldPosFromDepth(vec2 uv, float depth) {
    vec2 ndcXY = uv * 2.0 - 1.0;
    vec4 ndc   = vec4(ndcXY, depth, 1.0);

    vec4 viewSpace = inverse(ubo.proj) * ndc;
    viewSpace /= viewSpace.w;

    return (inverse(ubo.view) * viewSpace).xyz;
}

void main() {
	vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(depthMap, 0));
	float ssDepth = texture(depthMap, screenUV).r;
	vec3 worldPos = worldPosFromDepth(screenUV, ssDepth);
	mat4 invTransform = inverse(pc.volumeTransforms.transforms[pc.volumeIndex]);
	vec3 volumePos = (invTransform * vec4(worldPos, 1.0)).xyz;
	if (volumePos.x > 1.0 || volumePos.y > 1.0 || volumePos.z > 1.0) {
		discard;
	}
	if (volumePos.x < 0.0 || volumePos.y < 0.0 || volumePos.z < 0.0) {
		discard;
	}
}