#ifdef _WIN32
// ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "Source/Application/Wanderer/Wanderer.hpp"
#include "Source/Tools/LoadMgr.hpp"
#include "Source/Tools/SceneMgr.hpp"
#include "Source/Tools/TypeHelper.hpp"
#include "Source/Helper/VK.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <GLFW/glfw3.h>

// #define STB_IMAGE_IMPLEMENTATION
// #include "lib/stb_image.h"

Wanderer::Wanderer(RTG &rtg_) : rtg(rtg_)
{
	// set up application prerequisites
	init_depth_format();
	create_render_pass();
	create_command_pool();
	create_pipelines();
	create_description_pool();
	setup_workspaces();

	// load scene graph related info
	SceneMgr &sceneMgr = rtg.configuration.sceneMgr;
	LoadMgr::load_scene_graph_info_from_s72(rtg.configuration.scene_graph_path, sceneMgr);
	LoadMgr::load_s72_node_matrices(sceneMgr);

	// update animation time
	animation_timer.tmax = sceneMgr.get_animation_duration();

	// update scene camera info in sceneMgr
	sceneMgr.sceneCameraCount = sceneMgr.cameraObjectMap.size();

	// 	if given scene camera
	Camera &camera = rtg.configuration.camera;
	if (sceneMgr.sceneCameraCount > 0)
	{
		const std::string &target_scene_camera =  rtg.configuration.specified_default_camera;
		// (main camera)
		if (target_scene_camera != "")
		{
			// use specified scene camera as the default camera
			sceneMgr.currentSceneCameraItr = sceneMgr.cameraObjectMap.find(target_scene_camera);
			if (sceneMgr.currentSceneCameraItr == sceneMgr.cameraObjectMap.end()) {
				throw std::runtime_error("Scene camera object named \"" + target_scene_camera + "\" not found. Application exits.");
			}
		}
		else
		{
			// use the first scene camera in map as the default camera
			camera.current_camera_mode = Camera::Camera_Mode::SCENE;
			sceneMgr.currentSceneCameraItr = sceneMgr.cameraObjectMap.begin();
		}
		CLIP_FROM_WORLD = rtg.configuration.camera.apply_scene_mode_camera(sceneMgr);

		// (user camera) initialize using the main camera setting
		rtg.configuration.user_camera.current_camera_mode = Camera::Camera_Mode::USER;
		rtg.configuration.user_camera.update_info_from_another_camera(rtg.configuration.camera);

		// (debug camera) initialize using the main camera setting
		rtg.configuration.debug_camera.current_camera_mode = Camera::Camera_Mode::DEBUG;
		rtg.configuration.debug_camera.update_info_from_another_camera(rtg.configuration.camera);
	}
	// 	if not given scene camera
	else
	{
		// (main camera) to be in the user mode
		camera.current_camera_mode = Camera::Camera_Mode::USER;

		// make the camera initially looking toward a root node
		std::string &rootNodeName = sceneMgr.sceneObject->rootName[0];
		SceneMgr::NodeObject *rootNode = sceneMgr.nodeObjectMap.find(rootNodeName)->second;
		glm::mat4 root_matrix = sceneMgr.nodeMatrixMap.find(rootNode->name)->second;

		glm::vec3 root_translation = glm::vec3(root_matrix[3]);
		camera.position = root_translation + glm::vec3(0.0f, 0.0f, 2.0f);
		camera.target_position = glm::vec3(0.f, 0.f, 0.f);
		camera.front = glm::normalize(camera.target_position - camera.position); 
		camera.update_camera_eular_angles_from_vectors();

		// (user camera) skip; no need to set because no user camera setting backup is needed

		// (debug camera) initialize using the main camera setting
		rtg.configuration.debug_camera.current_camera_mode = Camera::Camera_Mode::DEBUG;
		rtg.configuration.debug_camera.update_info_from_another_camera(rtg.configuration.camera);
	}

	// load vertices resources
	load_lines_vertices();
	// load_objects_vertices();
	load_scene_objects_vertices();

	// set up textures
	setup_environment_cubemap(true);
	create_diy_textures();
	create_textures_descriptor();
}

Wanderer::~Wanderer()
{
	// just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS)
	{
		std::cerr << "Failed to vkDeviceWaitIdle in Wanderer::~Wanderer [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	// remove static resources
	if (texture_descriptor_pool)
	{
		vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
		texture_descriptor_pool = VK_NULL_HANDLE;

		// (this also frees the descriptor sets allocated from the pool)
		texture_descriptors.clear();
	}

	if (texture_sampler)
	{
		vkDestroySampler(rtg.device, texture_sampler, nullptr);
		texture_sampler = VK_NULL_HANDLE;
	}

	for (VkImageView &view : texture_views)
	{
		vkDestroyImageView(rtg.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	texture_views.clear();

	for (Helpers::AllocatedImage &image : textures)
	{
		rtg.helpers.destroy_image(std::move(image));
	}
	textures.clear();

	rtg.helpers.destroy_buffer(std::move(object_vertices));
	rtg.helpers.destroy_buffer(std::move(env_cubemap_buffer));

	// remove swapchain-dependent resources
	if (swapchain_depth_image.handle != VK_NULL_HANDLE)
	{
		destroy_framebuffers();
	}

	// remove per-workspace resources
	for (Workspace &workspace : workspaces)
	{
		if (workspace.command_buffer != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(rtg.device, command_pool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}

		if (workspace.lines_vertices_src.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
		}
		if (workspace.lines_vertices.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
		}

		if (workspace.Camera_src.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Camera_src));
		}

		if (workspace.Camera.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}
		// Camera_descriptors is freed when pool is destroyed.

		if (workspace.World_src.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.World_src));
		}

		if (workspace.World.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.World));
		}
		// World_descriptors is freed when pool is destroyed.

		if (workspace.Transforms_src.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
		}

		if (workspace.Transforms.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
		}
		// Transform_descriptors is freed when pool is destroyed.
	}
	workspaces.clear();

	if (descriptor_pool)
	{
		vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
		descriptor_pool = VK_NULL_HANDLE;
		// (this also frees the descriptor sets allocated from the pool)
	}

	background_pipeline.destroy(rtg);
	lines_pipeline.destroy(rtg);
	objects_pipeline.destroy(rtg);

	if (render_pass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}

	if (command_pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(rtg.device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}
}

void Wanderer::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) // re-makes the depth buffer at the correct size, and get a view of it
{
	// clean up existing framebuffers
	if (swapchain_depth_image.handle != VK_NULL_HANDLE)
	{
		destroy_framebuffers();
	}

	// allocate depth image for framebuffers to share
	swapchain_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped);

	// create depth image view
	{
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1}};
		VK(vkCreateImageView(rtg.device, &create_info, nullptr, &swapchain_depth_image_view));
	};

	// make framebuffers for each swapchain image:
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i)
	{
		std::array<VkImageView, 2> attachments{
			swapchain.image_views[i],
			swapchain_depth_image_view,
		};
		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1};
		VK(vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]));
	}

	std::cout << "[Wanderer] (Swapchain count) recreating " << swapchain.images.size() << " swapchains" << std::endl;
}

void Wanderer::destroy_framebuffers()
{
	for (VkFramebuffer &framebuffer : swapchain_framebuffers)
	{
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	assert(swapchain_depth_image_view != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, swapchain_depth_image_view, nullptr);
	swapchain_depth_image_view = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(swapchain_depth_image));
}

