//Edit Start ===========================================================================================================
#include <cstdint>
#include <stdlib.h>

struct Vector3 {
    float x, y, z;

    inline bool float_equals(float a, float b) const {
        return abs(a - b) < __FLT_EPSILON__;
    }
    
    bool operator==(const Vector3& other) const {
        return float_equals(x, other.x) && float_equals(y, other.y) && float_equals(z, other.z);
    }
};

struct Face {
    int v1, v2, v3;
};

//Edit End =============================================================================================================