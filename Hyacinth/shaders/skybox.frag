#version 460

layout (set = 1, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUV;
layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = texture(samplerCubeMap, inUV);
}