#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) flat in int colorSamplerIndex;
layout(location = 1) flat in int normalSamplerIndex;
layout(location = 2) in vec4 fragNormal;
layout(location = 3) in vec2 outUV;

layout(set = 0, binding = 1) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampledColor = texture(globalTextures2D[colorSamplerIndex], outUV);
    outColor = vec4(sampledColor.xyz, 1.0f);
    //outColor = vec4(vec3(1.0f, 1.0f, 1.0f), 1.0f);
}