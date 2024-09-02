#ifdef _WIN32
// ensure we have M_PI
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
[[maybe_unused]] u_int16_t g_frame = 0;
//Edit End ===============================================================================================================

Tutorial::Tutorial(RTG &rtg_) : rtg(rtg_)
{
	refsol::Tutorial_constructor(rtg, &depth_format, &render_pass, &command_pool);

	//Edit Start =========================================================================================================
	background_pipeline.create(rtg, render_pass, 0);
	lines_pipeline.create(rtg, render_pass, 0);
	//Edit End ===========================================================================================================

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces)
	{
		refsol::Tutorial_constructor_workspace(rtg, command_pool, &workspace.command_buffer);
	}
}

Tutorial::~Tutorial()
{
	// just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS)
	{
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (swapchain_depth_image.handle != VK_NULL_HANDLE)
	{
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces)
	{
		refsol::Tutorial_destructor_workspace(rtg, command_pool, &workspace.command_buffer);

		//Edit Start =========================================================================================================
		if (workspace.lines_vertices_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
		}
		if (workspace.lines_vertices.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
		}
		//Edit End ===========================================================================================================
	}
	workspaces.clear();

	//Edit Start =========================================================================================================
	lines_pipeline.destroy(rtg);
	background_pipeline.destroy(rtg);
	//Edit End ===========================================================================================================

	refsol::Tutorial_destructor(rtg, &render_pass, &command_pool);
}

void Tutorial::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain)
{
	//[re]create framebuffers:
	refsol::Tutorial_on_swapchain(rtg, swapchain, depth_format, render_pass, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}

void Tutorial::destroy_framebuffers()
{
	refsol::Tutorial_destroy_framebuffers(rtg, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}

void Tutorial::render(RTG &rtg_, RTG::RenderParams const &render_params)
{
	// assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	// get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	[[maybe_unused]] VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//Edit start =========================================================================================================

	// //record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	// refsol::Tutorial_render_record_blank_frame(rtg, render_pass, framebuffer, &workspace.command_buffer);

	// reset the command buffer
	VK(vkResetCommandBuffer(workspace.command_buffer, 0));

	{ // begin recording: command_buffer
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			//.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // record again every submit
		};
		VK(vkBeginCommandBuffer(workspace.command_buffer, &begin_info));
	}

	// update line vertices:
	if (!lines_vertices.empty()) {

		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = lines_vertices.size() * sizeof(lines_vertices[0]);

		if (workspace.lines_vertices_src.handle == VK_NULL_HANDLE 
			|| workspace.lines_vertices_src.size < needed_bytes) 
		{
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096; //round up to nearest 4k to avoid re-allocat1ing
																	  // continuously if vertex count grows slowly

			if (workspace.lines_vertices_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
			}
			if (workspace.lines_vertices.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
			}

			workspace.lines_vertices_src = rtg.helpers.create_buffer( //"staging" buffer, storing a frame's lines data using the CPU
				new_bytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //have GPU copy data from lines_vertices_src
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host visible memory, coherent (no special sync needed)
				Helpers::Mapped //put it somewhere in the CPU address space
			);
			workspace.lines_vertices = rtg.helpers.create_buffer( //vertex buffer on GPU side
				new_bytes,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //use as a vertex buffer, and a target of a memory copy
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //on GPU, not host visible
				Helpers::Unmapped
			);

			std::cout << "Workspace #" << render_params.workspace_index << " ";
			std::cout << "Re-allocating lines buffers to " << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.lines_vertices_src.size == workspace.lines_vertices.size);
		assert(workspace.lines_vertices_src.size >= needed_bytes);

		//host-side copy into liens_vertices_src:
		assert(workspace.lines_vertices_src.allocation.mapped);
		std::memcpy(workspace.lines_vertices_src.allocation.data(), lines_vertices.data(), needed_bytes);

		//device-side copy from lines_vertices_src to lines_vertices:
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.lines_vertices_src.handle, workspace.lines_vertices.handle, 1, &copy_region);
	};

	{ // memory barrier to make sure copies complete before render pass:
		VkMemoryBarrier memory_barrier{ 
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, //build barrier between src stage mask (Writring) -
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT	 // & dst stage mask (Reading)
		};

		vkCmdPipelineBarrier(
			workspace.command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,		//src stage mask
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,	//dst stage mask
			0,					// dependency flags
			1, &memory_barrier, // memory barriers (count, data)
			0, nullptr,			// buffer memory barriers (count, data)
			0, nullptr			// image memory barriers (count, data)
		);

	};

	// TODO: GPU commands here
	{ // render pass：describes layout, "input from", "output to" of attachments
		[[maybe_unused]] std::array<VkClearValue, 2> clear_values{
			VkClearValue{.color{.float32{0.0f, 0.0f, 0.0f, 1.0f}}},
			VkClearValue{.depthStencil{.depth = 1.0f, .stencil = 0}},
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
			.framebuffer = framebuffer, // provide references to specific attachments
			.renderArea{
				.offset = {.x = 0, .y = 0},		// starting point of render area within framebuffer
				.extent = rtg.swapchain_extent, // size of render area
			},

			// ver.1 simple color
			.clearValueCount = uint32_t(clear_values.size()),
			.pClearValues = clear_values.data(),

			// ver.2. flashing black and white
			//  .clearValueCount = uint32_t(g_frame % 100 < 50 ? clear_values1.size() : clear_values2.size()),
			//  .pClearValues = g_frame % 100 < 50 ? clear_values1.data() : clear_values2.data(),
		};

		vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// TODO: run pipelines here

		// {//frame increased for ClearValue ver.2
		// 	++ g_frame;
		// };

		{ // configure scissor rectangle
			VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = rtg.swapchain_extent,
			};
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor); //(xxx, index of first scissor, \
																		//count of scissor affected, address of scissor)
		};
		{ // configure viewport transform
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

		{ // draw with the background pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline.handle);

			{ // push time:
				BackgroundPipeline::Push push{
					.time = float(time)};
				vkCmdPushConstants(workspace.command_buffer, background_pipeline.layout,
								   VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			};

			vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		};

		{ // draw with the lines pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.handle);

			{ // use lines_vertices (offset 0) as vertex buffer binding 0:
				std::array< VkBuffer, 1 > vertex_buffers{ workspace.lines_vertices.handle };
				std::array< VkDeviceSize, 1 > offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
			};

			//draw lines vertices:
			vkCmdDraw(workspace.command_buffer, uint32_t(lines_vertices.size()), 1, 0, 0);
		};

		vkCmdEndRenderPass(workspace.command_buffer);
	};

	// end recording command_buffer
	VK(vkEndCommandBuffer(workspace.command_buffer));

	//Edit End ===========================================================================================================

	// submit `workspace.command buffer` for the GPU to run:
	refsol::Tutorial_render_submit(rtg, render_params, workspace.command_buffer);
}

void Tutorial::update(float dt)
{
	//Edit Start =========================================================================================================
	
	//update time
	time = std::fmod(time + dt, 60.0f); // avoid precision issues by keeping time in a reasonable range

	{ //set input vertices (an 'x'):
		lines_vertices.clear();
		lines_vertices.reserve(4);

		lines_vertices.emplace_back(LinesPipeline::Vertex{
			.Position{ .x = -1.0f, .y = -1.0f, .z = 0.0f },
			.Color { .r = 0xff, .g = 0x00, .b = 0x00, .a = 0xff }
		});

		lines_vertices.emplace_back(LinesPipeline::Vertex{
			.Position{ .x = 1.0f, .y = 1.0f, .z = 0.0f },
			.Color{ .r = 0x00, .g = 0xff, .b = 0x00, .a = 0xff }
		});

		lines_vertices.emplace_back(LinesPipeline::Vertex{
			.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
			.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
		});

		lines_vertices.emplace_back(LinesPipeline::Vertex{
			.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
			.Color{ .r = 0x00, .g = 0x00, .b = 0x00, .a = 0xff }
		});

		assert(lines_vertices.size() == 4);
	};
	//Edit End ===========================================================================================================
}

void Tutorial::on_input(InputEvent const &)
{
}
