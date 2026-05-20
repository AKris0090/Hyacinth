#version 460
#extension GL_EXT_buffer_reference : require

#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

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

layout(buffer_reference, std430) readonly buffer VolumeTransformBuffer { 
	mat4 transforms[];
};

layout( push_constant ) uniform constants
{
	VolumeTransformBuffer volumeTransforms;
    int volumeIndex;
} pc;

void main() 
{
	mat4 volumeT = pc.volumeTransforms.transforms[pc.volumeIndex];
	gl_Position = ubo.proj * ubo.view * volumeT * vec4(inPosition.xyz, 1.0f);
}