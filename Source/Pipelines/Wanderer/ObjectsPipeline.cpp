#include "Source/Application/Wanderer/Wanderer.hpp"

#include "Source/VkMemory/Helpers.hpp"
#include "Source/Helper/VK.hpp"
#include "vulkan/vulkan_core.h"

static uint32_t vert_code[] =
#include "spv/Source/Shader/Wanderer/objects.vert.inl"
    ;

static uint32_t frag_code[] =
#include "spv/Source/Shader/Wanderer/objects.frag.inl"
    ;

static uint32_t vert_env_code[] =
#include "spv/Source/Shader/Wanderer/objects-env.vert.inl"
    ;

static uint32_t frag_env_code[] =
#include "spv/Source/Shader/Wanderer/objects-env.frag.inl"
    ;

void Wanderer::ObjectsPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass)
{
    VkShaderModule vert_module;
    VkShaderModule frag_module;

    if (this->has_env_cubemap)
    {
        vert_module = rtg.helpers.create_shader_module(vert_env_code);
        frag_module = rtg.helpers.create_shader_module(frag_env_code);
    }
    else
    {
        vert_module = rtg.helpers.create_shader_module(vert_code);
        frag_module = rtg.helpers.create_shader_module(frag_code);
    }

    // create transforms descriptor set layout binding:

    { // set0_World layout holds world info in a uniform buffer used in the fragment shader
        std::array<VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data()};

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_World));
    };

    { // set1_Transforms layout holds a Transform structure in a STORAGE buffer used in the vertex shader
        std::array<VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT}};

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data()};

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_Transforms));
    };

    { // set2_TEXTURE layout holds three bindings for sampler2Ds used in the fragment shader (albedo, roughness, metalness)
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1, // 1 descriptor per binding
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
            VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data()};

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_TEXTURE));
    };

    // set3_ENVIRONMENT layout holds a single descriptor for a sampler2D used in the fragment shader
    if (this->has_env_cubemap) 
    { 
        std::array<VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data()};

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set3_ENVIRONMENT));
    };

    { // create pipeline layout:

        std::array<VkDescriptorSetLayout, 4> layouts;
        uint32_t layout_count = 0;

        if (this->has_env_cubemap) {
            layouts = {
                set0_World,
                set1_Transforms,
                set2_TEXTURE,
                set3_ENVIRONMENT
            };
            layout_count = 4;
        }
        else
        {
            layouts = {
                set0_World,
                set1_Transforms,
                set2_TEXTURE
            };
            layout_count = 3;
        }
        
        VkPushConstantRange range{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(Wanderer::ObjectsPipeline::Push)
        };

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = layout_count,
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range
        };

        VK(vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout));
    };

    { // create pipeline:
        // set shader stage (vertex, fragment code):
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main"},
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main"},
        };

        // set dynamic states (viewport and scissor):
        // the viewport and scissor state for the pipeline will be set dynamically later at runtime
        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()};

        // set input assembly state:
        // this pipeline will take vertex data as a list of triangles
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE};

        // set viewport state:
        // the pipeline uses only 1 viewport and 1 scissor rectangle
        VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1};

        // set rasterization state:
        // let rasterizer cull back faces and fill polygons
        VkPipelineRasterizationStateCreateInfo rasterzation_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,                 // enabling depth clamp will disable clipping primitives outside of frustum
            .rasterizerDiscardEnable = VK_FALSE,          // enabling will leave no fragments for framebuffer to render
            .polygonMode = VK_POLYGON_MODE_FILL,          // fill or line or point
            // .cullMode = VK_CULL_MODE_BACK_BIT,            // cull back
            .cullMode = VK_CULL_MODE_NONE,                // cull none
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // specifies the vertex order for faces to be considered front-facing
            .depthBiasEnable = VK_FALSE,                  // enabling will add depth bias to fragments
            .lineWidth = 1.0f};

        // set multisample state:
        // disable multisampling
        VkPipelineMultisampleStateCreateInfo muiltisample_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // 1 sample per pixel
            .sampleShadingEnable = VK_FALSE};

        // set depth stensil state:
        // disable depth / stencil test
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE};

        // set color blending:
        // enable color blending
        // refer: https://vkguide.dev/docs/new_chapter_3/blending/
        std::array<VkPipelineColorBlendAttachmentState, 1> attachment_states{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE, // enableing logical operations on the color, e.g. combine
            .attachmentCount = uint32_t(attachment_states.size()),
            .pAttachments = attachment_states.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.0f}};

        // bundle all states together into a large create_info:
        VkGraphicsPipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &Vertex::array_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterzation_state,
            .pMultisampleState = &muiltisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = layout,
            .renderPass = render_pass,
            .subpass = subpass};

        VK(vkCreateGraphicsPipelines(rtg.device,
                                     VK_NULL_HANDLE, // pipeline cache
                                     1,
                                     &create_info,
                                     nullptr, // allocation callback
                                     &handle));
    };

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
}

void Wanderer::ObjectsPipeline::destroy(RTG &rtg)
{
    if (set0_World != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, set0_World, nullptr);
        set0_World = VK_NULL_HANDLE;
    }

    if (set1_Transforms != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if (set2_TEXTURE != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, set2_TEXTURE, nullptr);
        set2_TEXTURE = VK_NULL_HANDLE;
    }

    if (set3_ENVIRONMENT != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, set3_ENVIRONMENT, nullptr);
        set3_ENVIRONMENT = VK_NULL_HANDLE;
    } 

    if (layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(rtg.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
}
