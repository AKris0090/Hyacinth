#version 460

layout 	(set = 0, binding = 0) uniform sampler2D compositeMap;

layout 	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

const int ITERATIONS = 12;
const float QUALITY[12] = float[12](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
const float FXAA_MAX_THRESH = 0.125; // 1/8
const float FXAA_MIN_THRESH = 0.03125; // 1/32

const float SUBPIXEL_QUALITY = 0.75;

layout( push_constant ) uniform constants
{
	vec2 inverseScreenSize;
} pc;

float rgb2lum(vec3 rgb) {
	return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}

// https://github.com/kosua20/Rendu/blob/master/resources/common/shaders/screens/fxaa.frag
void main() {
	vec3 rgbM = textureOffset(compositeMap, inUV, ivec2(0, 0)).xyz;

	// 1. non-linear rgb, converted to luminance
	float lumaN = rgb2lum(textureOffset(compositeMap, inUV, ivec2(0, -1)).xyz);
	float lumaW = rgb2lum(textureOffset(compositeMap, inUV, ivec2(-1, 0)).xyz);
	float lumaM = rgb2lum(rgbM);
	float lumaE = rgb2lum(textureOffset(compositeMap, inUV, ivec2(1, 0)).xyz);
	float lumaS = rgb2lum(textureOffset(compositeMap, inUV, ivec2(0, 1)).xyz);

	// 2. check local contrast to avoid processing non-edges
	float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
	float range = rangeMax - rangeMin;
	if (range < max(FXAA_MIN_THRESH, rangeMax * FXAA_MAX_THRESH)) {
		outColor = vec4(rgbM, 1.0);
		return;
	}

	float lumaNE = rgb2lum(textureOffset(compositeMap, inUV, ivec2(1, -1)).xyz);
	float lumaNW = rgb2lum(textureOffset(compositeMap, inUV, ivec2(-1, -1)).xyz);
	float lumaSE = rgb2lum(textureOffset(compositeMap, inUV, ivec2(1, 1)).xyz);
	float lumaSW = rgb2lum(textureOffset(compositeMap, inUV, ivec2(-1, 1)).xyz);

	float downUp = lumaS + lumaN;
	float leftRight = lumaE + lumaW;
	
	float leftCorners =  lumaSW + lumaNW;
	float downCorners =  lumaSE + lumaSW;
	float rightCorners = lumaSE + lumaNE;
	float upCorners =    lumaNE + lumaNW;

	// gradient estimation (offset towards edge)
	float edgeHorizontal =	abs(-2.0 * lumaW + leftCorners)	+ abs(-2.0 * lumaM + downUp ) * 2.0 + abs(-2.0 * lumaE + rightCorners);
	float edgeVertical =	abs(-2.0 * lumaN + upCorners) + abs(-2.0 * lumaM + leftRight) * 2.0 + abs(-2.0 * lumaS + downCorners);

	// 3. classified as horizontal or vertical
	bool isHorizontal = (edgeHorizontal >= edgeVertical);
	
	// choose stepSize
	float stepLength = isHorizontal ? pc.inverseScreenSize.y : pc.inverseScreenSize.x;
	
	// current pixel not necessarily on edge. determine which direction is the "real" edge border
	// select opposite direction edge neighbors and compute gradient in that direction
	float luma1 = isHorizontal ? lumaS : lumaW;
	float luma2 = isHorizontal ? lumaN : lumaE;
	float gradient1 = luma1 - lumaM;
	float gradient2 = luma2 - lumaM;

	// determine steepest
	bool grad1Steeper = abs(gradient1) >= abs(gradient2);

	float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

	// avg luma in correct grad direction. Move half a pixel in the gradient direction where the border is, and compute average local luma
	float lumaAvg = 0.0;
	if (grad1Steeper) {
		// switch direction
		stepLength = -stepLength;
		lumaAvg = 0.5 * (luma1 + lumaM);
	} else {
		lumaAvg = 0.5 * (luma2 + lumaM);
	}

	vec2 currentUV = inUV;
	if (isHorizontal) {
		currentUV.y += stepLength * 0.5;
	} else {
		currentUV.x += stepLength * 0.5;
	}
	
	// exploration along main axis of the edge. step in both directions, query lumas, and compute variation.
	vec2 offset = isHorizontal ? vec2(pc.inverseScreenSize.x, 0.0) : vec2(0.0, pc.inverseScreenSize.y);
	vec2 uv1 = currentUV - offset;
	vec2 uv2 = currentUV + offset;

	float lumaEnd1 = rgb2lum(texture(compositeMap, uv1).rgb);
	float lumaEnd2 = rgb2lum(texture(compositeMap, uv2).rgb);
	lumaEnd1 -= lumaAvg;
	lumaEnd2 -= lumaAvg;

	// if variation greater than local gradient, reached end of edge in this direction. otherwise, continue exploration in that direction
	bool reached1 = abs(lumaEnd1) >= gradientScaled;
	bool reached2 = abs(lumaEnd2) >= gradientScaled;
	bool reachedBoth = reached1 && reached2;

	if (!reached1) {
		uv1 -= offset;
	}
	if (!reached2) {
		uv2 += offset;
	}

	// keep iterating until both ends are reached or max iterations is up. speed up: increase amount of pixels after fifth iteration
	if (!reachedBoth) {
		for(int i = 2; i < ITERATIONS; i++) {
			if (!reached1) {
				lumaEnd1 = rgb2lum(texture(compositeMap, uv1).rgb);
				lumaEnd1 = lumaEnd1 - lumaAvg;
			}

			if (!reached2) {
				lumaEnd2 = rgb2lum(texture(compositeMap, uv2).rgb);
				lumaEnd2 = lumaEnd2 - lumaAvg;
			}

			reached1 = abs(lumaEnd1) >= gradientScaled;
			reached2 = abs(lumaEnd2) >= gradientScaled;
			reachedBoth = reached1 && reached2;

			if(!reached1){
				uv1 -= offset * QUALITY[i];
			}
			if(!reached2){
				uv2 += offset * QUALITY[i];
			}

			 if (reachedBoth) { 
				break;
			}
		}
	}

	// next, compute distance reached and get closest side and the ratio between this distance and the closest side
	// if the current pixel is closer to a side, bigger the UV offset applied
	float distance1 = isHorizontal ? (inUV.x - uv1.x) : (inUV.y - uv1.y);
	float distance2 = isHorizontal ? (uv2.x - inUV.x) : (uv2.y - inUV.y);

	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min(distance1, distance2);

	// length of edge
	float edgeThickness = (distance1 + distance2);
	
	float pixelOffset = -distanceFinal / edgeThickness + 0.5;

	// check if luma variations at extremities are coherent with current pixel, otherwise stepped to far and dont apply offset
	bool isLumaCenterSmaller = lumaM < lumaAvg;
	bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
	float finalOffset = correctVariation ? pixelOffset : 0.0;

	// subpixel alisasing, for thinner lines. compute average luma over 3x3 neighbors, get sub-pixel offset, and refine offset
	float lumaAverage3x3 = (1.0 / 12.0) * (2.0 * (downUp + leftRight) + leftCorners + rightCorners);

	// ratio of delta between global average and center, over luma range in 3x3
	float subPixelOffset1 = clamp(abs(lumaAverage3x3 - lumaM) / range, 0.0, 1.0);
	float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;

	// using this delta, calculate true subpixel offset
	float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

	finalOffset = max(finalOffset, subPixelOffsetFinal);

	// read
	vec2 finalUV = inUV;
	if (isHorizontal) {
		finalUV.y += finalOffset * stepLength;
	} else {
		finalUV.x += finalOffset * stepLength;
	}

	vec3 finalColor = texture(compositeMap, finalUV).rgb;
	outColor = vec4(finalColor, 1.0);
}  