#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <array>

struct PosNorTexVertex
{
    struct
    {
        float x, y, z;
    } Position;
    struct
    {
        float x, y, z;
    } Normal;
    struct
    {
        float s, t;
    } TexCoord;

    static const VkPipelineVertexInputStateCreateInfo array_input_state; // a pipeline vertex input state
                                                                         //  that works with a buffer holding a
                                                                         //  PosNorTexVertex[] array
};

static_assert(sizeof(PosNorTexVertex) == 3 * 4 + 3 * 4 + 2 * 4, "PosNorTexVertex is packed."); // size check, ensure no padding
