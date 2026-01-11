#version 460
#extension GL_EXT_buffer_reference : require

#define SHADOW_MAP_CASCADE_COUNT 3

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4[SHADOW_MAP_CASCADE_COUNT] cascadeViewProj;
} ubo;

struct DrawData {
	int transformIndex;
	int materialIndex;
};

layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 model[];
};

layout(buffer_reference, std430) readonly buffer DrawDataBuffer{
	DrawData draws[];
};

layout(push_constant) uniform pushConstant {
	int cascadeIndex;
	TransformBuffer transformBuffer;
	DrawDataBuffer drawDataBuffer;
} PushConstants;

layout(location = 0) in vec4 inPosition;
 
void main()
{
	DrawData draw = PushConstants.drawDataBuffer.draws[gl_InstanceIndex];
	mat4 model = PushConstants.transformBuffer.model[draw.transformIndex];
	gl_Position = ubo.cascadeViewProj[PushConstants.cascadeIndex] * model * vec4(inPosition.xyz, 1.0);
}