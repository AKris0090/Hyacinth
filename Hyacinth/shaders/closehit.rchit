#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;
hitAttributeEXT vec2 attribs;

layout(buffer_reference, std430) readonly buffer ProbePositionBuffer{
	vec3 positions[];
};

struct Vertex {
	vec4 position;
	vec4 normal;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer {
	uvec3 triangles[];
};

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosAddress;
	VertexBuffer vertexBufferAddress;
	IndexBuffer indexBufferAddress;
} pc;

const vec3 lightPos = vec3(-2.0, 12.0, -6.0);
const vec3 lightColor = vec3(1.0, 0.875, 0.5);

void main()
{
	uvec3 tri = pc.indexBufferAddress.triangles[gl_PrimitiveID];

	Vertex v0 = pc.vertexBufferAddress.vertices[tri.x];
	Vertex v1 = pc.vertexBufferAddress.vertices[tri.y];
	Vertex v2 = pc.vertexBufferAddress.vertices[tri.z];

	vec3 p0 = v0.position.xyz;
	vec3 p1 = v1.position.xyz;
	vec3 p2 = v2.position.xyz;
	vec3 n0 = v0.normal.xyz;
	vec3 n1 = v1.normal.xyz;
	vec3 n2 = v2.normal.xyz;

	const vec3 barys = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	vec3 normal = normalize(v0.normal.xyz * barys.x + v1.normal.xyz * barys.y + v2.normal.xyz * barys.z);
	vec3 normalWS = normalize(mat3(gl_ObjectToWorldEXT) * normal);
	vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

	vec3 lightVector = normalize(lightPos - worldPos);
	float ndotL = max(dot(lightVector, normalWS), 0.0);

	// irrad.x, irrad.y, irrad.z, localPos.z
	hitValue = vec4(1.0, 0.0, 1.0, 1.0); // vec4((lightColor * ndotL), worldPos.z);
}