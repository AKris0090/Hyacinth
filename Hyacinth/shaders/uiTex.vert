#version 460
#extension GL_EXT_buffer_reference : require

struct UIObj {
	vec2 origin;
	vec2 dimensions;
	vec4 flashAmntNDCXYApply;
	uint texIndex;
};

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout  (location = 0) flat out uint texIndex;
layout	(location = 1) out vec4 outUVCapXUVFlashP;

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

	vec2 outuv = vec2(inPosition.w, inNormal.w);
	if (ui.flashAmntNDCXYApply.w > 0.0) {
		outuv = (outuv * 0.5) + 0.25;

		vec2 ndcToUVPos = ui.flashAmntNDCXYApply.yz * 0.25;
		outuv -= ndcToUVPos;
	}

	outUVCapXUVFlashP.x		= outuv.x;
	outUVCapXUVFlashP.y		= outuv.y;

	texIndex = ui.texIndex;
	outUVCapXUVFlashP.z = 1.0;

	outUVCapXUVFlashP.w = ui.flashAmntNDCXYApply.x;

	gl_Position = vec4(pos, 1.0f);
}