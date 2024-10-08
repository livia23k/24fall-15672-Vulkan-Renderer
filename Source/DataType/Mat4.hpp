#pragma once

// A *small* matrix math library for 4x4 matrices only


#include <array>
#include <cmath>
#include <cstdint>

//NOTE: column-major order (the same as OpenGL / GLSL):
//element at row r and column c is stored at index c*4 + r
using mat4 = std::array< float, 16 >;
static_assert(sizeof(mat4) == 16*4, "mat4 is exactly 16 32-bit floats.");

using vec4 = std::array< float, 4 >;
static_assert(sizeof(vec4) == 4*4, "vec4 is exactly 4 32-bit floats.");

//Compute index of mat4 at row r and column c
inline int index(int r, int c) {
    return c*4 + r;
}

//Compute (mat4) A * (vec4) b
inline vec4 operator*(mat4 const &A, vec4 const &b) {
    vec4 ret;
    for (uint32_t r = 0; r < 4; ++ r) {
        ret[r] = A[index(r, 0)] * b[0];
        for (uint32_t c = 1; c < 4; ++ c) {
            ret[r] += A[index(r, c)] * b[c];
        }
    }
    return ret;
}

//Compute (mat4) ret = (mat4) A * (mat4) B
//ret[r, c] += A[r, k] * B[k, c]
inline mat4 operator*(mat4 const &A, mat4 const &B) {
    mat4 ret = {0.0f};
    float b_kc;
    for (uint32_t k = 0; k < 4; ++ k) { //shared axis
        for (uint32_t c = 0; c < 4; ++ c) { //major axis
            b_kc = B[index(k, c)];
            for (uint32_t r = 0; r < 4; ++ r) { //minor axis
                ret[index(r, c)] += A[index(r, k)] * b_kc;
            }
        }
    }
    return ret;
}

//Perspective Projection Matrix
// - vfov: field of view in radians
// - near maps to 0, far maps to 1
// looks down -z with +y up and +x right
inline mat4 perspective(float vfov, float aspect, float near, float far) {

    //as per https://www.terathon.com/gdc07_lengyel.pdf
	// (with modifications for Vulkan-style coordinate system)
	//  notably: flip y (vulkan device coords are y-down)
	//       and rescale z (vulkan device coords are z-[0,1])

    const float e = 1.0f / std::tan(vfov / 2.0f);
    const float a = aspect;
    const float n = near;
    const float f = far;
    return mat4{
        e/a,    0.0f,   0.0f,                               0.0f, // aspect passed in is width/height, e/a = e/width * height
        0.0f,   -e,     0.0f,                               0.0f,
        0.0f,   0.0f,   -0.5f + 0.5f * (-(f+n)/(f-n)),     -1.0f, // from [-1, 1] => [-1, 0]
        0.0f,   0.0f,   -(f*n)/(f-n),                       0.0f, 
    };
}

//LookAt Matrix
// makes a camera-space-from-world matrix for a camera at eye looking toward target with up-vector pointing along up
// it maps:
// - eye_xyz to the origin
// - the unit length vector from eye_xyz to target_xyz to -z
// - an as-close-as-possible unit-length vector to up to +y
inline mat4 look_at(
    float eye_x, float eye_y, float eye_z,
    float target_x, float target_y, float target_z,
    float up_x, float up_y, float up_z)
{
    //NOTE: this would be a lot cleaner with a vec3 type and some overloads

    //compute vector from eye to target
    float in_x = target_x - eye_x;  //'in' serves as the 'forward'
    float in_y = target_y - eye_y;
    float in_z = target_z - eye_z;

    //normalize 'in' vector:
    float inv_in_len = 1.0f / std::sqrt(in_x*in_x + in_y*in_y + in_z*in_z);
    in_x *= inv_in_len;
    in_y *= inv_in_len;
    in_z *= inv_in_len;

    //make 'up' orthogonal to 'in':
    float in_dot_up = in_x*up_x + in_y*up_y + in_z*up_z;
    up_x -= in_dot_up * in_x;   //subtract the part not perpendicular to 'in'
    up_y -= in_dot_up * in_y;
    up_z -= in_dot_up * in_z;

    //normalize 'up' vector:
    float inv_up_len = 1.0f / std::sqrt(up_x*up_x + up_y*up_y + up_z*up_z);
    up_x *= inv_up_len;
    up_y *= inv_up_len;
    up_z *= inv_up_len;

    //compute 'right' vector as 'in' x 'up':
    float right_x = in_y*up_z - in_z*up_y;
    float right_y = in_z*up_x - in_x*up_z;
    float right_z = in_x*up_y - in_y*up_x;

    //compute dot products of right, in, up with eye:
    float right_dot_eye = right_x*eye_x + right_y*eye_y + right_z*eye_z;
    float up_dot_eye = up_x*eye_x + up_y*eye_y + up_z*eye_z;
    float in_dot_eye = in_x*eye_x + in_y*eye_y + in_z*eye_z;

    //final matrix:
    // computes (right . (v - eye), up . (v - eye), -in . (v - eye), v.w)
    return mat4{
        right_x,        up_x,           -in_x,      0.0f,
        right_y,        up_y,           -in_y,      0.0f,
        right_z,        up_z,           -in_z,      0.0f,
        -right_dot_eye, -up_dot_eye,    in_dot_eye, 1.0f
    };
}
