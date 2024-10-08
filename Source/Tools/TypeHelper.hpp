#pragma once

#include "Source/DataType/Mat4.hpp"

#include <glm/glm.hpp>

namespace TypeHelper
{
    mat4 convert_glm_mat4_to_mat4(const glm::mat4& matrix);

};