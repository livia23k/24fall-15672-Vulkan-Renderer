#pragma once

#include "datastructures/PosColVertex.hpp"

#include "RTG.hpp"

struct Tutorial : RTG::Application {

	Tutorial(RTG &);
	Tutorial(Tutorial const &) = delete; //you shouldn't be copying this object
	~Tutorial();

	//kept for use in destructor:
	RTG &rtg;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat depth_format{};
	//Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;

	//Pipelines:

	//TODO
	//Edit Start =========================================================================================================
	struct BackgroundPipeline {
		//no descriptor set layouts //manages the resources needed by shader

		//push constants //uniform data that could be passed efficiently
		struct Push {
			float time;
		};

		VkPipelineLayout layout = VK_NULL_HANDLE; //arrangements of resources

		//no vertex bindings //how to read vertex data from buffers

		VkPipeline handle = VK_NULL_HANDLE; //the actual pipeline object used during drawing

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} background_pipeline;

	struct LinesPipeline {
		//no descriptor set layouts

		//push constants

		VkPipelineLayout layout = VK_NULL_HANDLE;

		using Vertex = PosColVertex;

		VkPipeline handle = VK_NULL_HANDLE;

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} lines_pipeline;
	//Edit End ===========================================================================================================

	//pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	
	//STEPX: Add descriptor pool here.

	//workspaces hold per-render resources:
	struct Workspace {
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

		//Edit Start =========================================================================================================
		//locations for lines data (streams to GPU per-frame):
		Helpers::AllocatedBuffer lines_vertices_src; //host coherent; mapped to cpu memory
		Helpers::AllocatedBuffer lines_vertices; //device-local
		//Edit End ===========================================================================================================

	};
	std::vector< Workspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

	Helpers::AllocatedImage swapchain_depth_image;
	VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
	std::vector< VkFramebuffer > swapchain_framebuffers;
	//used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
	void destroy_framebuffers();

	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	//Edit Start =========================================================================================================
	float time = 0.0f;

	std::vector< LinesPipeline::Vertex > lines_vertices;
	//Edit End ===========================================================================================================

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