void Wanderer::render(RTG &rtg_, RTG::RenderParams const &render_params)
{

	// assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	// get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	[[maybe_unused]] VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	// //record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:

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
	if (!lines_vertices.empty())
	{

		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = lines_vertices.size() * sizeof(lines_vertices[0]);

		if (workspace.lines_vertices_src.handle == VK_NULL_HANDLE || workspace.lines_vertices_src.size < needed_bytes)
		{
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096; // round up to nearest 4k to avoid re-allocat1ing
																	  //  continuously if vertex count grows slowly

			if (workspace.lines_vertices_src.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
			}
			if (workspace.lines_vertices.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
			}

			workspace.lines_vertices_src = rtg.helpers.create_buffer( //"staging" buffer, storing a frame's lines data using the CPU
				new_bytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,											// have GPU copy data from lines_vertices_src
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host visible memory, coherent (no special sync needed)
				Helpers::Mapped																// put it somewhere in the CPU address space
			);
			workspace.lines_vertices = rtg.helpers.create_buffer( // vertex buffer on GPU side
				new_bytes,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a vertex buffer, and a target of a memory copy
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,								  // on GPU, not host visible
				Helpers::Unmapped);

			std::cout << "Workspace #" << render_params.workspace_index << " ";
			std::cout << "Re-allocating lines buffers to " << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.lines_vertices_src.size == workspace.lines_vertices.size);
		assert(workspace.lines_vertices_src.size >= needed_bytes);

		// host-side copy into liens_vertices_src:
		assert(workspace.lines_vertices_src.allocation.mapped);
		std::memcpy(workspace.lines_vertices_src.allocation.data(), lines_vertices.data(), needed_bytes);

		// device-side copy from lines_vertices_src to lines_vertices:
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.lines_vertices_src.handle, workspace.lines_vertices.handle, 1, &copy_region);
	};

	{ // upload camera info:
		LinesPipeline::Camera camera{
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD};
		assert(workspace.Camera_src.size == sizeof(camera));

		// host-side copy into Camera_src:
		assert(workspace.Camera_src.allocation.mapped);
		std::memcpy(workspace.Camera_src.allocation.data(), &camera, sizeof(camera));

		// add device-side copy from Camera_src to Camera:
		assert(workspace.Camera_src.size == workspace.Camera.size);
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.Camera_src.size};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.Camera_src.handle, workspace.Camera.handle, 1, &copy_region);
	};

	{ // upload world info:
		assert(workspace.Camera_src.size == sizeof(world));

		// host-side copy into World_src:
		memcpy(workspace.World_src.allocation.data(), &world, sizeof(world));

		// add device-side copy from World_src to World:
		assert(workspace.World_src.size == workspace.World.size);
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.World_src.size};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.World_src.handle, workspace.World.handle, 1, &copy_region);
	};

	// upload object transforms info:
	if (!object_instances.empty())
	{

		// [re-]allocate object transforms buffers if needed:
		size_t needed_bytes = object_instances.size() * sizeof(ObjectsPipeline::Transform);

		// check if we need to re-allocate the buffers:
		if (workspace.Transforms_src.handle == VK_NULL_HANDLE || workspace.Transforms_src.size < needed_bytes)
		{
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096; // round up to nearest 4k to avoid re-allocat1ing
																	  //  continuously if vertex count grows slowly

			if (workspace.Transforms_src.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
			}
			if (workspace.Transforms.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
			}

			workspace.Transforms_src = rtg.helpers.create_buffer(
				new_bytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,											// have GPU copy data from Transforms_src
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host visible memory, coherent (no special sync needed)
				Helpers::Mapped																// put it somewhere in the CPU address space
			);
			workspace.Transforms = rtg.helpers.create_buffer(
				new_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a storage buffer, and a target of a memory copy
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,								   // on GPU, not host visible
				Helpers::Unmapped);

			// update the descriptor set:
			VkDescriptorBufferInfo Transforms_info{
				.buffer = workspace.Transforms.handle,
				.offset = 0,
				.range = workspace.Transforms.size};

			std::array<VkWriteDescriptorSet, 1> writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Transform_descriptors, // make descriptors stay up to date with the re-allocated transforms buffer
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Transforms_info}};

			vkUpdateDescriptorSets(
				rtg.device,
				uint32_t(writes.size()), writes.data(), // descriptorWrites count, data
				0, nullptr								// descriptorCopies count, data
			);

			std::cout << "Re-allocating Transforms buffers to " << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Transforms_src.size == workspace.Transforms.size);
		assert(workspace.Transforms_src.size >= needed_bytes);

		{ // copy transforms into Transforms_src:
			assert(workspace.Transforms_src.allocation.mapped);

			ObjectsPipeline::Transform *out = reinterpret_cast<ObjectsPipeline::Transform *>(
				workspace.Transforms_src.allocation.data()); // the transforms data is built directly into the mapped transforms sources memory, avoiding any copies

			for (ObjectInstance const &inst : object_instances)
			{
				*out = inst.transform;
				++out;
			}

			// device-side copy from Transforms_src -> Transforms:
			VkBufferCopy copy_region{
				.srcOffset = 0,
				.dstOffset = 0,
				.size = needed_bytes};

			vkCmdCopyBuffer(workspace.command_buffer, workspace.Transforms_src.handle, workspace.Transforms.handle, 1, &copy_region);
		}
	}

	{ // memory barrier to make sure copies complete before render pass:
		VkMemoryBarrier memory_barrier{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, // build barrier between src stage mask (Writring) -
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT	 // & dst stage mask (Reading)
		};

		vkCmdPipelineBarrier(
			workspace.command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,		// src stage mask
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, // dst stage mask
			0,									// dependency flags
			1, &memory_barrier,					// memory barriers (count, data)
			0, nullptr,							// buffer memory barriers (count, data)
			0, nullptr							// image memory barriers (count, data)
		);
	};

	// GPU commands here
	 // render pass：describes layout, "input from", "output to" of attachments

		// set clear value (black)
		[[maybe_unused]] std::array<VkClearValue, 2> clear_values{
			VkClearValue{.color{.float32{0.6f, 0.6f, 0.6f, 1.0f}}},
			VkClearValue{.depthStencil{.depth = 1.0f, .stencil = 0}},
		};

		// set render pass begin info
		VkRenderPassBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			//.pNext = nullptr,
			.renderPass = render_pass,
			.framebuffer = framebuffer, // provide references to specific attachments
			.renderArea{
				.offset = {.x = 0, .y = 0},		// starting point of render area within framebuffer
				.extent = rtg.swapchain_extent, // size of render area
			},

			.clearValueCount = uint32_t(clear_values.size()),
			.pClearValues = clear_values.data(),
		};

		// begin render pass
		vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Run pipelines here ==================================================================================================

		// correct the window height and width
		float swapchain_aspect = rtg.swapchain_extent.width / rtg.swapchain_extent.height;
		float &camera_aspect = rtg.configuration.camera.camera_attributes.aspect;

		float new_height = rtg.swapchain_extent.height;
		float new_width = rtg.swapchain_extent.width;
		float offset_x = 0.f;
		float offset_y = 0.f;

		if (camera_aspect > swapchain_aspect) // letterbox
		{
			new_width = rtg.swapchain_extent.height * camera_aspect;
			offset_x = (float(rtg.swapchain_extent.width) - new_width) / 2.f;
		}
		else  // pillarbox
		{
			new_height = rtg.swapchain_extent.width / camera_aspect;
			offset_y = (float(rtg.swapchain_extent.height) - new_height) / 2.f;
		}

		{ // configure scissor rectangle
			VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = {.width = static_cast<uint32_t>(new_width), .height = static_cast<uint32_t>(new_height)}
			};
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor); //(xxx, index of first scissor, \
																		//count of scissor affected, address of scissor)
		};
		{ // configure viewport transform
			VkViewport viewport{
				.x = offset_x,
				.y = offset_y,
				.width = new_width,
				.height = new_height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);
		};

		// { // draw with the background pipeline
		// 	vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline.handle);

		// 	{ // push time:
		// 		BackgroundPipeline::Push push{
		// 			.time = float(time)};
		// 		vkCmdPushConstants(workspace.command_buffer, background_pipeline.layout,
		// 						   VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		// 	};

		// 	vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		// };

		// { // draw with the lines pipeline
		// 	vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.handle);

		// 	{ // use lines_vertices (offset 0) as vertex buffer binding 0:
		// 		std::array<VkBuffer, 1> vertex_buffers{workspace.lines_vertices.handle};
		// 		std::array<VkDeviceSize, 1> offsets{0};
		// 		vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
		// 	};

		// 	{ // bind Camera descriptor set:
		// 		std::array<VkDescriptorSet, 1> descriptor_sets{
		// 			workspace.Camera_descriptors, // set0: Camera descriptor set
		// 		};
		// 		vkCmdBindDescriptorSets(
		// 			workspace.command_buffer,
		// 			VK_PIPELINE_BIND_POINT_GRAPHICS,						  // pipeline bind point
		// 			lines_pipeline.layout,									  // pipeline layout
		// 			0,														  // the set number of the first descriptor set to be bound
		// 			uint32_t(descriptor_sets.size()), descriptor_sets.data(), // descriptor sets count, ptr
		// 			0, nullptr												  // dynamic offsets count, ptr
		// 		);
		// 	};

		// 	// draw lines vertices:
		// 	vkCmdDraw(workspace.command_buffer, uint32_t(lines_vertices.size()), 1, 0, 0);
		// };

		{ // draw with the objects pipeline
			if (!object_instances.empty())
			{
				vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.handle);

				{ // use object_vertices (offset 0) as vertex buffer binding 0:
					std::array<VkBuffer, 1> vertex_buffers{object_vertices.handle};
					std::array<VkDeviceSize, 1> offsets{0};
					vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
				}

				// Camera descriptor set is already bound from the lines pipeline

				{ // bind Transforms descriptor set:
					std::array<VkDescriptorSet, 2> descriptor_sets{
						workspace.World_descriptors,	 // set0: World descriptor set
						workspace.Transform_descriptors, // set1: Transforms descriptor set
					};

					vkCmdBindDescriptorSets(
						workspace.command_buffer,								  // command buffer
						VK_PIPELINE_BIND_POINT_GRAPHICS,						  // pipeline bind point
						objects_pipeline.layout,								  // pipeline layout
						0,														  // first set
						uint32_t(descriptor_sets.size()), descriptor_sets.data(), // descriptor sets count, ptr
						0, nullptr												  // dynamic offsets count, ptr
					);
				}

				// draw all vertices:
				for (ObjectInstance const &inst : object_instances)
				{
					uint32_t index = uint32_t(&inst - &object_instances[0]);

					// bind texture descriptor set:
					vkCmdBindDescriptorSets(
						workspace.command_buffer,			   // command_buffer
						VK_PIPELINE_BIND_POINT_GRAPHICS,	   // pipeline bind point
						objects_pipeline.layout,			   // pipeline layout
						2,									   // second set
						1, &texture_descriptors[inst.texture], // descriptor sets count, ptr
						0, nullptr							   // dynamic offsets count, ptr
					);

					vkCmdDraw(workspace.command_buffer, inst.vertices.count, 1, inst.vertices.first, index);
				}
			}
		}

		vkCmdEndRenderPass(workspace.command_buffer);
	;

	// end recording command_buffer
	VK(vkEndCommandBuffer(workspace.command_buffer));

	// submit `workspace.command buffer` for the GPU to run:
	{
		std::array<VkSemaphore, 1> wait_semaphores{
			render_params.image_available};
		std::array<VkPipelineStageFlags, 1> wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		std::array<VkSemaphore, 1> signal_semaphores{
			render_params.image_done};
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()), // whcih semaphore the GPU should wait for before executing this task
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()), // signaled when GPU has finished rendering into this swapchain image
			.pSignalSemaphores = signal_semaphores.data()};

		VK(vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available));
	}

}

