//Edit Start =================================================================================================
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fract(inTexCoord), 0.0, 1.0);
}
//Edit End ===================================================================================================