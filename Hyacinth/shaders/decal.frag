#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require

#include "shadowCommon.glsl"
#include "bufferInfo.glsl"

layout	(location = 0) flat in uint matIndex;
layout	(location = 1) in vec2 inUV;

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
	mat4 viewModelProj;
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

layout(set = 1, binding = 0) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outColor;

void main() {
	Material m = pc.materialBuffer.mats[matIndex];
    	vec4 sampledColor = texture(globalTextures2D[m.baseColorIndex], inUV);

    	outColor = vec4(sampledColor.rgb * m.baseColor.rgb, sampledColor.a);    
}