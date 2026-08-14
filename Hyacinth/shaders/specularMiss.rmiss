#version 460
#extension GL_EXT_ray_tracing : enable

#include "probeCommon.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout (set = 0, binding = 2) uniform samplerCube samplerCubeMap;

void main()
{
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    vec4 worldColor = texture(samplerCubeMap, rayDir);
    payload.radiance = worldColor.xyz;
}