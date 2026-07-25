#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;

layout (set = 0, binding = 2) uniform samplerCube samplerCubeMap;

void main()
{
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    vec4 worldColor = texture(samplerCubeMap, rayDir);
    hitValue = vec4(worldColor.xyz, 1.0);
}