#extension GL_EXT_buffer_reference : require

const int IRRADIANCE_INNER = 6;
const int VISIBILITY_INNER = 14;

const int RAYS_PER_PROBE = 20000;
const float PI = 3.141592653;

ivec3 getRayDataTexelCoords(uint rayIndex, uint probeIndex, int probesPerPlane) {
    ivec3 coords;
    coords.x = int(rayIndex);
    coords.z = int(probeIndex) / probesPerPlane;
    coords.y = int(probeIndex) % probesPerPlane;
    return coords;
}

int probeIndexFromPixel(ivec3 pixelCoord, uint probeWithBorder, int probeDensityWidth, int probeDensityDepth) {
    int probeX = pixelCoord.x / int(probeWithBorder);
    int probeZ = pixelCoord.y / int(probeWithBorder);
    int layer = pixelCoord.z;

    // Number of probes laid out per row (width) and per column (depth)
    const int probesPerRow = probeDensityWidth;
    const int probesPerSlice = probeDensityWidth * probeDensityDepth;

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

ivec3 getAtlasPosition(int probeIndex, int tileWidth, int probeDensityWidth, int probeDensityDepth)
{
    ivec3 p;
    p.z = probeIndex / (probeDensityWidth * probeDensityDepth);
    int rem = probeIndex % (probeDensityWidth * probeDensityDepth);

    int px = rem % probeDensityWidth;
    int py = rem / probeDensityWidth;

    p.x = px * tileWidth + 1; // border
    p.y = py * tileWidth + 1; // border

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

int ProbeCoordsToIndex(ivec3 p, ivec3 probeCounts)
{
    return p.x
        + p.z * probeCounts.x
        + p.y * probeCounts.x * probeCounts.z;
}

ivec3 getBaseProbeCoords(vec3 worldPos, vec3 inverseProbeSpacing, vec3 volumePos, ivec3 probeCounts) {
    return clamp(ivec3((worldPos - volumePos) * inverseProbeSpacing), ivec3(0), probeCounts - ivec3(1));
}

vec3 getProbeWorldPos(ivec3 baseCoords, vec3 probeSpacing, vec3 volumePos) {
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