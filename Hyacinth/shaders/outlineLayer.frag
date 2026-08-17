#version 460

layout 	(set = 0, binding = 2) uniform sampler2D AMRMap;
layout	(location = 0) in vec2 inUV;

void main() {
	float mask = texture(AMRMap, inUV).w;
}