#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <array>
#include <cstddef>

struct MeshAttribute
{
    struct { float x, y, z; } Position;
    struct { float x, y, z; } Normal;
    struct { float x, y, z, w; } Tangent;
    struct { float s, t; } TexCoord;

    static const VkPipelineVertexInputStateCreateInfo array_input_state; // a pipeline vertex input state
                                                                         //  that works with a buffer holding a
                                                                         //  MeshAttributes[] array
};

static_assert(sizeof(MeshAttribute) == 3 * 4 + 3 * 4 + 4 * 4 + 2 * 4, "MeshAttributes is packed."); // size check, ensure no padding
