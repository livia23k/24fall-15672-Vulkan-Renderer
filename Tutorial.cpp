#ifdef _WIN32
// ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "Tutorial.hpp"
#include "refsol.hpp"
#include "scripts/FileMgr.hpp"

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
	objects_pipeline.create(rtg, render_pass, 0);

	{ //create descriptor pool:
		uint32_t per_workspace = uint32_t(rtg.workspaces.size()); //for easier-to-read counting

		std::array< VkDescriptorPoolSize, 1 > pool_sizes{
			//only need uniform buffer descriptors for now
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1 * per_workspace //1 descriptor per set, 1 set per workspace
			}
		};

		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, can't free individual descriptors allocated from this pool
			.maxSets = 1 * per_workspace, //one set per workspace
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data()
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );
	};
	//Edit End ===========================================================================================================

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces)
	{
		refsol::Tutorial_constructor_workspace(rtg, command_pool, &workspace.command_buffer);

		//Edit Start =========================================================================================================
		workspace.Camera_src = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host visible memory, coherent (no special sync needed)
			Helpers::Mapped //put it somewhere in the CPU address space
		);
		workspace.Camera = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //use as a uniform buffer, and a target of a memory copy
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //on GPU, not host visible
			Helpers::Unmapped
		);

		//DONE: descriptor set
		{ //allocate descriptor set for Camera descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &lines_pipeline.set0_Camera
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Camera_descriptors) );
		};

		//DONE: descriptor write
		{ //point descriptor to Camera buffer:
			VkDescriptorBufferInfo Camera_info{
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Camera_descriptors,
					.dstBinding = 0, 
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &Camera_info
				}
			};

			vkUpdateDescriptorSets(
				rtg.device,
				uint32_t(writes.size()),	//descriptor write count
				writes.data(),			    //descriptor writes 
				0,							//descriptor copy count
				nullptr						//descriptor copies
			);

		};
		//Edit End ===========================================================================================================
	}

	//Edit Start =============================================================================================================

	{ //create line vertices from .obj file:
		lines_vertices.clear();
		std::vector<LinesPipeline::Vertex> mesh_vertices;
		// FileMgr::load_line_from_object("models/obj/heart.obj", mesh_vertices); // heart model from https://www.cgtrader.com/free-3d-models/exterior/other/low-poly-heart-ef3d722e-8004-4f42-a53b-44c67874166d
		// FileMgr::load_line_from_object("models/obj/acoustic_guitar.obj", mesh_vertices); // guitar model from https://www.thebasemesh.com/asset/acoustic-guitar
		FileMgr::load_line_from_object("models/obj/boat.obj", mesh_vertices); // boat model from https://www.thebasemesh.com/asset/boat-ornament

		for (auto &v : mesh_vertices) {
			lines_vertices.push_back(v);
		}
	};

	{ //create object vertices from .obj file:
		std::vector< ObjectsPipeline::Vertex > vertices;

		{ //object 0: model read from .obj file
			heart_vertices.first = uint32_t(vertices.size());

			std::vector<ObjectsPipeline::Vertex> mesh_vertices;
			// FileMgr::load_mesh_from_object("models/obj/heart.obj", mesh_vertices);
			// FileMgr::load_mesh_from_object("models/obj/acoustic_guitar.obj", mesh_vertices);
			FileMgr::load_mesh_from_object("models/obj/boat.obj", mesh_vertices);
			
			for (auto &v : mesh_vertices) {
				vertices.push_back(v);
			}

			heart_vertices.count = uint32_t(vertices.size()) - heart_vertices.first;
		}

		{ //object 1: a quadrilateral	

			/* (x, z)
				(-length, -depth)	(-length, -depth)
					   C  /--------\ D
						 /		    \
					 A	/------------\ B
				(-length, depth)	(length, depth)
			*/

			plane_vertices.first = uint32_t(vertices.size());

			const float height = -0.0f;
			const float length = 2.0f;
			const float depth = 1.0f;

			// triangle ABC
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = -length, .y = height, .z = -depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
				.TexCoord{ .s = 0.0f, .t = 0.0f },
			});
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = -length, .y = height, .z = depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
				.TexCoord{ .s = 1.0f, .t = 0.0f },
			});
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = length, .y = height, .z = -depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
				.TexCoord{ .s = 0.0f, .t = 1.0f },
			});

			// triangle DCB
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = length, .y = height, .z = depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
				.TexCoord{ .s = 1.0f, .t = 1.0f },
			});
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = length, .y = height, .z = -depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
				.TexCoord{ .s = 0.0f, .t = 1.0f },
			});
			vertices.emplace_back(PosNorTexVertex{
				.Position{ .x = -length, .y = height, .z = depth },
				.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
				.TexCoord{ .s = 1.0f, .t = 0.0f },
			});

			plane_vertices.count = uint32_t(vertices.size()) - plane_vertices.first;
		}

		{ //object 2: a torus
			torus_vertices.first = uint32_t(vertices.size());

			// TODO: torus geometry
			//will parameterize the torus with (u,v) where:
			//	- u is angle around main axis (+z)
			//	- v is angle around tube axis (+y)

			constexpr float R1 = 0.75f; //main radius
			constexpr float R2 = 0.15f; //tube radius

			constexpr uint32_t U_STEPS = 20;
			constexpr uint32_t V_STEPS = 16;

			//texture repeats around the torus:
			constexpr float V_REPEATS = 2.0f;
			const float U_REPEATS = std::ceil(V_REPEATS / R2 * R1); // (U_REPEATS, V_REPEATS) proportional to the (R1, R2)

			auto emplace_vertex = [&](uint32_t ui, uint32_t vi) {
				//convert steps to angles;
				// (doing the mod since trig on 2 M_PI may not exactly match 0)
				float ua = (ui % U_STEPS) / float(U_STEPS) * 2.0f * float(M_PI);
				float va = (vi % V_STEPS) / float(V_STEPS) * 2.0f * float(M_PI);

    			// calculate the origin torus position
				float torus_x = (R1 + R2 * std::cos(va)) * std::cos(ua);
				float torus_y = (R1 + R2 * std::cos(va)) * std::sin(ua);
				float torus_z = R2 * std::sin(va);

				vertices.emplace_back( PosNorTexVertex{
					
					//rotate 90deg around x to make it aligned with my plane
					.Position{
						.x = torus_x,
						.y = -torus_z,
						.z = torus_y
					},
					.Normal{
						.x = std::cos(va) * std::cos(ua),
						.y = std::cos(va) * std::sin(ua),
						.z = std::sin(va)
					},
					.TexCoord{
						.s = (ui) / float(U_STEPS) * U_REPEATS,
						.t = (vi) / float(V_STEPS) * V_REPEATS
					}
				});
			};

			for (uint32_t ui = 0; ui < U_STEPS; ++ ui) {
				for (uint32_t vi = 0; vi < V_STEPS; ++ vi) {
					emplace_vertex(ui, vi + 1);
					emplace_vertex(ui, vi);
					emplace_vertex(ui + 1, vi);

					emplace_vertex(ui, vi + 1);
					emplace_vertex(ui + 1, vi);
					emplace_vertex(ui + 1, vi + 1);
				}
			}

			torus_vertices.count = uint32_t(vertices.size()) - torus_vertices.first;
		}

		size_t bytes = vertices.size() * sizeof(vertices[0]);

		object_vertices = rtg.helpers.create_buffer(
			bytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //use as a vertex buffer, and a target of a memory copy
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		//copy data to buffer
		rtg.helpers.transfer_to_buffer(vertices.data(), bytes, object_vertices);
	}
	//Edit End ===============================================================================================================
}

