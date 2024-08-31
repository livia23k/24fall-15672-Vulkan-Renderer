//Edit Start ========================================================================================================================================
#version 450

layout(push_constant) uniform Push {
    float time;
} pushData;

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec4 outColor;

#define M_PI 3.1415926535897932384626433832795

vec3 hsv2rgb(vec3 c) {

    {//ver1. HSV to RGB standard conversion formula 
    //from https://www.rapidtables.com/convert/color/hsv-to-rgb.html
        float H = c.x;
        float S = c.y;
        float V = c.z; 

        float H_prime = H * 6.0; //equals "float H' = H * 360 / 60;"

        float C = V * S;
        float X = C * (1.0 - abs( mod(H_prime, 2) - 1.0 ));
        float m = V - C;

        vec3 rgb;

        if (H_prime < 1.0) {
            rgb = vec3(C, X, 0.0);
        } else if (H_prime < 2.0) {
            rgb = vec3(X, C, 0.0);
        } else if (H_prime < 3.0) {
            rgb = vec3(0.0, C, X);
        } else if (H_prime < 4.0) {
            rgb = vec3(0.0, X, C);
        } else if (H_prime < 5.0) {
            rgb = vec3(X, 0.0, C);
        } else {
            rgb = vec3(C, 0.0, X);
        }

        return rgb + vec3(m);
    };


    // {//ver2. a faster version but i don't understand 
    // //from https://web.archive.org/web/20190526052603/http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
    //     vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    //     vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    //     return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    // };
}

void main() {

    // {//ver1. fragcoord output (pattern will vary)
    //     outColor = vec4( fract(gl_FragCoord.x / 100), gl_FragCoord.y / 1000, 0.2, 1.0  );
    // }; 

    // {//ver2. normalized position output (pattern will not vary)
    //     outColor = vec4(inPosition, 0.0, 1.0);
    // };

    {//ver3. self-customized pattern
        vec2 center = vec2(0.5, 0.5);
        float pi2 = 2.0 * 3.14159265;

        float angle = atan(inPosition.y - center.y, inPosition.x - center.x) + M_PI;    //convert angle range from [-pi, pi] to [0, 2pi]
        float angle_by_time = mod(angle + pushData.time, pi2);

        float hue = angle_by_time / pi2;                                                //convert angle range to [0,1] and use it as hue
        vec3 color = hsv2rgb(vec3(hue, 1.0, 1.0));
        
        outColor = vec4(color, 1.0);
    };
}
//Edit End ==========================================================================================================================================