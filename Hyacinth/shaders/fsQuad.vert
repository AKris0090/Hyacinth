#version 460

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;
layout	(location = 3) in vec4 inUVs;

layout	(location = 0) out vec2 outUV;

void main() 
{
	outUV = inUVs.xy;
	gl_Position = vec4(inPosition.xyz, 1.0f);
}