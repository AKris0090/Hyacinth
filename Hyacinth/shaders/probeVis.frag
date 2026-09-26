#version 460
#extension GL_EXT_buffer_reference : require

#include "probeCommon.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray irradianceArray;
layout(set = 1, binding = 1) uniform sampler2DArray visibilityArray;

layout (location = 0) flat in uint probeIndex;
layout (location = 1) in vec4 probeDir;

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
	ProbePositionBuffer probePosBuffer;
	uint volumeDimX;
	uint volumeDimY;
	uint volumeDimZ;
} pc;

void main() {
    ivec3 textureSize = textureSize(irradianceArray, 0);
    vec3 dir = normalize(probeDir.xyz);
    
    vec2 probeUV = oct_encode(dir) * 0.5 + 0.5;
    
    ivec3 base = getAtlasPosition(int(probeIndex), IRRADIANCE_INNER + 2, int(pc.volumeDimX), int(pc.volumeDimZ));
    
    vec2 atlasUV;
    atlasUV.x = (float(base.x) + probeUV.x * float(IRRADIANCE_INNER)) / float(textureSize.x);
    atlasUV.y = (float(base.y) + probeUV.y * float(IRRADIANCE_INNER)) / float(textureSize.y);
    
    outColor = texture(irradianceArray, vec3(atlasUV, base.z), 0);
}