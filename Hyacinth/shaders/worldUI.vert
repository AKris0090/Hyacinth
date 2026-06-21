#version 460
#extension GL_EXT_buffer_reference : require

#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout  (location = 0) flat out uint texIndex;
layout	(location = 1) out vec4 outUVCapXUVFlashP;

layout(set = 1, binding = 0) uniform UniformBufferObject {
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
	mat4 worldUIMat;
	uint texIndex;
	float healthPer;
} pc;

void main() 
{
	mat4 model = pc.worldUIMat;

	outUVCapXUVFlashP.x		= inPosition.w;
	outUVCapXUVFlashP.y		= inNormal.w;

	texIndex = pc.texIndex;
	outUVCapXUVFlashP.z = pc.healthPer;

	outUVCapXUVFlashP.w = 1.f;

	gl_Position = ubo.proj * ubo.view * model * vec4(inPosition.xyz, 1.0f);
}