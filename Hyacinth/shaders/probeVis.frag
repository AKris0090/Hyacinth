#version 460
#include "probeCommon.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray texArray;

layout (location = 0) in vec4 probeDir;
layout (location = 1) flat in int probeIndex;

layout(location = 0) out vec4 outColor;

void main() {
    ivec3 textureSize = textureSize(texArray, 0);
    vec3 dir = normalize(probeDir.xyz);

    vec2 probeUV = oct_encode(dir) * 0.5 + 0.5;
    ivec3 base = getAtlasPosition(probeIndex); // texel-space origin of inner tile

    vec2 atlasUV;
    atlasUV.x = (float(base.x) + probeUV.x * float(PROBE_INNER_RES)) / float(textureSize.x);
    atlasUV.y = (float(base.y) + probeUV.y * float(PROBE_INNER_RES)) / float(textureSize.y);

    outColor = texture(texArray, vec3(atlasUV, base.z), 0);
}