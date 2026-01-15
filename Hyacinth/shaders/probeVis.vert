#version 460
#extension GL_EXT_buffer_reference : require

#define SHADOW_MAP_CASCADE_COUNT 3

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout (location = 0) out vec4 probeDir;
layout (location = 1) flat out int probeIndex;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout(buffer_reference, std430) readonly buffer ProbePositionBuffer{ 
	vec4 positions[];
};

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosBuffer;
} PushConstants;

void main() 
{
	vec3 probePos = PushConstants.probePosBuffer.positions[gl_InstanceIndex].xyz;

	vec3 scaledPos = inPosition.xyz * 0.05;
	vec3 worldPos  = scaledPos + probePos;

	probeDir = vec4(normalize(worldPos - probePos), 0.0f);
	probeIndex = int(gl_InstanceIndex);

	gl_Position = ubo.proj * ubo.view * vec4(worldPos.xyz, 1.0f);
}