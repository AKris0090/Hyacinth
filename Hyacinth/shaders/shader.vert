#version 460
#extension GL_EXT_buffer_reference : require

#define SHADOW_MAP_CASCADE_COUNT 3

layout	(location = 0) in vec4 inPosition;
layout	(location = 1) in vec4 inNormal;
layout	(location = 2) in vec4 inTangent;

layout	(location = 0) flat out int colorSamplerIndex;
layout	(location = 1) flat out int normalSamplerIndex;
layout	(location = 2) flat out int metalRoughSamplerIndex;
layout  (location = 3) out vec4 viewPos;
layout  (location = 4) out vec4 outNormal;
layout	(location = 5) out vec4 fragPos;
layout	(location = 6) out mat3 TBNMatrix;
layout	(location = 9) out vec2 outUV;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 model[];
};

struct Material {
	int baseColorIndex;
	int normalIndex;
	int metalRoughIndex;
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
	mat4 model = PushConstants.transformBuffer.model[draw.transformIndex];
	gl_Position = ubo.proj * ubo.view * model * vec4(inPosition.xyz, 1.0f);

	fragPos = model * vec4(inPosition.xyz, 1.0);

	vec4 biTangent = vec4(normalize(cross(inNormal.xyz, inTangent.xyz)), 0.0);
	vec3 T = normalize(vec3(model * vec4(inTangent.xyz, 0.0)));
	vec3 B = normalize(vec3(model * biTangent));
	vec3 N = normalize(vec3(model * vec4(inNormal.xyz, 0.0)));
	TBNMatrix = mat3(T, B, N);

	Material mat = PushConstants.materialBuffer.mats[draw.materialIndex];

	colorSamplerIndex = mat.baseColorIndex;
	normalSamplerIndex = mat.normalIndex;
	metalRoughSamplerIndex = mat.metalRoughIndex;

	viewPos = (ubo.view * vec4(fragPos.xyz, 1.0));
	outNormal = vec4(mat3(transpose(inverse(model))) * inNormal.xyz, 1.0);

	outUV.x		= inPosition.w;
	outUV.y		= inNormal.w;
}