#version 460

layout 	(location = 0) in vec4 worldPos;
layout	(location = 1) in vec4 normal;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec4 outNormal;

void main() {
	outWorldPos = worldPos;
	outNormal = normal;
}