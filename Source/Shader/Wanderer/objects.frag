#version 450

#define PI radians(180.0)

layout(push_constant) uniform Push {
    bool has_env_cubemap;
    int material_type;
} pushData;

layout(set = 0, binding = 0, std140) uniform World {
    vec3 SKY_DIRECTION;
    vec3 SKY_ENERGY; //energy supplied by sky to a surface patch with normal = SKY_DIRECTION
    vec3 SUN_DIRECTION;
    vec3 SUN_ENERGY; //energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set = 2, binding = 0) uniform sampler2D TEXTURE;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;


void main() {
    vec3 n = normalize(inNormal);
    vec3 albedo = texture(TEXTURE, inTexCoord).rgb / PI;

    // hemisphere sky + directional sun:
    vec3 e = SKY_ENERGY * (0.5 * dot(n, SKY_DIRECTION) + 0.5) + SUN_ENERGY * max(0.0, dot(n, SUN_DIRECTION));
    outColor = vec4(e * albedo, 1.0);
}
