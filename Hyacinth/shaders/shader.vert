#version 460
#extension GL_EXT_buffer_reference : require

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inColor;

layout	(location = 0) flat out int colorSamplerIndex;
layout	(location = 1) flat out int normalSamplerIndex;
layout	(location = 2) out vec4 fragNormal;
layout	(location = 3) out vec2 outUV;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 model[];
};

struct Material {
	int baseColorIndex;
	int normalIndex;
};

struct DrawData {
	int transformIndex;
	int materialIndex;
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer{ 
	Material mats[];
};

layout(buffer_reference, std430) readonly buffer DrawDataBuffer{
	DrawData draws[];
};

layout( push_constant ) uniform constants
{
	TransformBuffer transformBuffer;
	MaterialBuffer materialBuffer;
	DrawDataBuffer drawDataBuffer;
} PushConstants;

void main() 
{
	DrawData draw = PushConstants.drawDataBuffer.draws[gl_InstanceIndex];
	gl_Position = ubo.proj * ubo.view * PushConstants.transformBuffer.model[draw.transformIndex] * vec4(inPosition.xyz, 1.0f);
	Material mat = PushConstants.materialBuffer.mats[draw.materialIndex];
	colorSamplerIndex = mat.baseColorIndex;
	normalSamplerIndex = mat.normalIndex;
	fragNormal	= inNormal;
	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}