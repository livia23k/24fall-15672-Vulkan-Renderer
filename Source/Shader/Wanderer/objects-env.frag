#version 450

#define PI radians(180.0)

layout(push_constant) uniform Push {
    int material_type;
    vec3 camera_position;
} pushData;

layout(set = 0, binding = 0, std140) uniform World {
    vec3 SKY_DIRECTION;
    vec3 SKY_ENERGY; //energy supplied by sky to a surface patch with normal = SKY_DIRECTION
    vec3 SUN_DIRECTION;
    vec3 SUN_ENERGY; //energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set = 2, binding = 0) uniform sampler2D TEXTURE;
layout (set = 3, binding = 0) uniform samplerCube ENVIRONMENT;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

vec3 convert_rgbe_to_radiance(vec4 environment)
{
    /* cr. 15-666 Computer Game Programming
        https://github.com/ixchow/15-466-ibl/blob/master/rgbe.hpp */

    if (environment == vec4(0.0)) return vec3(0.0);

    environment = environment * 256.0; // convert range [0, 1] to [0, 256]

    float exponent = environment.a - 128.0;
    float coefficient = pow(2, exponent);
    vec3 ret = (environment.rgb + 0.5) / 256.0;

    return coefficient * ret;
}

void main() {

    vec3 n_vulkan = normalize(inNormal);
    // vec3 n_s72 = vec3(n_vulkan.x, n_vulkan.z, -n_vulkan.y); 

    /* 
        ENVIRONMENT Material
    */

    if (pushData.material_type == 3) 
    {
        vec3 albedo = texture(TEXTURE, inTexCoord).rgb / PI;
        vec4 env = texture(ENVIRONMENT, n_vulkan);
        vec3 radiance = convert_rgbe_to_radiance(env);
        outColor = vec4(radiance, 1.0);
    }

    /* 
        MIRROR Material
    */

    else if (pushData.material_type == 2) // TOCHECK
    {
        vec3 view_dir = normalize(pushData.camera_position - inPosition);
        vec3 reflect_dir = reflect(-view_dir, n_vulkan);
        vec4 env = texture(ENVIRONMENT, reflect_dir);
        vec3 radiance = convert_rgbe_to_radiance(env);
        outColor = vec4(radiance, 1.0);
    }

    /*
        Other Materials
    */

    else // default
    {
        vec3 albedo = texture(TEXTURE, inTexCoord).rgb / PI;

        // hemisphere sky + directional sun:
        vec3 e = SKY_ENERGY * (0.5 * dot(n_vulkan, SKY_DIRECTION) + 0.5) + SUN_ENERGY * max(0.0, dot(n_vulkan, SUN_DIRECTION));
        outColor = vec4(e * albedo, 1.0);
    }
}
