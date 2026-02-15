#version 460

layout 	(set = 0, binding = 0) uniform sampler2D albedoMap;
layout 	(set = 0, binding = 1) uniform sampler2D normalMap;

layout 	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

void main() {
	outColor = texture(albedoMap, inUV);
}