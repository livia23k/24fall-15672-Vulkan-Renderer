#include <cstdint>
#include <stdlib.h>

struct Vector2
{
    float x, y;

    inline bool float_equals(float a, float b) const
    {
        return abs(a - b) < __FLT_EPSILON__;
    }

    bool operator==(const Vector2 &other) const
    {
        return float_equals(x, other.x) && float_equals(y, other.y);
    }
};

struct Vector3
{
    float x, y, z;

    inline bool float_equals(float a, float b) const
    {
        return abs(a - b) < __FLT_EPSILON__;
    }

    bool operator==(const Vector3 &other) const
    {
        return float_equals(x, other.x) && float_equals(y, other.y) && float_equals(z, other.z);
    }
};

struct VertexIndices
{
    int v, vt, vn;
};

// struct Face {
//     int v1, v2, v3;     //vertex indices
//     int vt1, vt2, vt3;  //vertex texture indices
//     int vn1, vn2, vn3;  //vertex normal indices
// };
