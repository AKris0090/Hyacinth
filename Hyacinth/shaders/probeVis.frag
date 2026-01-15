#version 460

layout(set = 1, binding = 0) uniform sampler2DArray texArray;

layout (location = 0) in vec4 probeDir;
layout (location = 1) flat in int probeIndex;

layout(location = 0) out vec4 outColor;

vec2 dirToNDC(vec3 d) {
    float sum = dot(vec3(1.0), abs(d));
    vec3 s = d/sum;

    if (s.z < 0.0) {
        s.x = (s.x < 0.0 ? -1.0 : 1.0) * (1.0 - abs(s.y));
        s.y = (s.y < 0.0 ? -1.0 : 1.0) * (1.0 - abs(s.x));
    }

    return s.xy;
}

const int PROBE_RES    = 6;
const int PROBE_BORDER = 1;
const int PROBE_TILE  = PROBE_RES + 2 * PROBE_BORDER;

ivec3 getAtlasPosition(uint probeIndex)
{
    const int PROBES_X = 10; // probes per row

    ivec3 p;
    p.z = int(probeIndex / (PROBES_X * PROBES_X));
    int rem = int(probeIndex % (PROBES_X * PROBES_X));

    int px = rem % PROBES_X;
    int py = rem / PROBES_X;

    p.x = px * PROBE_TILE + PROBE_BORDER;
    p.y = py * PROBE_TILE + PROBE_BORDER;

    return p;
}

void main() {
    vec2 ndc = dirToNDC(probeDir.xyz);
    vec2 uv = ndc * 0.5 + 0.5;
    ivec2 texel = ivec2(uv * 6.0);

    ivec3 atlasPos = getAtlasPosition(probeIndex);
    atlasPos.xy += texel;

    vec4 sampledColor = texelFetch(texArray, atlasPos, 0);
    outColor = sampledColor;
}