void Wanderer::update(float dt)
{

	// update time1
	time = std::fmod(time + dt, 60.0f); // avoid precision issues by keeping time in a reasonable range
	animation_timer.update(dt);

	Camera &camera = rtg.configuration.camera;
	Camera &debug_camera = rtg.configuration.debug_camera;

	// ===============================================
	// set camera matrix
	{ 
		// ------------------------------------------------------------------------------
		// camera control

		// USER mode, main camera control
		if (camera.current_camera_mode == Camera::USER)
		{
			// keyboard & camera movement
			if (camera.movements.up && !camera.movements.down) {
				if (camera.sensitivity.sensitivity_increase && !camera.sensitivity.sensitivity_decrease) { camera.sensitivity.kb_upward += camera.unit_sensitivity; }
				camera.position += camera.sensitivity.kb_upward * camera.up;
			} else if (camera.movements.down && !camera.movements.up) {
				if (camera.sensitivity.sensitivity_decrease && !camera.sensitivity.sensitivity_increase) { camera.sensitivity.kb_upward -= camera.unit_sensitivity; }
				camera.position -= camera.sensitivity.kb_upward * camera.up;
			} 

			if (camera.movements.left && !camera.movements.right) {
				if (camera.sensitivity.sensitivity_increase && !camera.sensitivity.sensitivity_decrease) { camera.sensitivity.kb_rightward += camera.unit_sensitivity; }
				camera.position -= camera.sensitivity.kb_rightward * camera.right;
			} else if (camera.movements.right && !camera.movements.left) {
				if (camera.sensitivity.sensitivity_decrease && !camera.sensitivity.sensitivity_increase) { camera.sensitivity.kb_rightward -= camera.unit_sensitivity; }
				camera.position += camera.sensitivity.kb_rightward * camera.right;
			}

			if (camera.movements.forward && !camera.movements.backward) {
				if (camera.sensitivity.sensitivity_increase && !camera.sensitivity.sensitivity_decrease) { camera.sensitivity.kb_forward += camera.unit_sensitivity; }
				camera.position += camera.sensitivity.kb_forward * camera.front;
			} else if (camera.movements.backward && !camera.movements.forward) {
				if (camera.sensitivity.sensitivity_decrease && !camera.sensitivity.sensitivity_increase) { camera.sensitivity.kb_forward -= camera.unit_sensitivity; }
				camera.position -= camera.sensitivity.kb_forward * camera.front;
			}

			if (camera.postures.yaw_left && !camera.postures.yaw_right) {
				if (camera.sensitivity.sensitivity_increase && !camera.sensitivity.sensitivity_decrease) { camera.sensitivity.kb_yaw += camera.unit_sensitivity; }
				camera.yaw -= camera.sensitivity.kb_yaw * camera.unit_angle;
			} else if (camera.postures.yaw_right && !camera.postures.yaw_left) {
				if (camera.sensitivity.sensitivity_decrease && !camera.sensitivity.sensitivity_increase) { camera.sensitivity.kb_yaw -= camera.unit_sensitivity; }
				camera.yaw += camera.sensitivity.kb_yaw * camera.unit_angle;
			}

			if (camera.postures.pitch_up && !camera.postures.pitch_down) {
				if (camera.sensitivity.sensitivity_increase && !camera.sensitivity.sensitivity_decrease) { camera.sensitivity.kb_pitch += camera.unit_sensitivity; }
				camera.pitch += camera.sensitivity.kb_pitch * camera.unit_angle;
			} else if (camera.postures.pitch_down && !camera.postures.pitch_up) {
				if (camera.sensitivity.sensitivity_decrease && !camera.sensitivity.sensitivity_increase) { camera.sensitivity.kb_pitch -= camera.unit_sensitivity; }
				camera.pitch -= camera.sensitivity.kb_pitch * camera.unit_angle;
			}

			camera.update_camera_vectors_from_eular_angles();

			// mouse & rotation
			// [TODO]

		}

		// SCENE mode, no control, skip

		// DEBUG mode, debug camera control
		if (debug_camera.current_camera_mode == Camera::DEBUG)
		{
			// keyboard & camera movement
			if (debug_camera.movements.up && !debug_camera.movements.down) {
				if (debug_camera.sensitivity.sensitivity_increase && !debug_camera.sensitivity.sensitivity_decrease) { debug_camera.sensitivity.kb_upward += debug_camera.unit_sensitivity; }
				debug_camera.position += debug_camera.sensitivity.kb_upward * debug_camera.up;
			} else if (debug_camera.movements.down && !debug_camera.movements.up) {
				if (debug_camera.sensitivity.sensitivity_decrease && !debug_camera.sensitivity.sensitivity_increase) { debug_camera.sensitivity.kb_upward -= debug_camera.unit_sensitivity; }
				debug_camera.position -= debug_camera.sensitivity.kb_upward * debug_camera.up;
			} 

			if (debug_camera.movements.left && !debug_camera.movements.right) {
				if (debug_camera.sensitivity.sensitivity_increase && !debug_camera.sensitivity.sensitivity_decrease) { debug_camera.sensitivity.kb_rightward += debug_camera.unit_sensitivity; }
				debug_camera.position -= debug_camera.sensitivity.kb_rightward * debug_camera.right;
			} else if (debug_camera.movements.right && !debug_camera.movements.left) {
				if (debug_camera.sensitivity.sensitivity_decrease && !debug_camera.sensitivity.sensitivity_increase) { debug_camera.sensitivity.kb_rightward -= debug_camera.unit_sensitivity; }
				debug_camera.position += debug_camera.sensitivity.kb_rightward * debug_camera.right;
			}

			if (debug_camera.movements.forward && !debug_camera.movements.backward) {
				if (debug_camera.sensitivity.sensitivity_increase && !debug_camera.sensitivity.sensitivity_decrease) { debug_camera.sensitivity.kb_forward += debug_camera.unit_sensitivity; }
				debug_camera.position += debug_camera.sensitivity.kb_forward * debug_camera.front;
			} else if (debug_camera.movements.backward && !debug_camera.movements.forward) {
				if (debug_camera.sensitivity.sensitivity_decrease && !debug_camera.sensitivity.sensitivity_increase) { debug_camera.sensitivity.kb_forward -= debug_camera.unit_sensitivity; }
				debug_camera.position -= debug_camera.sensitivity.kb_forward * debug_camera.front;
			}

			if (debug_camera.postures.yaw_left && !debug_camera.postures.yaw_right) {
				if (debug_camera.sensitivity.sensitivity_increase && !debug_camera.sensitivity.sensitivity_decrease) { debug_camera.sensitivity.kb_yaw += debug_camera.unit_sensitivity; }
				debug_camera.yaw -= debug_camera.sensitivity.kb_yaw * debug_camera.unit_angle;
			} else if (debug_camera.postures.yaw_right && !debug_camera.postures.yaw_left) {
				if (debug_camera.sensitivity.sensitivity_decrease && !debug_camera.sensitivity.sensitivity_increase) { debug_camera.sensitivity.kb_yaw -= debug_camera.unit_sensitivity; }
				debug_camera.yaw += debug_camera.sensitivity.kb_yaw * debug_camera.unit_angle;
			}

			if (debug_camera.postures.pitch_up && !debug_camera.postures.pitch_down) {
				if (debug_camera.sensitivity.sensitivity_increase && !debug_camera.sensitivity.sensitivity_decrease) { debug_camera.sensitivity.kb_pitch += debug_camera.unit_sensitivity; }
				debug_camera.pitch += debug_camera.sensitivity.kb_pitch * debug_camera.unit_angle;
			} else if (debug_camera.postures.pitch_down && !debug_camera.postures.pitch_up) {
				if (debug_camera.sensitivity.sensitivity_decrease && !debug_camera.sensitivity.sensitivity_increase) { debug_camera.sensitivity.kb_pitch -= debug_camera.unit_sensitivity; }
				debug_camera.pitch -= debug_camera.sensitivity.kb_pitch * debug_camera.unit_angle;
			}

			debug_camera.update_camera_vectors_from_eular_angles();

			// mouse & rotation
			// [TODO]
		}

		
		// ------------------------------------------------------------------------------
		// apply main camera changes in USER mode

		if (camera.current_camera_mode == Camera::Camera_Mode::USER) {

			glm::vec3 target_direction = camera.position + camera.front;

			CLIP_FROM_WORLD = perspective(
							  camera.camera_attributes.vfov,	// fov in radians
							  camera.camera_attributes.aspect,  // aspect
							  camera.camera_attributes.near,	// near
							  camera.camera_attributes.far	    // far
							  ) *
						  look_at(
							  camera.position[0], camera.position[1], camera.position[2], 	 // eye
							  target_direction[0], target_direction[1], target_direction[2], // target
							  camera.up[0], camera.up[1], camera.up[2]					   	 // up
						  );
		}


		// ------------------------------------------------------------------------------
		// SCENE mode camera changes only happen possibly after the animation driving, so update later, skip for now
		

		// ------------------------------------------------------------------------------
		// if in DEBUG mode, 
		// apply debug camera changes to CLIP_FROM_WORLD

		if (camera.current_camera_mode == Camera::Camera_Mode::DEBUG) {

			glm::vec3 target_direction = debug_camera.position + debug_camera.front;

			CLIP_FROM_WORLD = perspective(
							  debug_camera.camera_attributes.vfov,	  // fov in radians
							  debug_camera.camera_attributes.aspect,  // aspect
							  debug_camera.camera_attributes.near,	  // near
							  debug_camera.camera_attributes.far	  // far
							  ) *
						  look_at(
							  debug_camera.position[0], debug_camera.position[1], debug_camera.position[2], 	 // eye
							  target_direction[0], target_direction[1], target_direction[2],					 // target
							  debug_camera.up[0], debug_camera.up[1], debug_camera.up[2]					   	 // up
						  );
		}
	};

	// ===============================================
	// set world data (sun and sky)
	{ 
		world.SKY_DIRECTION.x = 0.0f;
		world.SKY_DIRECTION.y = 0.0f;
		world.SKY_DIRECTION.z = 1.0f;

		world.SKY_ENERGY.r = 0.1f;
		world.SKY_ENERGY.g = 0.1f;
		world.SKY_ENERGY.b = 0.2f;

		world.SUN_DIRECTION.x = 6.0f / 23.0f;
		world.SUN_DIRECTION.y = 13.0f / 23.0f;
		world.SUN_DIRECTION.z = 18.0f / 23.0f;

		world.SUN_ENERGY.r = 1.0f;
		world.SUN_ENERGY.g = 1.0f;
		world.SUN_ENERGY.b = 0.9f;
	};

	// ===============================================
	// apply drivers to nodes to animate the scene
	if (!animation_timer.paused) 
	{ 
		rtg.configuration.sceneMgr.update_nodes_from_animation_drivers(animation_timer.t);
    	LoadMgr::load_s72_node_matrices(rtg.configuration.sceneMgr);
		
		// update the clip from world matrix after animation is applied
		if (camera.current_camera_mode == Camera::Camera_Mode::SCENE)
		{
			CLIP_FROM_WORLD = camera.apply_scene_mode_camera(rtg.configuration.sceneMgr); // make senses when animation driver 
		}
	};

	// ===============================================
	// set objects transformation
	{ 
		object_instances.clear();

		// instances for all scene graph nodes
		construct_scene_graph_vertices_with_culling(object_instances, rtg.configuration.sceneMgr, CLIP_FROM_WORLD); 
	};
}

