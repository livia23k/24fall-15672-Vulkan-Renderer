#pragma once

#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "Source/DataType/Mat4.hpp"

#include "Source/Configuration/RTG.hpp"

struct Tutorial : RTG::Application
{

	Tutorial(RTG &);
	Tutorial(Tutorial const &) = delete; // you shouldn't be copying this object
	~Tutorial();

	// kept for use in destructor:
	RTG &rtg;

	//--------------------------------------------------------------------
	// Resources that last the lifetime of the application:

	// chosen format for depth buffer:
	VkFormat depth_format{};
	// Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;

	// Pipelines:

	struct BackgroundPipeline
	{
		// no descriptor set layouts //manages the resources needed by shader

		// push constants //uniform data that could be passed efficiently
		struct Push
		{
			float time;
		};

		VkPipelineLayout layout = VK_NULL_HANDLE; // arrangements of resources

		// no vertex bindings //how to read vertex data from buffers

		VkPipeline handle = VK_NULL_HANDLE; // the actual pipeline object used during drawing

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} background_pipeline;

	struct LinesPipeline
	{
		// descriptor set layouts:
		VkDescriptorSetLayout set0_Camera = VK_NULL_HANDLE;

		// types for descriptors:
		struct Camera
		{
			mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(Camera) == 16 * 4, "camera buffer structure is packed.");

		// push constants (none)

		VkPipelineLayout layout = VK_NULL_HANDLE;

		using Vertex = PosColVertex;

		VkPipeline handle = VK_NULL_HANDLE;

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} lines_pipeline;

	struct ObjectsPipeline
	{
		// descriptot set layouts:
		// VkDescriptorSetLayout set0_Camera = VK_NULL_HANDLE;
		VkDescriptorSetLayout set0_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout set2_TEXTURE = VK_NULL_HANDLE;

		// types for descriptors:
		//  using Camera = LinesPipeline::Camera;

		struct World
		{
			struct
			{
				float x, y, z, padding_;
			} SKY_DIRECTION;
			struct
			{
				float r, g, b, padding_;
			} SKY_ENERGY;
			struct
			{
				float x, y, z, padding_;
			} SUN_DIRECTION;
			struct
			{
				float r, g, b, padding_;
			} SUN_ENERGY;
		};
		static_assert(sizeof(World) == 4 * 4 + 4 * 4 + 4 * 4 + 4 * 4, "World is the expected size.");

		struct Transform
		{
			mat4 CLIP_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL_NORMAL;
		};
		static_assert(sizeof(Transform) == 16 * 4 + 16 * 4 + 16 * 4, "Transform is the expected size.");

		// push constants (none)

		VkPipelineLayout layout = VK_NULL_HANDLE;

		using Vertex = PosNorTexVertex;

		VkPipeline handle = VK_NULL_HANDLE;

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} objects_pipeline;

	// pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	// STEPX: Add descriptor pool here.
	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

	// workspaces hold per-render resources:
	struct Workspace
	{
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; // from the command pool above; reset at the start of every render.

		// locations for lines data (streamed to GPU per-frame):
		Helpers::AllocatedBuffer lines_vertices_src; // host coherent; mapped to cpu memory
		Helpers::AllocatedBuffer lines_vertices;	 // device-local

		// locations for LinesPipeline::Camera data (streamed to GPU per-frame):
		Helpers::AllocatedBuffer Camera_src; // host coherent; mapped to cpu memory
		Helpers::AllocatedBuffer Camera;	 // device-local
		VkDescriptorSet Camera_descriptors;	 // references Camera

		// location for ObjectsPipeline::World data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer World_src; // host coherent; mapped
		Helpers::AllocatedBuffer World;		// device-local
		VkDescriptorSet World_descriptors;	// references World

		// locations for ObjectsPipeline::Transform data (streamed to GPU per-frame):
		Helpers::AllocatedBuffer Transforms_src; // host coherent; mapped to cpu memory
		Helpers::AllocatedBuffer Transforms;	 // device-local
		VkDescriptorSet Transform_descriptors;	 // references Transfroms
	};
	std::vector<Workspace> workspaces;

	//-------------------------------------------------------------------
	// static scene resources:

	Helpers::AllocatedBuffer object_vertices;

	struct ObjectVertices
	{
		uint32_t first = 0; // index of first vertex in object_vertices
		uint32_t count = 0; // number of vertices in object_vertices
	};
	ObjectVertices plane_vertices;
	ObjectVertices torus_vertices;
	ObjectVertices boat_vertices;
	ObjectVertices sea_vertices;

	std::vector<Helpers::AllocatedImage> textures;			   // holds handles of actual image data
	std::vector<VkImageView> texture_views;					   // references to portions of of whole textures
	VkSampler texture_sampler = VK_NULL_HANDLE;				   // how to sample from textures (wrapping, interpolation, etc.)
	VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE; // pool from which texture descriptors are allocated
	std::vector<VkDescriptorSet> texture_descriptors;		   // descriptor for each texture, allocated from texture_descriptor_pool
	//--------------------------------------------------------------------
	// Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

	Helpers::AllocatedImage swapchain_depth_image;
	VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> swapchain_framebuffers;
	// used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
	void destroy_framebuffers();

	//--------------------------------------------------------------------
	// Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;

	mat4 CLIP_FROM_WORLD;

	std::vector<LinesPipeline::Vertex> lines_vertices;

	ObjectsPipeline::World world;

	struct ObjectInstance
	{
		ObjectVertices vertices;
		ObjectsPipeline::Transform transform;
		uint32_t texture = 0;
	};
	std::vector<ObjectInstance> object_instances;

	//--------------------------------------------------------------------
	// Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
