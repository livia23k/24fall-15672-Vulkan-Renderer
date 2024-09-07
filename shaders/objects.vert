//Edit Start =================================================================================================
#version 450

layout(set=0, binding=0, std140) uniform Camera {
	mat4 CLIP_FROM_WORLD;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

void main() {
	gl_Position = CLIP_FROM_WORLD * vec4(inPosition, 1.0);

    outPosition = inPosition;
	outNormal = Normal;
	outTexCoord = TexCoord;
}
//Edit End ===================================================================================================