void Wanderer::on_input(InputEvent const &event)
{
	Camera &camera = rtg.configuration.camera;
	Camera &user_camera = rtg.configuration.user_camera;
	Camera &debug_camera = rtg.configuration.debug_camera;
	SceneMgr &sceneMgr = rtg.configuration.sceneMgr;

	if (event.type == InputEvent::KeyDown)
	{
		// Camera Mode ---------------------------------------------------------------------------------------------------------------------

		if (event.key.key == GLFW_KEY_1) // change to camera mode: SCENE
		{
			if (sceneMgr.sceneCameraCount == 0) {
				std::cout << "[Camera] (Mode) SCENE mode: no camera available." << std::endl;
				return;
			}

			// if changed from USER camera, save user camera setting
			if (camera.current_camera_mode == Camera::Camera_Mode::USER) 
				user_camera.update_info_from_another_camera(camera);

			// change camera mode
			camera.current_camera_mode = Camera::Camera_Mode::SCENE;
			this->CLIP_FROM_WORLD = camera.apply_scene_mode_camera(sceneMgr); 

			std::cout << "[Camera] (Mode) switched to SCENE mode, camera: " << sceneMgr.currentSceneCameraItr->second->name << std::endl;
		}
		else if (event.key.key == GLFW_KEY_2) // change to camera mode: USER
		{
			camera.current_camera_mode = Camera::Camera_Mode::USER;
			camera.update_info_from_another_camera(user_camera); // recover user camera setting

			std::cout << "[Camera] (Mode) switched to USER mode." << std::endl;
		}
		else if (event.key.key == GLFW_KEY_3) // change to camera mode: DEBUG
		{
			// if changed from USER camera, save user camera setting
			if (camera.current_camera_mode == Camera::Camera_Mode::USER) 
				user_camera.update_info_from_another_camera(camera);
			
			// change camera mode
			camera.current_camera_mode = Camera::Camera_Mode::DEBUG;

			std::cout << "[Camera] (Mode) switched to DEBUG mode." << std::endl;
		}
		else if (event.key.key == GLFW_KEY_V) // switch between SCENE cameras
		{
			if (rtg.configuration.camera.current_camera_mode == Camera::Camera_Mode::SCENE)
			{
				++ sceneMgr.currentSceneCameraItr;

				if (sceneMgr.currentSceneCameraItr == sceneMgr.cameraObjectMap.end())
					sceneMgr.currentSceneCameraItr = sceneMgr.cameraObjectMap.begin();
				
				this->CLIP_FROM_WORLD = camera.apply_scene_mode_camera(sceneMgr);

				std::cout << "[Camera] (Mode) SCENE mode: switched to " << sceneMgr.currentSceneCameraItr->second->name << " perspective." << std::endl;
			}
		}
		else if (event.key.key == GLFW_KEY_P)
		{
			animation_timer.pause_or_resume();
		}
		else if (event.key.key == GLFW_KEY_R)
		{
			animation_timer.reset();
		}

		// Camera Movement -----------------------------------------------------------------------------------------------------------------
			/* cr. learned from CMU 15666 Computer Game Programming code base
				https://github.com/15-466/15-466-f24-base2/blob/b7584e87b2498e4491e6438770f4b4a8d593bbde/PlayMode.cpp#L70 */

		// USER mode, camera control
		if (camera.current_camera_mode == Camera::Camera_Mode::USER)
		{
			// camera movements
			if (event.key.key == GLFW_KEY_W)
			{
				camera.movements.forward = true;
			}
			else if (event.key.key == GLFW_KEY_S)
			{
				camera.movements.backward = true;
			}
			else if (event.key.key == GLFW_KEY_A)
			{
				camera.movements.left = true;
			}
			else if (event.key.key == GLFW_KEY_D)
			{
				camera.movements.right = true;
			}
			else if (event.key.key == GLFW_KEY_Q)
			{
				camera.movements.up = true;
			}
			else if (event.key.key == GLFW_KEY_E)
			{
				camera.movements.down = true;
			}
			// camera postures
			else if (event.key.key == GLFW_KEY_UP)
			{
				camera.postures.pitch_up = true;
			}
			else if (event.key.key == GLFW_KEY_DOWN)
			{
				camera.postures.pitch_down = true;
			}
			else if (event.key.key == GLFW_KEY_LEFT)
			{
				camera.postures.yaw_left = true;
			}
			else if (event.key.key == GLFW_KEY_RIGHT)
			{
				camera.postures.yaw_right = true;
			}
			// camera sensitivity
			else if (event.key.key == GLFW_KEY_LEFT_BRACKET)
			{
				camera.sensitivity.sensitivity_decrease = true;
			}
			else if (event.key.key == GLFW_KEY_RIGHT_BRACKET)
			{
				camera.sensitivity.sensitivity_increase = true;
			}
		}

		// DEBUG mode, camera control
		if (camera.current_camera_mode == Camera::Camera_Mode::DEBUG)
		{
			// camera movements
			if (event.key.key == GLFW_KEY_W)
			{
				debug_camera.movements.forward = true;
			}
			else if (event.key.key == GLFW_KEY_S)
			{
				debug_camera.movements.backward = true;
			}
			else if (event.key.key == GLFW_KEY_A)
			{
				debug_camera.movements.left = true;
			}
			else if (event.key.key == GLFW_KEY_D)
			{
				debug_camera.movements.right = true;
			}
			else if (event.key.key == GLFW_KEY_Q)
			{
				debug_camera.movements.up = true;
			}
			else if (event.key.key == GLFW_KEY_E)
			{
				debug_camera.movements.down = true;
			}
			// camera postures
			else if (event.key.key == GLFW_KEY_UP)
			{
				debug_camera.postures.pitch_up = true;
			}
			else if (event.key.key == GLFW_KEY_DOWN)
			{
				debug_camera.postures.pitch_down = true;
			}
			else if (event.key.key == GLFW_KEY_LEFT)
			{
				debug_camera.postures.yaw_left = true;
			}
			else if (event.key.key == GLFW_KEY_RIGHT)
			{
				debug_camera.postures.yaw_right = true;
			}
			// camera sensitivity
			else if (event.key.key == GLFW_KEY_LEFT_BRACKET)
			{
				debug_camera.sensitivity.sensitivity_decrease = true;
			}
			else if (event.key.key == GLFW_KEY_RIGHT_BRACKET)
			{
				debug_camera.sensitivity.sensitivity_increase = true;
			}
		}

		// NON-DEBUG mode, set debug_camera info the same as the main camera settings
		if (camera.current_camera_mode == Camera::Camera_Mode::SCENE || camera.current_camera_mode == Camera::Camera_Mode::USER)
		{
			if (event.key.key == GLFW_KEY_Z)
			{
				debug_camera.update_info_from_another_camera(camera);
				debug_camera.reset_camera_control_status();
			}
		}


	}
	else if (event.type == InputEvent::KeyUp)
	{
		/* cr. learned from CMU 15666 Computer Game Programming code base
			https://github.com/15-466/15-466-f24-base2/blob/b7584e87b2498e4491e6438770f4b4a8d593bbde/PlayMode.cpp#L70 */
		
		if (camera.current_camera_mode == Camera::USER)
		{
			// camera movement
			if (event.key.key == GLFW_KEY_W)
			{
				camera.movements.forward = false;
			}
			else if (event.key.key == GLFW_KEY_S)
			{
				camera.movements.backward = false;
			}
			else if (event.key.key == GLFW_KEY_A)
			{
				camera.movements.left = false;
			}
			else if (event.key.key == GLFW_KEY_D)
			{
				camera.movements.right = false;
			}
			else if (event.key.key == GLFW_KEY_Q)
			{
				camera.movements.up = false;
			}
			else if (event.key.key == GLFW_KEY_E)
			{
				camera.movements.down = false;
			}
			// camera postures
			else if (event.key.key == GLFW_KEY_UP)
			{
				camera.postures.pitch_up = false;
			}
			else if (event.key.key == GLFW_KEY_DOWN)
			{
				camera.postures.pitch_down = false;
			}
			else if (event.key.key == GLFW_KEY_LEFT)
			{
				camera.postures.yaw_left = false;
			}
			else if (event.key.key == GLFW_KEY_RIGHT)
			{
				camera.postures.yaw_right = false;
			}
			// camera sensitivity
			else if (event.key.key == GLFW_KEY_LEFT_BRACKET)
			{
				camera.sensitivity.sensitivity_decrease = false;
			}
			else if (event.key.key == GLFW_KEY_RIGHT_BRACKET)
			{
				camera.sensitivity.sensitivity_increase = false;
			}
		}

		if (camera.current_camera_mode == Camera::Camera_Mode::DEBUG)
		{
			// camera movements
			if (event.key.key == GLFW_KEY_W)
			{
				debug_camera.movements.forward = false;
			}
			else if (event.key.key == GLFW_KEY_S)
			{
				debug_camera.movements.backward = false;
			}
			else if (event.key.key == GLFW_KEY_A)
			{
				debug_camera.movements.left = false;
			}
			else if (event.key.key == GLFW_KEY_D)
			{
				debug_camera.movements.right = false;
			}
			else if (event.key.key == GLFW_KEY_Q)
			{
				debug_camera.movements.up = false;
			}
			else if (event.key.key == GLFW_KEY_E)
			{
				debug_camera.movements.down = false;
			}
			// camera postures
			else if (event.key.key == GLFW_KEY_UP)
			{
				debug_camera.postures.pitch_up = false;
			}
			else if (event.key.key == GLFW_KEY_DOWN)
			{
				debug_camera.postures.pitch_down = false;
			}
			else if (event.key.key == GLFW_KEY_LEFT)
			{
				debug_camera.postures.yaw_left = false;
			}
			else if (event.key.key == GLFW_KEY_RIGHT)
			{
				debug_camera.postures.yaw_right = false;
			}
			// camera sensitivity
			else if (event.key.key == GLFW_KEY_LEFT_BRACKET)
			{
				debug_camera.sensitivity.sensitivity_decrease = false;
			}
			else if (event.key.key == GLFW_KEY_RIGHT_BRACKET)
			{
				debug_camera.sensitivity.sensitivity_increase = false;
			}
		}
	}
	else if (event.type == InputEvent::MouseMotion)
	{
		if (camera.current_camera_mode == Camera::USER)
		{
			// [TODO]
		}
	}
}

