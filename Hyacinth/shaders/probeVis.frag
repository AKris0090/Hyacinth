#version 460
#include "probeCommon.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray texArray;

layout (location = 0) in vec4 probeDir;
layout (location = 1) flat in int probeIndex;

layout(location = 0) out vec4 outColor;

void main() {
    ivec3 textureSize = textureSize(texArray, 0);
    vec3 dir = normalize(probeDir.xyz);

    vec2 uv = oct_encode(dir) * 0.5 + 0.5;
    vec2 texel = clamp(uv * float(PROBE_INNER_RES),
                              vec2(0.0),
                              vec2(PROBE_INNER_RES - 1));

    vec3 atlasPos = vec3(getAtlasPosition(probeIndex));
    atlasPos.xy += texel;
    atlasPos = vec3(atlasPos.xy / vec2(textureSize.xy), atlasPos.z);

    outColor = texture(texArray, atlasPos, 0);
}