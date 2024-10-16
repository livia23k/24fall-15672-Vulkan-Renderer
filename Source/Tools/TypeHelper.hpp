#pragma once

#include "Source/DataType/Mat4.hpp"

#include <glm/glm.hpp>

namespace TypeHelper
{
    mat4 convert_glm_mat4_to_mat4(const glm::mat4& matrix);
    glm::mat4 convert_mat4_to_glm_mat4(const mat4& matrix);

};