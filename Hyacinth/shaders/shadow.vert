#version 460
#extension GL_EXT_buffer_reference : require

#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4[SHADOW_MAP_CASCADE_COUNT] cascadeViewProj;
} ubo;

layout(push_constant) uniform pushConstant {
	RenderCallBuffer renderCallBuffer;
	int cascadeIndex;
} pc;

layout(location = 0) in vec4 inPosition;
 
void main()
{
	RenderCall r = pc.renderCallBuffer.calls[gl_InstanceIndex];
	gl_Position = ubo.cascadeViewProj[pc.cascadeIndex] * r.instanceMatrix * vec4(inPosition.xyz, 1.0);
}