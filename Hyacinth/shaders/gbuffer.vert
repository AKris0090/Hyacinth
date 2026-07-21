#version 460
#extension GL_EXT_buffer_reference : require

#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout	(location = 0) flat out uint matIndex;
layout  (location = 1) out vec4 viewPos;
layout  (location = 2) out vec4 outNormal;
layout	(location = 3) out vec4 fragPos;
layout	(location = 4) out mat3 TBNMatrix;
layout	(location = 7) out vec2 outUV;

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

	fragPos = draw.instanceMatrix * vec4(inPosition.xyz, 1.0);

	vec4 biTangent = vec4(normalize(cross(inNormal.xyz, inTangent.xyz)), 0.0);
	vec3 T = normalize(vec3(draw.instanceMatrix * vec4(inTangent.xyz, 0.0)));
	vec3 B = normalize(vec3(draw.instanceMatrix * biTangent));
	vec3 N = normalize(vec3(draw.instanceMatrix * vec4(inNormal.xyz, 0.0)));
	TBNMatrix = mat3(T, B, N);

	matIndex = draw.materialIndex;

	viewPos = (ubo.view * vec4(fragPos.xyz, 1.0));
	outNormal = vec4(inNormal.xyz, 1.0);

	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}