// Constructor modules functions Impl =========================================================================================================

void Wanderer::init_depth_format()
{
	// select the depth format ==================================================================
	depth_format = rtg.helpers.find_image_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32}, // depth format on current GPU; at least 1 is supported; the former is preferred
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	std::cout << "[Wanderer] (Depth Format) " << string_VkFormat(depth_format) << std::endl;
}

void Wanderer::create_render_pass()
{
	// set attachments info =====================================================================
	//	(1: color image, 2: depth image)
	std::array<VkAttachmentDescription, 2> attachments{
		VkAttachmentDescription{
			// specify the initial and final layout states for an image used in the subpass
			.format = rtg.surface_format.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,	 // clear the screen
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // keep result for later display
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // only used for image display
		},
		VkAttachmentDescription{
			.format = depth_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

	// set subpass info =========================================================================
	VkAttachmentReference color_attachment_ref{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkAttachmentReference depth_attachment_ref{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
		.pDepthStencilAttachment = &depth_attachment_ref};

	// set dependencies info ====================================================================
	//	(this defers the image load actions for the attachments; is a happens-before guarantee for the render pass)
	std::array<VkSubpassDependency, 2> dependencies{
		VkSubpassDependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL, // where the dependency is
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dependency src to wait
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // target dst to do
			.srcAccessMask = 0,											   // dependency src resource
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		   // target dst resource
		},
		VkSubpassDependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // finish the last point in the ppl that touches the depth buffer
			.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		}};

	// wrap-up create info ======================================================================
	VkRenderPassCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = uint32_t(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = uint32_t(dependencies.size()),
		.pDependencies = dependencies.data()};

	// create render pass =======================================================================
	VK(vkCreateRenderPass(rtg.device, &create_info, nullptr, &render_pass));
}

void Wanderer::create_command_pool()
{
	// wrap-up create info ======================================================================
	VkCommandPoolCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = rtg.graphics_queue_family.value()};

	// create command pool ======================================================================
	VK(vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool));
}

void Wanderer::create_pipelines()
{
	// create pipelines ========================================================================
	//  1: background, 2: lines, 3: objects (impl in Source/Pipelines/Wanderer/*)
	background_pipeline.create(rtg, render_pass, 0);
	lines_pipeline.create(rtg, render_pass, 0);
	objects_pipeline.create(rtg, render_pass, 0);
}

void Wanderer::create_description_pool()
{
	uint32_t per_workspace = uint32_t(rtg.workspaces.size()); // for easier-to-read counting

	std::array<VkDescriptorPoolSize, 2> pool_sizes{
		VkDescriptorPoolSize{
			// for camera
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 2 * per_workspace // 1 descriptor per set, 2 set per workspace
		},
		VkDescriptorPoolSize{
			// for transform
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1 * per_workspace // 1 descriptor per set, 1 set per workspace
		},
	};

	VkDescriptorPoolCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,					  // because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, can't free individual descriptors allocated from this pool
		.maxSets = 3 * per_workspace, // one set per workspace
		.poolSizeCount = uint32_t(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data()};

	// create description pool ==================================================================
	VK(vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool));
}

void Wanderer::setup_workspaces()
{
	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces)
	{
		// allocate command buffer ==============================================================
		{
			VkCommandBufferAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = command_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1};
			VK(vkAllocateCommandBuffers(rtg.device, &alloc_info, &workspace.command_buffer));
		}

		// allocate Camera and World descriptor sets =============================================
		{
			// lines_pipeline.set0_Camera --------------------------------------------------------

			// create buffers for sources
			workspace.Camera_src = rtg.helpers.create_buffer(
				sizeof(LinesPipeline::Camera),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host visible memory, coherent (no special sync needed)
				Helpers::Mapped																// put it somewhere in the CPU address space
			);

			// create buffers for destinations
			workspace.Camera = rtg.helpers.create_buffer(
				sizeof(LinesPipeline::Camera),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a uniform buffer, and a target of a memory copy
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,								   // on GPU, not host visible
				Helpers::Unmapped);

			// allocate descriptor set
			VkDescriptorSetAllocateInfo camera_set_alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &lines_pipeline.set0_Camera};

			VK(vkAllocateDescriptorSets(rtg.device, &camera_set_alloc_info, &workspace.Camera_descriptors));

			// objects_pipeline.set0_World --------------------------------------------------------

			// create buffers for sources
			workspace.World_src = rtg.helpers.create_buffer(
				sizeof(ObjectsPipeline::World),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host visible memory, coherent (no special sync needed)
				Helpers::Mapped																// put it somewhere in the CPU address space
			);

			// create buffers for destinations
			workspace.World = rtg.helpers.create_buffer(
				sizeof(ObjectsPipeline::World),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a uniform buffer, and a target of a memory copy
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,								   // on GPU, not host visible
				Helpers::Unmapped);

			// allocate descriptor set
			VkDescriptorSetAllocateInfo world_set_alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set0_World};

			VK(vkAllocateDescriptorSets(rtg.device, &world_set_alloc_info, &workspace.World_descriptors));
		};

		// bind Camera and World descriptor set to buffer =======================================
		{
			VkDescriptorBufferInfo Camera_info{
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size};

			VkDescriptorBufferInfo World_info{
				.buffer = workspace.World.handle,
				.offset = 0,
				.range = workspace.World.size};

			std::array<VkWriteDescriptorSet, 2> writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Camera_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &Camera_info},

				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.World_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &World_info}};

			vkUpdateDescriptorSets(
				rtg.device,
				uint32_t(writes.size()), // descriptor write count
				writes.data(),			 // descriptor writes
				0,						 // descriptor copy count
				nullptr					 // descriptor copies
			);
		};

		// allocate descriptor sets for set1 descriptor ==========================================
		{
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set1_Transforms};

			VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Transform_descriptors));
		};

		// bind Transform descriptor set to buffer is done in the render loop
	}
}

