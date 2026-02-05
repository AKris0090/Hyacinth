#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;

const vec3 lightColor = vec3(0.53, 0.81, 0.92);

void main()
{
    hitValue = vec4(lightColor, 1.0);
}