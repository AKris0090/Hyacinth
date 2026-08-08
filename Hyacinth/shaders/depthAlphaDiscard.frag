#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require

#include "bufferInfo.glsl"

layout(set = 1, binding = 0) uniform sampler2D globalTextures2D[];

layout	(location = 0) flat in uint matIndex;
layout	(location = 1) in vec2 inUV;

layout( push_constant ) uniform constants
{
	RenderCallBuffer renderCallBuffer;
	MaterialBuffer materialBuffer;
} pc;

void main() {
	Material m = pc.materialBuffer.mats[matIndex];
    vec4 sampledColor = texture(globalTextures2D[m.baseColorIndex], inUV);
	if (sampledColor.w < m.alphaCutoff) {
		discard;
	}
}