#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;

void main()
{
    // should be light Color? or sky box
    hitValue = vec4(0.0, 0.0, 0.0, 0.0);
}