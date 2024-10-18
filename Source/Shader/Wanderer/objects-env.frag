#version 450

#define PI radians(180.0)

layout(push_constant) uniform Push {
    vec4 camera_position;      // x, y, z, padding
    vec4 constant_albedo;      // x, y, z, padding
    int material_type;
    int packed_has_src_flags;  //  0x1 albedo, 0x10 roughness, 0x100 metalness
    float constant_roughness;
    float constant_metalness;
} pushData;

layout(set = 0, binding = 0, std140) uniform World {
    vec3 SKY_DIRECTION;
    vec3 SKY_ENERGY; //energy supplied by sky to a surface patch with normal = SKY_DIRECTION
    vec3 SUN_DIRECTION;
    vec3 SUN_ENERGY; //energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set = 2, binding = 0) uniform sampler2D TEXTURE_ALBEDO;
layout(set = 2, binding = 1) uniform sampler2D TEXTURE_ROUGHNESS;
layout(set = 2, binding = 2) uniform sampler2D TEXTURE_METALNESS;
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
        vec4 env = texture(ENVIRONMENT, n_vulkan);
        vec3 radiance = convert_rgbe_to_radiance(env);
        outColor = vec4(radiance, 1.0);
    }

    /* 
        MIRROR Material
    */

    else if (pushData.material_type == 2) // TOCHECK
    {
        vec3 view_dir = normalize(pushData.camera_position.xyz - inPosition);
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
        vec3 albedo = bool(pushData.packed_has_src_flags & 0x1) ? texture(TEXTURE_ALBEDO, inTexCoord).rgb / PI : pushData.constant_albedo.xyz;
        float roughness = bool(pushData.packed_has_src_flags & 0x10) ? texture(TEXTURE_ALBEDO, inTexCoord).r : pushData.constant_roughness;
        float metalness = bool(pushData.packed_has_src_flags & 0x100) ? texture(TEXTURE_ALBEDO, inTexCoord).r : pushData.constant_metalness;

        // hemisphere sky + directional sun:
        vec3 e = SKY_ENERGY * (0.5 * dot(n_vulkan, SKY_DIRECTION) + 0.5) + SUN_ENERGY * max(0.0, dot(n_vulkan, SUN_DIRECTION));
        outColor = vec4(e * albedo, 1.0);
        outColor = vec4(albedo, 1.0); // TOCHECK: texture not correct
        // outColor = vec4(vec3(roughness), 1.0); // TOCHECK: texture not correct
        // outColor = vec4(vec3(metalness), 1.0); // TOCHECK: texture not correct
    }
}
