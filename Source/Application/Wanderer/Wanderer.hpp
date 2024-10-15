#pragma once

#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "Source/DataType/MeshAttribute.hpp"
#include "Source/DataType/Mat4.hpp"
#include "Source/DataType/Frustum.hpp"
#include "Source/Tools/Timer.hpp"
#include "Source/Configuration/RTG.hpp"


#include <chrono>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>

struct Wanderer : RTG::Application
{

	Wanderer(RTG &);
	Wanderer(Wanderer const &) = delete; // you shouldn't be copying this object
	~Wanderer();

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
		// helper info:
		bool has_env_cubemap;

		// descriptor set layouts:
		// VkDescriptorSetLayout set0_Camera = VK_NULL_HANDLE;
		VkDescriptorSetLayout set0_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout set2_TEXTURE = VK_NULL_HANDLE;
		VkDescriptorSetLayout set3_ENVIRONMENT = VK_NULL_HANDLE;

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
		static_assert(sizeof(Transform) == (16 * 4) * 3, "Transform is the expected size.");

		// push constants
		struct Push
		{
			SceneMgr::MaterialType material_type;
		};

		VkPipelineLayout layout = VK_NULL_HANDLE;

		using Vertex = MeshAttribute;

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
		Helpers::AllocatedBuffer Transforms_src; // host coherent; mapped to cpu memorys
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
	std::vector<ObjectVertices> scene_nodes_vertices;


	// general textures
	std::vector<Helpers::AllocatedImage> textures;			   // holds handles of actual image data
	std::vector<VkImageView> texture_views;					   // references to portions of of whole textures
	VkSampler texture_sampler = VK_NULL_HANDLE;				   // how to sample from textures (wrapping, interpolation, etc.)
	VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE; // pool from which texture descriptors are allocated
	std::vector<VkDescriptorSet> texture_descriptors;		   // descriptor for each texture, allocated from texture_descriptor_pool


	// cubemap related resource
	Helpers::AllocatedBuffer env_cubemap_buffer;
	Helpers::AllocatedImage env_cubemap;
	VkImageView env_cubemap_view;
	VkSampler env_cubemap_sampler = VK_NULL_HANDLE;
	VkDescriptorPool env_cubemap_descriptor_pool = VK_NULL_HANDLE;
	VkDescriptorSet env_cubemap_descriptor;

	
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
	Timer animation_timer;

	mat4 CLIP_FROM_WORLD;

	std::vector<LinesPipeline::Vertex> lines_vertices;

	ObjectsPipeline::World world;

	struct ObjectInstance
	{
		ObjectVertices vertices;
		ObjectsPipeline::Transform transform;
		uint32_t texture = 0;
		SceneMgr::MaterialType material_type;
	};
	std::vector<ObjectInstance> object_instances;

	//--------------------------------------------------------------------
	// Constructor modules functions, breaking up the constructor into smaller parts:

	void init_depth_format();
    void create_render_pass();
    void create_command_pool();
    void create_pipelines();
    void create_description_pool();
    void setup_workspaces();

	//--------------------------------------------------------------------
	// Load resources:

	// vertices
	void load_lines_vertices();
	void load_objects_vertices();
	void load_scene_objects_vertices();

	// textures
	void setup_environment_cubemap(bool flip);
	void create_environment_cubemap_descriptor();
	void create_diy_textures();
	void create_textures_descriptor();

	// object instances
	void construct_scene_graph_vertices_with_culling(std::vector<ObjectInstance> &object_instances, SceneMgr &sceneMgr, const mat4 &CLIP_FROM_WORLD);
	

	//--------------------------------------------------------------------
	// GPU resources related:

	// textures
	const uint32_t NUM_CUBE_FACES = 6;
	void create_environment_cubemap(char **cubemap_data, const uint32_t &face_w, const uint32_t &face_h, const int&bytes_per_pixel);


	//--------------------------------------------------------------------
	// Load resources Helper:

	// vertices helper
	void load_mesh_object_vertices(SceneMgr::MeshObject *meshObject, std::vector<ObjectsPipeline::Vertex> &tmp_object_vertices);

	// object instances helper
	mat4 calculate_normal_matrix(const glm::mat4 &worldFromLocal);


	//--------------------------------------------------------------------
	// Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
