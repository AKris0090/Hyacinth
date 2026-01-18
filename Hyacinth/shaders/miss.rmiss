#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;

const vec3 lightColor = vec3(1.0, 0.875, 0.5);

void main()
{
    hitValue = vec4(vec3(0.0), 1.0);
}