void Wanderer::load_lines_vertices()
{
	const float boat_amplification = 5.f;
	// const float sea_depression = 4.f;
	// const float sea_downward = 3.0f;

	lines_vertices.clear();

	std::vector<LinesPipeline::Vertex> mesh_vertices;

	// line 0: boat from .obj file ---------------------------------------------------------------

	LoadMgr::load_line_from_OBJ("Assets/Objects/boat.obj", mesh_vertices); // boat model from https://www.thebasemesh.com/asset/boat-ornament

	for (auto &v : mesh_vertices)
	{
		v.Position.x *= boat_amplification;
		v.Position.y *= boat_amplification;
		v.Position.z *= boat_amplification;
		lines_vertices.push_back(v);
	}

	// line 1: sea from .obj file ----------------------------------------------------------------

	// LoadMgr::load_line_from_OBJ("Assets/Objects/pool.obj", mesh_vertices); // ocean model from https://www.cgtrader.com/3d-model/pool-art

	// for (auto &v : mesh_vertices) {
	// 	v.Position.x /= sea_depression;
	// 	v.Position.y /= sea_depression;
	// 	v.Position.z /= sea_depression;
	// 	v.Position.y -= sea_downward;
	// 	lines_vertices.push_back(v);
	// }

	// create buffer for line vertices is done in the render loop --------------------------------
}

void Wanderer::load_objects_vertices()
{
	const float boat_amplification = 5.f;
	const float sea_depression = 4.f;
	const float sea_downward = 3.0f;

	std::vector<ObjectsPipeline::Vertex> tmp_object_vertices;

	// object 0: Boat from .obj file -----------------------------------------------------------

	boat_vertices.first = uint32_t(tmp_object_vertices.size());

	std::vector<ObjectsPipeline::Vertex> mesh_vertices;
	LoadMgr::load_object_from_OBJ("Assets/Objects/boat.obj", mesh_vertices);

	for (auto &v : mesh_vertices)
	{
		v.Position.x *= boat_amplification;
		v.Position.y *= boat_amplification;
		v.Position.z *= boat_amplification;
		tmp_object_vertices.push_back(v);
	}

	boat_vertices.count = uint32_t(tmp_object_vertices.size()) - boat_vertices.first;

	// object 1: sea from .obj file ------------------------------------------------------------

	sea_vertices.first = uint32_t(tmp_object_vertices.size());

	mesh_vertices.clear();
	LoadMgr::load_object_from_OBJ("Assets/Objects/pool.obj", mesh_vertices);

	for (auto &v : mesh_vertices)
	{
		v.Position.x /= sea_depression;
		v.Position.y /= sea_depression;
		v.Position.z /= sea_depression;
		v.Position.y -= sea_downward;
		tmp_object_vertices.push_back(v);
	}

	sea_vertices.count = uint32_t(tmp_object_vertices.size()) - sea_vertices.first;

	// create buffer for object vertices -------------------------------------------------------

	size_t bytes = tmp_object_vertices.size() * sizeof(tmp_object_vertices[0]);

	object_vertices = rtg.helpers.create_buffer(
		bytes,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a vertex buffer, and a target of a memory copy
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped);

	// copy data to buffer ----------------------------------------------------------------------

	rtg.helpers.transfer_to_buffer(tmp_object_vertices.data(), bytes, object_vertices);
}

void Wanderer::load_scene_objects_vertices()
{
	// traverse all scene objects, starting from the roots

	SceneMgr &sceneMgr = rtg.configuration.sceneMgr;
	typedef SceneMgr::NodeObject NodeObject;

	if (sceneMgr.sceneObject == nullptr)
		return;

	std::queue<NodeObject *> nodeQueue;
	for (std::string &nodeName : sceneMgr.sceneObject->rootName)
	{
		auto findNodeResult = sceneMgr.nodeObjectMap.find(nodeName);
		if (findNodeResult == sceneMgr.nodeObjectMap.end())
			continue;

		nodeQueue.push(findNodeResult->second);
	}

	std::vector<ObjectsPipeline::Vertex> tmp_object_vertices;
	tmp_object_vertices.reserve(sceneMgr.sceneObject->rootName.size() + sceneMgr.nodeObjectMap.size());

	while (!nodeQueue.empty())
	{
		// get the top node
		NodeObject *node = nodeQueue.front();
		nodeQueue.pop();

		// std::cout << node->name << std::endl; // [PASS]

		// load vertices of node if not exist (based on attributes of reference mesh object)
		if (sceneMgr.meshVerticesIndexMap.find(node->refMeshName) == sceneMgr.meshVerticesIndexMap.end())
		{
			auto findMeshResult = sceneMgr.meshObjectMap.find(node->refMeshName);
			if (findMeshResult != sceneMgr.meshObjectMap.end())
			{
				load_mesh_object_vertices(findMeshResult->second, tmp_object_vertices);

				// std::cout << "Mesh vertices " << sceneMgr.meshVerticesIndexMap.find(node->refMeshName)->first << " newly built." << std::endl; // [PASS]
			}
		}
		// else
		// {
		// 	std::cout << "Mesh vertices " << sceneMgr.meshVerticesIndexMap.find(node->refMeshName)->first << " already built." << std::endl; // [PASS]
		// }

		// push children to queue
		for (std::string &nodeName : node->childName)
		{
			auto findNodeResult = sceneMgr.nodeObjectMap.find(nodeName);
			if (findNodeResult == sceneMgr.nodeObjectMap.end())
				continue;

			nodeQueue.push(findNodeResult->second);
		}
	}

	// transfer attributes data to buffer
	size_t bytes = tmp_object_vertices.size() * sizeof(tmp_object_vertices[0]);

	object_vertices = rtg.helpers.create_buffer(
		bytes,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // use as a vertex buffer, and a target of a memory copy
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped);

	// copy data to buffer ----------------------------------------------------------------------

	rtg.helpers.transfer_to_buffer(tmp_object_vertices.data(), bytes, object_vertices);
}

void Wanderer::create_environment_cubemap(char **cubemap_data, const uint32_t &face_w, const uint32_t &face_h, const int&bytes_per_pixel)
{

	/* cr. Cube map tutorial by satellitnorden
		https://satellitnorden.wordpress.com/2018/01/23/vulkan-adventures-cube-map-tutorial/ */

	const VkDeviceSize layer_size = face_w * face_h * bytes_per_pixel;	
	const VkDeviceSize image_size = layer_size * NUM_CUBE_FACES;

	this->env_cubemap_buffer =  rtg.helpers.create_buffer(
		image_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host visible memory, coherent (no special sync needed)
		Helpers::Mapped																// put it somewhere in the CPU address space
	);

	/* cr. Rendering a Skybox with a Vulkan Cubemap by beaumanvienna
		https://www.youtube.com/watch?v=G2X3Exgi3co */

	uint64_t mem_address = reinterpret_cast<uint64_t>(this->env_cubemap_buffer.allocation.data());
	for (uint32_t i = 0; i < NUM_CUBE_FACES; ++ i) {
		std::memcpy(reinterpret_cast<void*>(mem_address), cubemap_data[i], layer_size);
		mem_address += layer_size;
	}

	// create image
	this->env_cubemap = rtg.helpers.create_cubemap_image(
		VkExtent2D{.width = static_cast<unsigned int>(face_w), .height = static_cast<unsigned int>(face_h)},
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // with sample and upload
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device local
		Helpers::Unmapped);
	
	rtg.helpers.transition_image_layout(env_cubemap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NUM_CUBE_FACES);

	// copy texture buffer to image
	rtg.helpers.copy_buffer_to_image(env_cubemap_buffer, env_cubemap, face_w, face_h, NUM_CUBE_FACES);

	rtg.helpers.transition_image_layout(env_cubemap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, NUM_CUBE_FACES);

	// create the sampler for the texture
	VkSamplerCreateInfo sampler_create_info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.flags = 0,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 0.f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.f,
		.maxLod = 0.f,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE};

	VK(vkCreateSampler(rtg.device, &sampler_create_info, nullptr, &env_cubemap_sampler));

	// create image view, which is the abstract of images
	VkImageViewCreateInfo image_view_create_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = env_cubemap.handle,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = NUM_CUBE_FACES},
	};

	VK(vkCreateImageView(rtg.device, &image_view_create_info, nullptr, &env_cubemap_view));
}


