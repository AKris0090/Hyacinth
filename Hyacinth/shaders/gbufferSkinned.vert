#version 460
#extension GL_EXT_buffer_reference : require

#include "bufferInfo.glsl"
#include "shadowCommon.glsl"

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;
layout  (location = 3) in vec4 jointIndex;
layout  (location = 4) in vec4 jointWeight;

layout	(location = 0) flat out int matIndex;
layout  (location = 1) out vec4 viewPos;
layout  (location = 2) out vec4 outNormal;
layout	(location = 3) out vec4 fragPos;
layout	(location = 4) out mat3 TBNMatrix;
layout	(location = 7) out vec2 outUV;

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
	mat4 entityTransformMatrix;
	TransformBuffer transformBuffer;
	DrawDataBuffer drawDataBuffer;
	JointMatricesBuffer jmBuffer;
	MaterialBuffer materialBuffer;
    VolumeDataBuffer volumeDataBuffer;
} pc;

void main() 
{
	vec4 position = vec4(inPosition.xyz, 1.0);
	vec4 normal = vec4(inNormal.xyz, 1.0);
	vec4 tangent = inTangent;

	mat4 skinMatrix =
		    jointWeight.x * pc.jmBuffer.jointMatrices[int(jointIndex.x)] +
		    jointWeight.y * pc.jmBuffer.jointMatrices[int(jointIndex.y)] +
		    jointWeight.z * pc.jmBuffer.jointMatrices[int(jointIndex.z)] +
		    jointWeight.w * pc.jmBuffer.jointMatrices[int(jointIndex.w)];
	
	position = vec4((skinMatrix * position).xyz, 1.0);
	
	mat3 skinMatrix3 = mat3(skinMatrix);
	normal.xyz = skinMatrix3 * normal.xyz;
	tangent.xyz = skinMatrix3 * tangent.xyz;

	DrawData draw = pc.drawDataBuffer.draws[gl_InstanceIndex];
	mat4 model = pc.transformBuffer.model[draw.transformIndex];
	gl_Position = ubo.proj * ubo.view * pc.entityTransformMatrix * model * position;

	fragPos = model * vec4(position.xyz, 1.0);

	vec4 biTangent = vec4(normalize(cross(normal.xyz, tangent.xyz)), 0.0);
	vec3 T = normalize(vec3(model * vec4(tangent.xyz, 0.0)));
	vec3 B = normalize(vec3(model * biTangent));
	vec3 N = normalize(vec3(model * vec4(normal.xyz, 0.0)));
	TBNMatrix = mat3(T, B, N);

	matIndex = draw.materialIndex;

	viewPos = (ubo.view * vec4(fragPos.xyz, 1.0));
	outNormal = vec4(normal.xyz, 1.0);

	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}