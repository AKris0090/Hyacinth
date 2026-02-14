#version 460
#include "probeCommon.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray irradianceArray;
layout(set = 1, binding = 1) uniform sampler2DArray visibilityArray;

layout (location = 0) in vec4 probeDir;
layout (location = 1) flat in int probeIndex;

layout(location = 0) out vec4 outColor;

void main() {
    ivec3 textureSize = textureSize(irradianceArray, 0);
    vec3 dir = normalize(probeDir.xyz);

    vec2 oct = oct_encode(dir);
    vec2 probeUV = oct * 0.5 + 0.5;
    vec2 texel = probeUV * float(IRRADIANCE_INNER);
    ivec3 base = getAtlasPosition(probeIndex, IRRADIANCE_INNER + 2, 30, 20);
    vec2 atlasUV = (vec2(base.xy) + texel) / vec2(textureSize.xy);

    outColor = texture(irradianceArray, vec3(atlasUV, base.z), 0);
}