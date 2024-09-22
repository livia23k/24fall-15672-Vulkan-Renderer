#version 450

layout(set = 0, binding = 0, std140) uniform Camera {
	mat4 CLIP_FROM_WORLD;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
	gl_Position = CLIP_FROM_WORLD * vec4(inPosition, 1.0);
	outColor = inColor;
}
