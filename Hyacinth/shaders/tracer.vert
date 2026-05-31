#version 460
#extension GL_EXT_buffer_reference : require

#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout	(location = 0) out vec2 outUV;

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
	TransformBuffer transformBuffer;
	MaterialBuffer materialBuffer;
	uint matIndex;
	uint tracerInd;
	bool alpha;
} pc;

void main() 
{
	mat4 model = pc.transformBuffer.model[pc.tracerInd];

	gl_Position = ubo.proj * ubo.view * model * vec4(inPosition.xyz, 1.0);

	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}