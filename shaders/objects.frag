//Edit Start =================================================================================================
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(inNormal);
    vec3 l = vec3(0.0, 0.0, 1.0);
    // vec3 albedo = vec3(1.0); // [PASS] for test, should be white color
    vec3 albedo = vec3(fract(inTexCoord), 0.0);
    
    vec3 e = vec3(0.5 * dot(n,l) + 0.5);

    outColor = vec4(e * albedo, 1.0);
}
//Edit End ===================================================================================================