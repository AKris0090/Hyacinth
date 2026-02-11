#version 460
#extension GL_EXT_buffer_reference : require

#include "probeCommon.glsl"
#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout( push_constant ) uniform constants
{
	TransformBuffer transformBuffer;
	MaterialBuffer materialBuffer;
	DrawDataBuffer drawDataBuffer;
	// ProbePositionBuffer probePosBuffer;
} PushConstants;

void main()
{
	DrawData draw = PushConstants.drawDataBuffer.draws[gl_InstanceIndex];
	mat4 model = PushConstants.transformBuffer.model[draw.transformIndex];
	gl_Position = ubo.proj * ubo.view * model * vec4(inPosition.xyz, 1.0f);
}