void Wanderer::load_mesh_object_vertices(SceneMgr::MeshObject *meshObject, std::vector<ObjectsPipeline::Vertex> &tmp_object_vertices)
{
	SceneMgr &sceneMgr = rtg.configuration.sceneMgr;
	
	auto findMeshResult = sceneMgr.meshObjectMap.find(meshObject->name);
	if (findMeshResult == sceneMgr.meshObjectMap.end())
		return;

	SceneMgr::MeshObject *refMesh = findMeshResult->second;

	// format check
	if (refMesh->attrPosition.format != VK_FORMAT_R32G32B32_SFLOAT || refMesh->attrNormal.format != VK_FORMAT_R32G32B32_SFLOAT || refMesh->attrTangent.format != VK_FORMAT_R32G32B32A32_SFLOAT || refMesh->attrTexcoord.format != VK_FORMAT_R32G32_SFLOAT)
	{
		std::cerr << "[load_mesh_object_vertices] Mesh name '" << meshObject->name << "' attribute format invalid.";
		return;
	}

	// load each attribute to each attribute list
	const std::string srcFolder = rtg.configuration.scene_graph_parent_folder;
	LoadMgr::read_s72_mesh_attribute_to_list(refMesh->positionList, refMesh->attrPosition, srcFolder);
	LoadMgr::read_s72_mesh_attribute_to_list(refMesh->normalList, refMesh->attrNormal, srcFolder);
	LoadMgr::read_s72_mesh_attribute_to_list(refMesh->tangentList, refMesh->attrTangent, srcFolder);
	LoadMgr::read_s72_mesh_attribute_to_list(refMesh->texcoordList, refMesh->attrTexcoord, srcFolder);
	assert(refMesh->positionList.size() == refMesh->normalList.size() && refMesh->normalList.size() == refMesh->tangentList.size() && refMesh->tangentList.size() == refMesh->texcoordList.size());


	// TODO: delete
	// if (meshObject->name == "Rounded-Cube")
	// {
	// 	for (auto &texcoord : refMesh->texcoordList)
	// 	{
	// 		std::cout << "[Texcoord] " << texcoord.x << ", " << texcoord.y << std::endl;
	// 	}
	// }


	ObjectVertices mesh_vertices;
	mesh_vertices.first = uint32_t(tmp_object_vertices.size());

	uint32_t vertexCount = refMesh->positionList.size();

	// calculate BBox for the mesh object

	for (uint32_t i = 0; i < vertexCount; ++ i)
	{
		refMesh->bbox.enclose(refMesh->positionList[i]);
	}

	// assembly attributes into scene object vertex

	for (uint32_t i = 0; i < vertexCount; ++i)
	{
		ObjectsPipeline::Vertex node_vertex;
		node_vertex.Position.x = refMesh->positionList[i].x;
		node_vertex.Position.y = refMesh->positionList[i].y;
		node_vertex.Position.z = refMesh->positionList[i].z;

		node_vertex.Normal.x = refMesh->normalList[i].x;
		node_vertex.Normal.y = refMesh->normalList[i].y;
		node_vertex.Normal.z = refMesh->normalList[i].z;

		node_vertex.Tangent.x = refMesh->tangentList[i].x;
		node_vertex.Tangent.y = refMesh->tangentList[i].y;
		node_vertex.Tangent.z = refMesh->tangentList[i].z;
		node_vertex.Tangent.w = refMesh->tangentList[i].w;

		node_vertex.TexCoord.s = refMesh->texcoordList[i].x;
		node_vertex.TexCoord.t = refMesh->texcoordList[i].y;

		tmp_object_vertices.push_back(node_vertex);
	}

	mesh_vertices.count = uint32_t(tmp_object_vertices.size()) - mesh_vertices.first;

	sceneMgr.meshVerticesIndexMap[meshObject->name] = scene_nodes_vertices.size();
	scene_nodes_vertices.push_back(mesh_vertices);
}

void Wanderer::setup_environment_cubemap(bool flip)
{
	/* cr. Rendering a Skybox with a Vulkan Cubemap by beaumanvienna
		https://www.youtube.com/watch?v=G2X3Exgi3co */

	/*
		load cubemap data from file
	*/

	const int desired_channels = 4;
	int w, h, org_channels;
	char *texture_data[6]; // NUM_CUBE_FACES
	std::string environment_map_src = rtg.configuration.scene_graph_parent_folder + rtg.configuration.sceneMgr.environmentObject->radiance.src;

	LoadMgr::load_cubemap_from_file(texture_data, environment_map_src.c_str(), w, h, org_channels, desired_channels, NUM_CUBE_FACES, flip);
	
	/*
		store cubemap on GPU
	*/

    unsigned int face_w = static_cast<unsigned int>(w);
    unsigned int face_h = static_cast<unsigned int>(h/6);
	const int bytes_per_pixel = desired_channels * sizeof(float);
	create_environment_cubemap(texture_data, face_w, face_h, bytes_per_pixel);

	// rtg.helpers.transition_image_layout(env_cubemap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NUM_CUBE_FACES);

	// // copy texture buffer to image
	// rtg.helpers.copy_buffer_to_image(env_cubemap_buffer, env_cubemap, face_w, face_h, NUM_CUBE_FACES);

	// rtg.helpers.transition_image_layout(env_cubemap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, NUM_CUBE_FACES);

	// // create the sampler for the texture
	// VkSamplerCreateInfo sampler_create_info{
	// 	.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
	// 	.flags = 0,
	// 	.magFilter = VK_FILTER_NEAREST,
	// 	.minFilter = VK_FILTER_NEAREST,
	// 	.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
	// 	.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.mipLodBias = 0.f,
	// 	.anisotropyEnable = VK_FALSE,
	// 	.maxAnisotropy = 0.f,
	// 	.compareEnable = VK_FALSE,
	// 	.compareOp = VK_COMPARE_OP_ALWAYS,
	// 	.minLod = 0.f,
	// 	.maxLod = 0.f,
	// 	.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
	// 	.unnormalizedCoordinates = VK_FALSE};

	// VK(vkCreateSampler(rtg.device, &sampler_create_info, nullptr, &env_cubemap_sampler));

	// // create image view, which is the abstract of images
	// VkImageViewCreateInfo image_view_create_info{
	// 	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	// 	.image = env_cubemap.handle,
	// 	.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
	// 	.format = VK_FORMAT_R8G8B8A8_UNORM,
	// 	.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
	// 	.subresourceRange{
	// 		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	// 		.baseMipLevel = 0,
	// 		.levelCount = 1,
	// 		.baseArrayLayer = 0,
	// 		.layerCount = NUM_CUBE_FACES},
	// };

	// VK(vkCreateImageView(rtg.device, &image_view_create_info, nullptr, &env_cubemap_view));
	

	// clean
	for (uint8_t i = 0; i < NUM_CUBE_FACES; ++ i)
	{
		delete texture_data[i];
	}
}


void Wanderer::create_diy_textures()
{
	SceneMgr &sceneMgr = rtg.configuration.sceneMgr;

	textures.reserve(sceneMgr.materialObjectMap.size() + 1);

	// create texture 0: checkerboard with a red square at the origin ============================

	// actually make the texture:
	uint32_t size = 128;
	std::vector<uint32_t> data;
	data.reserve(size * size);

	for (uint32_t y = 0; y < size; ++y)
	{
		float fy = (y + 0.5f) / float(size);
		for (uint32_t x = 0; x < size; ++x)
		{
			float fx = (x + 0.5f) / float(size);
			// highlight the origin
			if (fx < 0.05f && fy < 0.05f)
				data.emplace_back(0xff0000ff); // red
			else if ((fx < 0.5f) == (fy < 0.5f))
				data.emplace_back(0xff444444); // dark grey
			else
				data.emplace_back(0xffbbbbbb); // light grey
		}
	}
	assert(data.size() == size * size);

	// make a place for texture to live on the GPU
	textures.emplace_back(rtg.helpers.create_image(
		VkExtent2D{.width = size, .height = size}, // size pf image
		VK_FORMAT_R8G8B8A8_UNORM,				   // interpret image using SRGB-encoded 8-bit RGBA
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,						  // should be device-local
		Helpers::Unmapped));

	// transfer data
	rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures.back());


	// TODO: delete the following code

	// // create texture 1: 'xor' texture ============================================================

	// // actually make the texture:
	// size = 256;
	// data.clear();
	// data.reserve(size * size);

	// for (uint32_t y = 0; y < size; ++y)
	// {
	// 	for (uint32_t x = 0; x < size; ++x)
	// 	{
	// 		uint8_t r = uint8_t(x) ^ uint8_t(y);
	// 		uint8_t g = uint8_t(x + 128) ^ uint8_t(y);
	// 		uint8_t b = uint8_t(x) ^ uint8_t(y + 27);
	// 		uint8_t a = 0xff;
	// 		data.emplace_back(uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24));
	// 	}
	// }
	// assert(data.size() == size * size);

	// // make a place for texture to live on the GPU
	// textures.emplace_back(rtg.helpers.create_image(
	// 	VkExtent2D{.width = size, .height = size}, // size of image
	// 	VK_FORMAT_R8G8B8A8_SRGB,				   // interpret image using SRGB-encoded 8-bit RGBA
	// 	VK_IMAGE_TILING_OPTIMAL,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
	// 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,						  // should be device-local
	// 	Helpers::Unmapped));

	// // transfer data:
	// rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures.back());

	// // texture 2: for object sea =================================================================

	// // actually make the texture:
	// size = 256;
	// data.clear();
	// data.reserve(size * size);

	// for (uint32_t y = 0; y < size; ++y)
	// {
	// 	for (uint32_t x = 0; x < size; ++x)
	// 	{
	// 		uint32_t blue = floor(std::cos(x / 100.0) * size);
	// 		data.emplace_back((0x50 << 24) | (blue << 16) | (0x10 << 8) | (0x00));
	// 	}
	// }
	// assert(data.size() == size * size);

	// // make a place for texture to live on the GPU
	// textures.emplace_back(rtg.helpers.create_image(
	// 	VkExtent2D{.width = size, .height = size}, // size of image
	// 	VK_FORMAT_R8G8B8A8_UNORM,				   // interpret image using SRGB-encoded 8-bit RGBA
	// 	VK_IMAGE_TILING_OPTIMAL,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
	// 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,						  // should be device-local
	// 	Helpers::Unmapped));

	// // transfer data:
	// rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures.back());
}

