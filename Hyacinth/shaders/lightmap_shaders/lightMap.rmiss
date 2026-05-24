#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
    uint shaded;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	payload.shaded = 1;
}