Tutorial::~Tutorial()
{
	// just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS)
	{
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	rtg.helpers.destroy_buffer(std::move(object_vertices));

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

		if (workspace.Camera_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera_src));
		}

		if (workspace.Camera.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}
		//Camera_descriptors is freed when pool is destroyed.
		//Edit End ===========================================================================================================
	}
	workspaces.clear();

	//Edit Start =========================================================================================================
	if (descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
		descriptor_pool = VK_NULL_HANDLE;
		// (this also frees the descriptor sets allocated from the pool)
	}

	background_pipeline.destroy(rtg);
	lines_pipeline.destroy(rtg);
	objects_pipeline.destroy(rtg);
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

	{ //upload camera info:
		LinesPipeline::Camera camera{
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD	
		};
		assert(workspace.Camera_src.size == sizeof(camera));

		//host-side copy into Camera_src:
		assert(workspace.Camera_src.allocation.mapped);
		std::memcpy(workspace.Camera_src.allocation.data(), &camera, sizeof(camera));

		//add device-side copy from Camera_src to Camera:
		assert(workspace.Camera_src.size == workspace.Camera.size);
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.Camera_src.size
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.Camera_src.handle, workspace.Camera.handle, 1, &copy_region);
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

			{ // bind Camera descriptor set:
				std::array< VkDescriptorSet, 1 > descriptor_sets{
					workspace.Camera_descriptors,	// set0: Camera descriptor set
				};
				vkCmdBindDescriptorSets(
					workspace.command_buffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,	//pipeline bind point
					lines_pipeline.layout,				//pipeline layout
					0,									//the set number of the first descriptor set to be bound
					uint32_t(descriptor_sets.size()), descriptor_sets.data(),	//descriptor sets count, ptr
					0, nullptr							//dynamic offsets count, ptr
				);
			};

			//draw lines vertices:
			vkCmdDraw(workspace.command_buffer, uint32_t(lines_vertices.size()), 1, 0, 0);
		};

		{ // draw with the objects pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.handle);

			{ //use object_vertices (offset 0) as vertex buffer binding 0:
				std::array< VkBuffer, 1 > vertex_buffers{ object_vertices.handle };
				std::array< VkDeviceSize, 1 > offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
			}

			//Camera descriptor set is already bound from the lines pipeline

			//draw all vertices:
			vkCmdDraw(workspace.command_buffer, uint32_t(object_vertices.size / sizeof(ObjectsPipeline::Vertex)), 1, 0, 0);
		}

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

	{ //set camera matrix (orbiting the origin):
		float rotate_speed = 5.0f;
		float ang = (float(M_PI) * 2.0f * rotate_speed) * (time / 60.0f);
		float fov = 60.0f;

		//camera (heart)
		// float lookat_distance = 150.0f;
		
		// CLIP_FROM_WORLD = perspective(
		// 	fov / float(M_PI) * 180.0f, 											//fov in radians
		// 	float(rtg.swapchain_extent.width) / float(rtg.swapchain_extent.height),	// aspect
		// 	0.1f,	//near
		// 	1000.0f //far
		// ) * look_at(
		// 	lookat_distance * std::cos(ang), 0.0f, lookat_distance * std::sin(ang),	//eye
		// 	0.0f, 0.0f, 0.0f, 														//target
		// 	0.0f, 1.0f, 0.0f 														//up
		// );

		//camera (guitar)
		// float lookat_distance = 1.5f;
		
		// CLIP_FROM_WORLD = perspective(
		// 	fov / float(M_PI) * 180.0f, 											//fov in radians
		// 	float(rtg.swapchain_extent.width) / float(rtg.swapchain_extent.height),	// aspect
		// 	0.1f,	//near
		// 	1000.0f //far
		// ) * look_at(
		// 	lookat_distance * std::cos(ang), 0.5f, lookat_distance * std::sin(ang),	//eye
		// 	0.0f, 0.5f, 0.0f, 														//target
		// 	0.0f, 1.0f, 0.0f 														//up
		// );

		//camera (boat)
		float lookat_distance = 0.7f;
		
		CLIP_FROM_WORLD = perspective(
			fov / float(M_PI) * 180.0f, 											//fov in radians
			float(rtg.swapchain_extent.width) / float(rtg.swapchain_extent.height),	// aspect
			0.1f,	//near
			1000.0f //far
		) * look_at(
			lookat_distance * std::cos(ang), 0.2f, lookat_distance * std::sin(ang),	//eye
			0.0f, 0.2f, 0.0f, 														//target
			0.0f, 1.0f, 0.0f 														//up
		);

	}

	// { //set input vertices (an 'x'):
	// 	lines_vertices.clear();
	// 	lines_vertices.reserve(4);

	// 	lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 		.Position{ .x = -1.0f, .y = -1.0f, .z = 0.0f },
	// 		.Color { .r = 0xff, .g = 0x00, .b = 0x00, .a = 0xff }
	// 	});

	// 	lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 		.Position{ .x = 1.0f, .y = 1.0f, .z = 0.0f },
	// 		.Color{ .r = 0x00, .g = 0xff, .b = 0x00, .a = 0xff }
	// 	});

	// 	lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 		.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
	// 		.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
	// 	});

	// 	lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 		.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
	// 		.Color{ .r = 0x00, .g = 0x00, .b = 0x00, .a = 0xff }
	// 	});

	// 	assert(lines_vertices.size() == 4);
	// };

	// { //set input vertices (crossing lines at different depth):
	// 	lines_vertices.clear();
	// 	constexpr size_t count = 2 * 30 + 2 * 30;
	// 	lines_vertices.reserve(count);

	// 	//horizontal lines at z = 0.5f
	// 	for (uint32_t i = 0; i < 30; ++ i) {
	// 		float y = (i + 0.5f) / 30.0f * 2.0f - 1.0f; // [0.0,30.0] -> [0.0,1.0] -> [-1.0,1.0]
	// 		lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 			.Position{ .x = -1.0f, .y = y, .z = 0.5f },
	// 			.Color{ .r = 0x00, .g = 0x00, .b = 0x00, .a = 0xff }
	// 		});
	// 		lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 			.Position{ .x = 1.0f, .y = y, .z = 0.5f },
	// 			.Color{ .r = 0x00, .g = 0x00, .b = 0x00, .a = 0xff }
	// 		});
	// 	}

	// 	//vertical lines at z = 0.0f (near) through 1.0f (far)
	// 	for (uint32_t i= 0; i < 30; ++ i) {
	// 		float x = (i + 0.5f) / 30.0f * 2.0f - 1.0f;
	// 		lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 			.Position{ .x = x, .y = -1.0f, .z = 0.0f },
	// 			.Color{ .r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff }
	// 		});
	// 		lines_vertices.emplace_back(LinesPipeline::Vertex{
	// 			.Position{ .x = x, .y = 1.0f, .z = 1.0f },
	// 			.Color{ .r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff }
	// 		});
	// 	}
	// 	assert(lines_vertices.size() == count);
	// };


	//HACK: transform vertices to clip space on CPU:
	// for (PosColVertex &v : lines_vertices) {
	// 	vec4 res = CLIP_FROM_WORLD * vec4{ v.Position.x, v.Position.y, v.Position.z, 1.0f };
	// 	v.Position.x = res[0] / res[3];
	// 	v.Position.y = res[1] / res[3];
	// 	v.Position.z = res[2] / res[3];
	// }

	//Edit End ===========================================================================================================
}

void Tutorial::on_input(InputEvent const &)
{
}