void Wanderer::create_textures_descriptor()
{
	// make image views for the textures ========================================================

	texture_views.reserve(textures.size());
	for (Helpers::AllocatedImage const &image : textures)
	{
		VkImageViewCreateInfo image_view_create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.flags = 0,
			.image = image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = image.format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1},
		};

		VkImageView image_view = VK_NULL_HANDLE;
		VK(vkCreateImageView(rtg.device, &image_view_create_info, nullptr, &image_view));

		texture_views.emplace_back(image_view);
	}
	assert(texture_views.size() == textures.size());

	// make a sampler for the textures ==========================================================

	VkSamplerCreateInfo sampler_create_info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.flags = 0,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT, // how to handle texture coordinates outside of [0, 1]
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 0.0f, // doesn't matter if anisotropy is disabled
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS, // doesn't matter if compare is disabled
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE};

	VK(vkCreateSampler(rtg.device, &sampler_create_info, nullptr, &texture_sampler));

	// create the texture descriptor pool =======================================================

	uint32_t per_texture = uint32_t(textures.size()); // for easier-to-read counting

	std::array<VkDescriptorPoolSize, 1> pool_sizes{
		VkDescriptorPoolSize{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1 * 1 * per_texture, // 1 descriptor per set, 1 set per texture
		}};

	VkDescriptorPoolCreateInfo desc_pool_create_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,					// because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, can't free individual descriptors allocated from this pool
		.maxSets = 1 * per_texture, // one set per texture
		.poolSizeCount = uint32_t(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data()};

	VK(vkCreateDescriptorPool(rtg.device, &desc_pool_create_info, nullptr, &texture_descriptor_pool));

	// allocate and write the texture descriptor sets ============================================

	// allocate descriptor sets (different sets are using the same alloc_info) -------------------
	VkDescriptorSetAllocateInfo desc_set_alloc_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = texture_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &objects_pipeline.set2_TEXTURE};
	texture_descriptors.assign(textures.size(), VK_NULL_HANDLE);

	for (VkDescriptorSet &descriptor_set : texture_descriptors)
	{
		VK(vkAllocateDescriptorSets(rtg.device, &desc_set_alloc_info, &descriptor_set));
	}

	// write descriptors for textures -----------------------------------------------------------
	std::vector<VkDescriptorImageInfo> infos(textures.size());
	std::vector<VkWriteDescriptorSet> writes(textures.size());

	for (Helpers::AllocatedImage const &image : textures)
	{
		size_t i = &image - &textures[0];

		infos[i] = VkDescriptorImageInfo{
			.sampler = texture_sampler,
			.imageView = texture_views[i],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		writes[i] = VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = texture_descriptors[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &infos[i],
		};
	}

	vkUpdateDescriptorSets(rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr);
}

void Wanderer::construct_scene_graph_vertices_with_culling(std::vector<ObjectInstance> &object_instances, SceneMgr &sceneMgr, const mat4 &CLIP_FROM_WORLD)
{
	typedef SceneMgr::NodeObject NodeObject;

	if (sceneMgr.sceneObject == nullptr)
		return;

	std::queue<NodeObject *> nodeQueue;
	for (std::string &nodeName : sceneMgr.sceneObject->rootName)
	{
		auto findNodeResult = sceneMgr.nodeObjectMap.find(nodeName);
		if (findNodeResult == sceneMgr.nodeObjectMap.end())
			continue;

		nodeQueue.push(findNodeResult->second);
	}

	while (!nodeQueue.empty())
	{
		// get the top node
		NodeObject *node = nodeQueue.front();
		nodeQueue.pop();

		// std::cout << "Constructing node instance:" << node->name << std::endl; // [PASS]

		// construct node instance
		auto findMatrixResult = sceneMgr.nodeMatrixMap.find(node->name);
		auto findVertexIdxResult = sceneMgr.meshVerticesIndexMap.find(node->refMeshName);
		if (findMatrixResult != sceneMgr.nodeMatrixMap.end() && findVertexIdxResult != sceneMgr.meshVerticesIndexMap.end())
		{
			glm::mat4 WORLD_FROM_LOCAL_GLM = findMatrixResult->second;
			mat4 WORLD_FROM_LOCAL = TypeHelper::convert_glm_mat4_to_mat4(WORLD_FROM_LOCAL_GLM);
			mat4 WORLD_FROM_LOCAL_NORMAL = calculate_normal_matrix(findMatrixResult->second);
			
			auto nodeMeshIt = sceneMgr.meshObjectMap.find(node->refMeshName);
			if (nodeMeshIt == sceneMgr.meshObjectMap.end())
				continue;

			SceneMgr::MeshObject *refMesh = nodeMeshIt->second;
			

			// frustum culling
			if (rtg.configuration.culling_mode == RTG::Configuration::Culling_Mode::FRUSTUM)
			{
				
				// update node bbox 
				node->bbox.reset();

				// method 1: (by enclosing the corners of the transformed mesh bbox)
				std::vector<glm::vec3> meshBBoxCorners = refMesh->bbox.get_corners();
				for (auto & corner : meshBBoxCorners)
				{
					glm::vec4 corner_vec4 = glm::vec4(corner, 1);
					glm::vec4 transformed_corner = WORLD_FROM_LOCAL_GLM * corner_vec4;
					if (transformed_corner.w != 0.f)
						transformed_corner /= transformed_corner.w;
					corner = glm::vec3(transformed_corner[0], transformed_corner[1], transformed_corner[2]);
					node->bbox.enclose(corner);
				}

				// method 2: (by enclosing all exact transformed vertices)
				// for (auto & vertex : refMesh->positionList)
				// {
				// 	glm::vec4 vertex_vec4 = glm::vec4(vertex, 1);
				// 	glm::vec4 transformed_vertex_vec4 = WORLD_FROM_LOCAL_GLM * vertex_vec4;
				// 	if (transformed_vertex_vec4.w != 0.f)
				// 		transformed_vertex_vec4 /= transformed_vertex_vec4.w;
				// 	glm::vec3 transformed_vertex_vec3 = glm::vec3(transformed_vertex_vec4.x, transformed_vertex_vec4.y, transformed_vertex_vec4.z);
				// 	node->bbox.enclose(transformed_vertex_vec3);
				// }

				Frustum camera_frustum = Frustum::createFrustumFromCamera(rtg.configuration.camera); // always use the main camera for culling

				if (!camera_frustum.isBBoxInFrustum(node->bbox)) {
					// std::cout << "Culling node " << node->name << std::endl;
					continue;
				}
			}

			object_instances.emplace_back(ObjectInstance{
				.vertices = scene_nodes_vertices[findVertexIdxResult->second],
				.transform{
					.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL_NORMAL
					// NOTE: the upper left 3x3 of WORLD_FROM_LOCAL_NORMAL should be the inverse transpose of the upper left 3x3
				},
				.texture = 0,
			});
		}
		// else
		// {
		// 	if (findMatrixResult == sceneMgr.nodeMatrixMap.end())
		// 		std::cout << "[ERROR] " << node->name << " not founding matrix" << std::endl; // [PASS]
		// 	else
		// 		std::cout << "[ERROR] not founding mesh index count" << std::endl; // [PASS]
		// }

		// push children to queue
		for (std::string &nodeName : node->childName)
		{
			auto findNodeResult = sceneMgr.nodeObjectMap.find(nodeName);
			if (findNodeResult == sceneMgr.nodeObjectMap.end())
				continue;

			nodeQueue.push(findNodeResult->second);
		}
	}
}

mat4 Wanderer::calculate_normal_matrix(const glm::mat4 &worldFromLocal)
{
	glm::mat3 normalMatrix = glm::mat3(worldFromLocal);
	normalMatrix = glm::transpose(glm::inverse(normalMatrix));

	glm::mat4 worldFromLocalNormal{
		normalMatrix[0][0], normalMatrix[0][1], normalMatrix[0][2], 0.0f,
		normalMatrix[1][0], normalMatrix[1][1], normalMatrix[1][2], 0.0f,
		normalMatrix[2][0], normalMatrix[2][1], normalMatrix[2][2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f};

	return TypeHelper::convert_glm_mat4_to_mat4(worldFromLocalNormal);
}
