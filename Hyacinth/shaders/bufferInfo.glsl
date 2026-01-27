#extension GL_EXT_nonuniform_qualifier : enable

struct Material {
	int baseColorIndex;
	int normalIndex;
	int metalRoughIndex;
};

struct DrawData {
	int transformIndex;
	int materialIndex;
};

struct Vertex {
	vec4 position;
	vec4 normal;
};

struct AABB {
	vec4 min;
	vec4 max;
};

struct IndexedIndirectCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
};

layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 model[];
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer{ 
	Material mats[];
};

layout(buffer_reference, std430) readonly buffer MaterialIntBuffer {
	int mats[];
};

layout(buffer_reference, std430) readonly buffer DrawDataBuffer{
	DrawData draws[];
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer {
	uint indices[];
};

layout(buffer_reference, std430) readonly buffer BoundingBoxes {
	AABB boxes[];
};

layout(buffer_reference, std430) buffer OutputIndirectDraws {
	IndexedIndirectCommand indirectDrawsOut[];
};