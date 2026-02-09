#extension GL_EXT_buffer_reference : require

const int PROBE_DENSITY_WIDTH = 30;
const int PROBE_DENSITY_DEPTH = 20;
const int PROBE_DENSITY_HEIGHT = 14;
const int PROBES_PER_PLANE = PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH;

const int IRRADIANCE_INNER_RES = 6;
const int IRRADIANCE_TILE_WIDTH = 8;

const int VISIBILITY_INNER_RES = 14;
const int VISIBILITY_TILE_WIDTH = 16;

const int VISIBIILITY_INNER_RES = 14;
const int VISIBIILITY_TILE_WIDTH = 16;

const int PROBE_BORDER = 1;

const int RAYS_PER_PROBE = 20000;

const float PI = 3.141592653;

ivec3 getRayDataTexelCoords(uint rayIndex, uint probeIndex) {
    ivec3 coords;
    coords.x = int(rayIndex);
    coords.z = int(probeIndex) / PROBES_PER_PLANE;
    coords.y = int(probeIndex) % PROBES_PER_PLANE;
    return coords;
}

int probeIndexFromPixel(ivec3 pixelCoord, uint probeWithBorder) {
    int probeX = pixelCoord.x / int(probeWithBorder);
    int probeZ = pixelCoord.y / int(probeWithBorder);
    int layer = pixelCoord.z;

    // Number of probes laid out per row (width) and per column (depth)
    const int probesPerRow = PROBE_DENSITY_WIDTH;
    const int probesPerSlice = PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH;

    return probeX
        + probeZ * probesPerRow
        + layer * probesPerSlice;
}

float saturate(float inp) {
    return clamp(inp, 0.0, 1.0);
}

// https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6/ue4-plugin/4.27/RTXGI/Shaders/Private/SDK/ddgi/ProbeCommon.ush#L225
vec3 DDGISphericalFibonacci(float index, float numSamples)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = (2 * PI)*fract(index * b);
    float cosTheta = 1.f - (2.f * index + 1.f) * (1.f / numSamples);
    float sinTheta = sqrt(saturate(1.f - (cosTheta * cosTheta)));

    return normalize(vec3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta));
}

ivec3 getAtlasPosition(int probeIndex, int tileWidth)
{
    ivec3 p;
    p.z = probeIndex / (PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH);
    int rem = probeIndex % (PROBE_DENSITY_WIDTH * PROBE_DENSITY_DEPTH);

    int px = rem % PROBE_DENSITY_WIDTH;
    int py = rem / PROBE_DENSITY_WIDTH;

    p.x = px * tileWidth + PROBE_BORDER;
    p.y = py * tileWidth + PROBE_BORDER;

    return p;
}

vec2 normalized_oct_coord(ivec2 fragCoord, int probe_side_length) {

    int probe_with_border_side = probe_side_length + 2;
    vec2 octahedral_texel_coordinates = ivec2((fragCoord.x - 1) % probe_with_border_side, (fragCoord.y - 1) % probe_with_border_side);

    octahedral_texel_coordinates += vec2(0.5f);
    octahedral_texel_coordinates *= (2.0f / float(probe_side_length));
    octahedral_texel_coordinates -= vec2(1.0f);

    return octahedral_texel_coordinates;
}


float sign_not_zero(in float k) {
    return (k >= 0.0) ? 1.0 : -1.0;
}

vec2 sign_not_zero2(in vec2 v) {
    return vec2(sign_not_zero(v.x), sign_not_zero(v.y));
}

// Assumes that v is a unit vector. The result is an octahedral vector on the [-1, +1] square.
vec2 oct_encode(in vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * sign_not_zero2(result.xy);
    }
    return result;
}


// Returns a unit vector. Argument o is an octahedral vector packed via oct_encode,
// on the [-1, +1] square
vec3 oct_decode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero2(v.xy);
    }
    return normalize(v);
}


layout(buffer_reference, std430) readonly buffer ProbePositionBuffer {
    vec3 positions[];
};

const vec3 volumePos = vec3(-16.044f, -1.4202f, -9.08f);
const vec3 probeSpacing = vec3(1.061833, 0.984286, 0.943500);
const vec3 invProbeSpacing = vec3(1.0 / 1.061833, 1.0 / 0.984286, 1.0 / 0.943500);
const ivec3 probeCounts = ivec3(PROBE_DENSITY_WIDTH, PROBE_DENSITY_HEIGHT, PROBE_DENSITY_DEPTH);

int ProbeCoordsToIndex(ivec3 p)
{
    return p.x
        + p.z * probeCounts.x
        + p.y * probeCounts.x * probeCounts.z;
}

ivec3 getBaseProbeCoords(vec3 worldPos) {
    return clamp(ivec3((worldPos - volumePos) * invProbeSpacing), ivec3(0), probeCounts - ivec3(1));
}

vec3 getProbeWorldPos(ivec3 baseCoords) {
    return baseCoords * probeSpacing + volumePos;
}

vec3 getSurfaceBias(vec3 normal, vec3 camDir) {
    return (normal * 0.2) + (-camDir * 0.05);
}

struct RayPayload {
    vec3 radiance;
    float distance;
    int depth;
};

const uint MAX_DEPTH = 4;