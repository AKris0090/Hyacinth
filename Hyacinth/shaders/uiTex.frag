#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout	(location = 0) flat in uint texIndex;
layout  (location = 1) in vec2 outUV;
layout	(location = 3) in float capXUV;

layout  (set = 0, binding = 0) uniform sampler2D globalTextures2D[];

layout  (location = 0) out vec4 outColor;

void main() {
	if (outUV.x > capXUV) {
		discard;
	}
	outColor = texture(globalTextures2D[texIndex], outUV);
}