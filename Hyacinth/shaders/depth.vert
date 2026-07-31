#version 460
#extension GL_EXT_buffer_reference : require

#include "probeCommon.glsl"
#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout	(location = 0) flat out uint matIndex;
layout	(location = 1) out vec2 outUV;

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
	RenderCallBuffer renderCallBuffer;
	MaterialBuffer materialBuffer;
} pc;

void main()
{
	RenderCall draw = pc.renderCallBuffer.calls[gl_InstanceIndex];
	gl_Position = ubo.proj * ubo.view * draw.instanceMatrix * vec4(inPosition.xyz, 1.0);

	matIndex = draw.materialIndex;
	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}