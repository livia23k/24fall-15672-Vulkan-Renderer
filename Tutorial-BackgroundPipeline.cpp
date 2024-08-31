//Edit Start ===========================================================================================================
#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"
#include "VK.hpp"
#include "vulkan/vulkan_core.h"

static uint32_t vert_code[] = 
#include "spv/background.vert.inl"
;

static uint32_t frag_code[] =
#include "spv/background.frag.inl"
;

void Tutorial::BackgroundPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) {
    //ver.1 before implementing shader
    // VkShaderModule vert_module = VK_NULL_HANDLE;
    // VkShaderModule frag_module = VK_NULL_HANDLE;

    //ver.2 after implementing shader
    VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

    //ver.1 before create pipeline layout
    // refsol::BackgroundPipeline_create(rtg, render_pass, subpass, vert_module, frag_module, &layout, &handle);

    //ver.2 after create pipeline layout
    { //create pipeline layout:
        VkPushConstantRange range{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, //tells which shader will use this push constant
            .offset = 0,                                //offset in bytes from start of push constant block
            .size = sizeof(Push)                        //size in bytes of push constant block
        };

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range
        };

        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
    };

    { //create pipeline:

        //set shader stage (vertex, fragment code):
        std::array< VkPipelineShaderStageCreateInfo, 2 > stages {
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main"
            },
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main"
            },
        };

        //set dynamic states (viewport and scissor):
        //the viewport and scissor state for the pipeline will be set dynamically later at runtime
        std::vector< VkDynamicState > dynamic_states{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR  
		};
        VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()
        };
        
        //set vertex input state: 
        //this pipeline will take 0 per-vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
        };

        //set input assembly state:
        //this pipeline will take vertex data as a list of triangles
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        //set viewport state:
        //the pipeline uses only 1 viewport and 1 scissor rectangle
        VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1
        };

        //set rasterization state:
        //let rasterizer cull back faces and fill polygons
        VkPipelineRasterizationStateCreateInfo rasterzation_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,           //enabling depth clamp will disable clipping primitives outside of frustum
            .rasterizerDiscardEnable = VK_FALSE,    //enabling will leave no fragments for framebuffer to render
            .polygonMode = VK_POLYGON_MODE_FILL,    //fill or line or point
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, //specifies the vertex order for faces to be considered front-facing
            .depthBiasEnable = VK_FALSE,            //enabling will add depth bias to fragments
            .lineWidth = 1.0f
        };

        //set multisample state:
        //disable multisampling
        VkPipelineMultisampleStateCreateInfo muiltisample_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,  //1 sample per pixel
            .sampleShadingEnable = VK_FALSE
        };

        //set depth stensil state:
        //disable depth / stencil test
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
        };

        //set color blending:
        //enable color blending
        //refer: https://vkguide.dev/docs/new_chapter_3/blending/
        std::array< VkPipelineColorBlendAttachmentState, 1 > attachment_states{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,                              //enableing logical operations on the color, e.g. combine
            .attachmentCount = uint32_t(attachment_states.size()),
            .pAttachments = attachment_states.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.0f}
        };

        //bundle all states together into a large create_info:
        VkGraphicsPipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterzation_state,
            .pMultisampleState = &muiltisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = layout,
            .renderPass = render_pass,
            .subpass = subpass
        };

        VK( vkCreateGraphicsPipelines(  rtg.device, 
                                        VK_NULL_HANDLE,     //pipeline cache
                                        1, 
                                        &create_info, 
                                        nullptr,            //allocation callback
                                        &handle) );
    };

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
}

void Tutorial::BackgroundPipeline::destroy(RTG &rtg) {
    refsol::BackgroundPipeline_destroy(rtg, &layout, &handle);
}
//Edit End ===========================================================================================================