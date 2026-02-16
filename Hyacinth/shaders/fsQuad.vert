#version 460

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout	(location = 0) out vec2 outUV;

void main() 
{
	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
	gl_Position = vec4(inPosition.xyz, 1.0f);
}