#include "Source/Tools/TypeHelper.hpp"

mat4 TypeHelper::convert_glm_mat4_to_mat4(const glm::mat4& matrix)
{
    // NOTE: mat4 and glm::mat4 are both col-major

    mat4 result;

    for (int col = 0; col < 4; ++ col) {
        for (int row = 0; row < 4; ++ row) {
            result[col * 4 + row] = matrix[col][row];
        }
    }

    return result;
}