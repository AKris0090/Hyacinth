#version 450

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inColor;

layout	(location = 0) out vec4 fragColor;
layout	(location = 1) out vec4 fragNormal;
layout	(location = 2) out vec2 outUV;

layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
} PushConstants;

void main() 
{
	gl_Position = PushConstants.render_matrix * vec4(inPosition.xyz, 1.0f);
	fragColor	= inColor;
	fragNormal	= inNormal;
	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}