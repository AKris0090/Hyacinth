#version 460

layout 	(set = 0, binding = 2) uniform sampler2D AMRMap;
layout	(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(AMRMap, inUV);
}