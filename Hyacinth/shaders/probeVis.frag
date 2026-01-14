#version 460

layout(set = 1, binding = 0) uniform sampler2DArray texArray;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}