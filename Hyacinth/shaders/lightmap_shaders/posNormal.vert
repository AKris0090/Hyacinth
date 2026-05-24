#version 460
#extension GL_EXT_buffer_reference : require

#include "../bufferInfo.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;
layout	(location = 3) in vec4 inUVs;

layout 	(location = 0) out vec4 worldPos;
layout	(location = 1) out vec4 normal;

layout( push_constant ) uniform constants
{
	TransformBuffer transformBuffer;
	DrawDataBuffer drawDataBuffer;
} pc;

void main() 
{
	DrawData draw = pc.drawDataBuffer.draws[gl_InstanceIndex];
	mat4 model = pc.transformBuffer.model[draw.transformIndex];
	worldPos = model * vec4(inPosition.xyz, 1.0);
	normal = vec4(normalize(mat3(transpose(inverse(model))) * inNormal.xyz), 1.0);

	gl_Position = vec4(inUVs.zw * 2.0 - 1.0, 0.0, 1.0);
}