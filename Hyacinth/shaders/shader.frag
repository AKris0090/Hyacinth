#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout	(location = 0) flat in int colorSamplerIndex;
layout	(location = 1) flat in int normalSamplerIndex;
layout  (location = 2) in vec4 inNormal;
layout	(location = 3) in vec4 fragPos;
layout	(location = 4) in mat3 TBNMatrix;
layout	(location = 7) in vec2 inUV;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 viewPos;
	vec4 lightPos;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D globalTextures2D[];

layout(location = 0) out vec4 outColor;

const float ambientStrength = 0.5;
const float specularStrength = 0.75;

void main() {
    vec4 sampledColor = texture(globalTextures2D[colorSamplerIndex], inUV);

    vec4 trueNormal = texture(globalTextures2D[normalSamplerIndex], inUV);
    trueNormal = vec4(normalize(trueNormal.xyz * 2.0 - 1.0), 1.0);
    trueNormal = vec4(normalize(TBNMatrix * trueNormal.xyz), 1.0);

    vec3 lightDir   = normalize(ubo.lightPos.xyz - fragPos.xyz);
    vec3 viewDir    = normalize(ubo.viewPos.xyz - fragPos.xyz);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // diffuse
    float diffuse = max(dot(trueNormal.xyz, lightDir), 0.0); // * lightColor

    // specular
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 64);
    float specular = specularStrength * spec; // * lightColor

    vec3 result = (vec3(ambientStrength) + vec3(diffuse) + vec3(specular)) * sampledColor.xyz;
    outColor = vec4(result, 1.0f);
}