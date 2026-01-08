#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout	(location = 0) flat in int colorSamplerIndex;
layout	(location = 1) flat in int normalSamplerIndex;
layout	(location = 2) flat in int metalRoughSamplerIndex;
layout  (location = 3) in vec4 inNormal;
layout	(location = 4) in vec4 fragPos;
layout	(location = 5) in mat3 TBNMatrix;
layout	(location = 8) in vec2 inUV;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float ambientStrength = 0.7;
const vec3 lightColor = vec3(5.0);

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 sampledColor = texture(globalTextures2D[colorSamplerIndex], inUV);
    vec4 metalRough = texture(globalTextures2D[metalRoughSamplerIndex], inUV);
    vec3 N = texture(globalTextures2D[normalSamplerIndex], inUV).xyz;
    N = normalize(N * 2.0 - 1.0);
    N = normalize(TBNMatrix * N);
    vec3 V    = normalize(ubo.viewPos.xyz - fragPos.xyz);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 1; i++) {
        vec3 L = normalize(ubo.lightPos.xyz - fragPos.xyz);
        vec3 H = normalize(V + L);

        float distance = length(ubo.lightPos.xyz - fragPos.xyz);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = attenuation * lightColor;

        vec3 F0 = vec3(0.04); 
        F0      = mix(F0, sampledColor.rgb, metalRough.b);
        vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, metalRough.g);
        float G   = GeometrySmith(N, V, L, metalRough.g);  
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalRough.b;

        float NdotL = max(dot(N, L), 0.0); 
        Lo += (kD * sampledColor.rgb / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(ambientStrength) * sampledColor.rgb; // * ambient occlusion;
    vec3 color = ambient + Lo;

    outColor = vec4(color, 1.0f);
}