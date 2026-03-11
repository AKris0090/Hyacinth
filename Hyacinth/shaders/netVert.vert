#version 460
#extension GL_EXT_buffer_reference : require

#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout(buffer_reference, std430) readonly buffer EntityPositionBuffer{ 
	vec3 positions[];
};

layout( push_constant ) uniform constants
{
	EntityPositionBuffer entityPosBuffer;
} pc;

void main() 
{
	vec3 entityPos = pc.entityPosBuffer.positions[gl_InstanceIndex];
	vec3 worldPos  = inPosition.xyz + entityPos;
	gl_Position = ubo.proj * ubo.view * vec4(worldPos.xyz, 1.0f);
}