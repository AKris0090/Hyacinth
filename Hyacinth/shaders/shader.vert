#version 460
#extension GL_EXT_buffer_reference : require

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inColor;

layout	(location = 0) out vec4 fragColor;
layout	(location = 1) out vec4 fragNormal;
layout	(location = 2) out vec2 outUV;

layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 model[];
};

layout( push_constant ) uniform constants
{	
	mat4 viewProj;
	TransformBuffer transformBuffer;
} PushConstants;

void main() 
{
	gl_Position = PushConstants.viewProj * PushConstants.transformBuffer.model[gl_InstanceIndex] * vec4(inPosition.xyz, 1.0f);
	fragColor	= inColor;
	fragNormal	= inNormal;
	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}