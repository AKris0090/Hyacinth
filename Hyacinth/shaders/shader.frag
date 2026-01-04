#version 460

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 fragNormal;
layout(location = 2) in vec2 outUV;

layout(location = 0) out vec4 outColor;

void main() {
    // float nDotL = dot(normalize(fragNormal.xyz), normalize(vec3(0.0, 0.0, 1.0)));
    outColor = vec4((fragColor).xyz, 1.0f);
}