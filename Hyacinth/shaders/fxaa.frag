#version 460

layout 	(set = 0, binding = 0) uniform sampler2D compositeMap;

layout 	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

const float FXAA_MAX_THRESH = 0.125; // 1/8
const float FAXX_MIN_THRESH = 0.03125; // 1/32

float rgb2lum(vec3 rgb) {
	return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}

void main() {
	vec3 rgbN = textureOffset(compositeMap, inUV, ivec2(0, -1)).xyz;
	vec3 rgbW = textureOffset(compositeMap, inUV, ivec2(-1, 0)).xyz;
	vec3 rgbM = textureOffset(compositeMap, inUV, ivec2(0, 0)).xyz;
	vec3 rgbE = textureOffset(compositeMap, inUV, ivec2(1, 0)).xyz;
	vec3 rgbS = textureOffset(compositeMap, inUV, ivec2(0, 1)).xyz;

	float lumaN = rgb2lum(rgbN);
	float lumaW = rgb2lum(rgbW);
	float lumaM = rgb2lum(rgbM);
	float lumaE = rgb2lum(rgbE);
	float lumaS = rgb2lum(rgbS);

	float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
	float range = rangeMax - rangeMin;
	if (range < max(FXAA_MIN_THRESH, rangeMax * FXAA_MAX_THRESH)) {
		outColor = rgbM;
		return;
	}
}  