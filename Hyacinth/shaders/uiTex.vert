#version 460
#extension GL_EXT_buffer_reference : require

struct UIObj {
	vec2 origin;
	vec2 dimensions;
	uint texIndex;
};

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;
layout	(location = 3) in vec4 inUVs;

layout  (location = 0) flat out uint texIndex;
layout	(location = 1) out vec2 outUV;

layout(buffer_reference, std430) readonly buffer UITransformBuffer{ 
	UIObj buff[];
};

layout( push_constant ) uniform constants
{
	UITransformBuffer uiBuffer;
} pc;

void main() 
{
	UIObj ui = pc.uiBuffer.buff[gl_InstanceIndex];

	vec3 pos = inPosition.xyz;
	pos.x = (pos.x * ui.dimensions.x) + ui.origin.x;
	pos.y = (pos.y * ui.dimensions.y) + ui.origin.y;

	outUV = inUVs.xy;

	texIndex = ui.texIndex;

	gl_Position = vec4(pos, 1.0f);
}