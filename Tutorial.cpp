#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "Tutorial.hpp"
#include "refsol.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

//Edit Start =============================================================================================================
u_int16_t g_frame = 0;
//Edit End ===============================================================================================================

Tutorial::Tutorial(RTG &rtg_) : rtg(rtg_) {
	refsol::Tutorial_constructor(rtg, &depth_format, &render_pass, &command_pool);

	//Edit Start =========================================================================================================
	background_pipeline.create(rtg, render_pass, 0);
	//Edit End ===========================================================================================================

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		refsol::Tutorial_constructor_workspace(rtg, command_pool, &workspace.command_buffer);
	}
}

Tutorial::~Tutorial() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces) {
		refsol::Tutorial_destructor_workspace(rtg, command_pool, &workspace.command_buffer);
	}
	workspaces.clear();

	//Edit Start =========================================================================================================
	background_pipeline.destroy(rtg);
	//Edit End ===========================================================================================================

	refsol::Tutorial_destructor(rtg, &render_pass, &command_pool);
}

void Tutorial::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//[re]create framebuffers:
	refsol::Tutorial_on_swapchain(rtg, swapchain, depth_format, render_pass, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}

void Tutorial::destroy_framebuffers() {
	refsol::Tutorial_destroy_framebuffers(rtg, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}


void Tutorial::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	[[maybe_unused]] VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//Edit start =========================================================================================================
	
	// //record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`: 
	// refsol::Tutorial_render_record_blank_frame(rtg, render_pass, framebuffer, &workspace.command_buffer); 

	//reset the command buffer
	VK( vkResetCommandBuffer(workspace.command_buffer, 0) );
	{
		//begin recording command_buffer
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			//.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //record again every submit
		};
		VK( vkBeginCommandBuffer(workspace.command_buffer, &begin_info) );
	}

	//TODO: GPU commands here
	{ //render pass：describes layout, "input from", "output to" of attachments
		[[maybe_unused]] std::array< VkClearValue, 2 > clear_values{
			VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
			VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },	
		};

		// [[maybe_unused]] std::array< VkClearValue, 2 > clear_values1{
		// 	VkClearValue{ .color{ .float32{1.0f, 1.0f, 1.0f, 1.0f} } },
		// 	VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },	
		// };

		// [[maybe_unused]] std::array< VkClearValue, 2 > clear_values2{
		// 	VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 0.0f} } },
		// 	VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },	
		// };

		VkRenderPassBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			//.pNext = nullptr,
			.renderPass = render_pass,
			.framebuffer = framebuffer, //provide references to specific attachments
			.renderArea{
				.offset = { .x = 0, .y = 0 }, //starting point of render area within framebuffer
				.extent = rtg.swapchain_extent, //size of render area
			},

			//ver.1 simple color
			.clearValueCount = uint32_t(clear_values.size()),
			.pClearValues = clear_values.data(),

			//ver.2. flashing black and white
			// .clearValueCount = uint32_t(g_frame % 100 < 50 ? clear_values1.size() : clear_values2.size()),
			// .pClearValues = g_frame % 100 < 50 ? clear_values1.data() : clear_values2.data(),
		};

		vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		//TODO: run pipelines here

		// {//frame increased for ClearValue ver.2
		// 	++ g_frame;
		// };

		{ //configure scissor rectangle
			VkRect2D scissor{
				.offset = { .x = 0, .y = 0 },
				.extent = rtg.swapchain_extent,	
			};
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor); 	//(xxx, index of first scissor, \
																		//count of scissor affected, address of scissor)
		};
		{ //configure viewport transform
			VkViewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = float(rtg.swapchain_extent.width),
				.height = float(rtg.swapchain_extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);
		};
		{ //draw with the background pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline.handle);

			{ //push time:
				BackgroundPipeline::Push push{
					.time = float(time)
				};
				vkCmdPushConstants(workspace.command_buffer, background_pipeline.layout, 
									VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			};

			vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		};

		vkCmdEndRenderPass(workspace.command_buffer);
	};

	//end recording command_buffer
	VK( vkEndCommandBuffer(workspace.command_buffer) );

	//Edit End ===========================================================================================================

	//submit `workspace.command buffer` for the GPU to run:
	refsol::Tutorial_render_submit(rtg, render_params, workspace.command_buffer);
}


void Tutorial::update(float dt) {
	//Edit Start =========================================================================================================
	time = std::fmod(time + dt, 60.0f); //avoid precision issues by keeping time in a reasonable range
	//Edit End ===========================================================================================================
}


void Tutorial::on_input(InputEvent const &) {
}
