#version 460

#include "shadowCommon.glsl"

const float PI = 3.141592653;

layout 	(set = 0, binding = 0) uniform sampler2D albedoMap;
layout 	(set = 0, binding = 1) uniform sampler2D normalMap;
layout	(set = 0, binding = 2) uniform sampler2D depthMap;
layout  (set = 0, binding = 3) uniform sampler2D ddgiImage;

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

vec3 worldPosFromDepth(float depth) {
    vec2 ndcXY = inUV * 2.0 - 1.0;
    vec4 ndc   = vec4(ndcXY, depth, 1.0);

    vec4 viewSpace = inverse(ubo.proj) * ndc;
    viewSpace /= viewSpace.w;

    return (inverse(ubo.view) * viewSpace).xyz;
}

void main() {
    float depth = texture(depthMap, inUV).r;
	vec3 fragPos = worldPosFromDepth(depth);
	vec4 Nshadow = texture(normalMap, inUV);
    vec3 N = Nshadow.xyz * 2.0 - 1.0; // only because swapchain image is unorm
	vec4 albedo = texture(albedoMap, inUV);

	vec3 V    = normalize(ubo.viewPos.xyz - fragPos);
	vec3 L    = normalize(ubo.lightPos.xyz - fragPos);
	vec3 radiance = lightColor * vec3(17.0);

	float NdotL = max(dot(N, L), 0.0);
    float customLambert = (NdotL * 0.35) + 0.025;
    vec3 diffuse = (albedo.rgb / PI) * customLambert * radiance;

	vec3 r = reflect(-L, N);
    float specular = max(0.0, dot(r, V));
    specular = pow(specular, 16.0) * albedo.w;
    if (depth == 1.0) {
        specular = 0.0;
    }

    vec3 irrad = texture(ddgiImage, inUV).xyz;
	vec3 ambient = albedo.rgb * irrad * ubo.ABOD.w;

	vec3 color = ambient + (diffuse + vec3(specular)) * Nshadow.w;

    outColor = vec4(color, 1.0);

    if (ubo.ABOD.x == 1.0) {
        outColor = vec4(irrad, 1.0);
    }
}  