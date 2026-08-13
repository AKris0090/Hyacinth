#version 460

#include "shadowCommon.glsl"

const float PI = 3.141592653;

layout 	(set = 0, binding = 0) uniform sampler2D albedoMap;
layout 	(set = 0, binding = 1) uniform sampler2D normalMap;
layout 	(set = 0, binding = 2) uniform sampler2D AMRMap;
layout	(set = 0, binding = 3) uniform sampler2D depthMap;
layout  (set = 0, binding = 4) uniform sampler2D ddgiImage;

layout(set = 1, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
	vec4 ABOD; // ambient toggle, bias, offset scale, ddgi intensity
	mat4 globalShadowMatrix;
	vec4 cascadeSplits;
	mat4 cascadeViewProj[SHADOW_MAP_CASCADE_COUNT];
	vec4 cascadeOffsets[SHADOW_MAP_CASCADE_COUNT];
	vec4 cascadeScales[SHADOW_MAP_CASCADE_COUNT];
} ubo;

layout 	(location = 0) in vec2 inUV;
layout	(location = 0) out vec4 outColor;

const vec3 lightColor = vec3(0.99, 0.98, 0.83);

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x) {
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.f), vec3(1.f));
}

vec3 worldPosFromDepth(float depth) {
    vec2 ndcXY = inUV * 2.0 - 1.0;
    vec4 ndc   = vec4(ndcXY, depth, 1.0);

    vec4 viewSpace = inverse(ubo.proj) * ndc;
    viewSpace /= viewSpace.w;

    return (inverse(ubo.view) * viewSpace).xyz;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic, float roughness, vec3 albedo)
{
	vec3 F0 = mix(vec3(0.04), albedo, metallic); // * material.specular
	vec3 Fmax = max(vec3(1.0 - roughness), F0);
	vec3 F = F0 + (Fmax - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 albedo)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, rroughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic, roughness, albedo);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor * 8.0;
	}

	return color;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void main() {
    float depth = texture(depthMap, inUV).r;
	if (depth == 1.0) {
		discard;
	}

	vec3 fragPos = worldPosFromDepth(depth);
	vec4 amr = texture(AMRMap, inUV);
	vec4 Nshadow = texture(normalMap, inUV);
    vec3 N = Nshadow.xyz * 2.0 - 1.0; // only because swapchain image is unorm
	vec4 albedo = texture(albedoMap, inUV);

	vec3 V    = normalize(ubo.viewPos.xyz - fragPos);
	vec3 L    = normalize(ubo.lightPos.xyz - fragPos);
    vec3 irrad = texture(ddgiImage, inUV).xyz;
	vec3 ambient = (albedo.rgb) * irrad * 1.0; // amr.r;

	vec3 color = ambient + BRDF(L, V, N, amr.y, amr.z, albedo.xyz);

    outColor = vec4(ACESFilm(color), 1.0);

    if (ubo.ABOD.x == 1.0) {
        outColor = vec4(irrad * amr.r, 1.0);
    }
}  