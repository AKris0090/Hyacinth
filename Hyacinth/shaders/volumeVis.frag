#version 460
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec4 outColor;

layout(buffer_reference, std430) readonly buffer VolumeTransformBuffer { 
	mat4 transforms[];
};

layout( push_constant ) uniform constants
{
	VolumeTransformBuffer volumeTransforms;
    int volumeIndex;
} pc;

void main() {
    if (pc.volumeIndex == 0) {
        outColor = vec4(1.0, 0.0, 0.0, 0.25);
    } else if (pc.volumeIndex == 1) {
        outColor = vec4(0.0, 0.0, 1.0, 0.25);
    }
}