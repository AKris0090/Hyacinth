#version 460

#include "probeCommon.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray irradianceArray;
layout(set = 1, binding = 1) uniform sampler2DArray visibilityArray;

layout (location = 0) in vec4 probeDir;
layout (location = 1) flat in int probeIndex;

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosBuffer;
	ivec3 volumeDims;
} pc;

void main() {
    ivec3 textureSize = textureSize(irradianceArray, 0);
    vec3 dir = normalize(probeDir.xyz);

    vec2 probeUV = oct_encode(dir) * 0.5 + 0.5;

    ivec3 base = getAtlasPosition(probeIndex, IRRADIANCE_INNER + 2, pc.volumeDims.x, pc.volumeDims.z);

    vec2 atlasUV;
    atlasUV.x = (float(base.x) + probeUV.x * float(IRRADIANCE_INNER)) / float(textureSize.x);
    atlasUV.y = (float(base.y) + probeUV.y * float(IRRADIANCE_INNER)) / float(textureSize.y);

    outColor = texture(irradianceArray, vec3(atlasUV, base.z), 0);
}