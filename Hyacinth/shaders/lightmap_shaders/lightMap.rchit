#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

struct RayPayload {
    uint shaded;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	payload.shaded = 0;
}