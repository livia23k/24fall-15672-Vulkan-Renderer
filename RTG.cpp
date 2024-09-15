#include "RTG.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <set>

void RTG::Configuration::parse(int argc, char **argv) {
	for (int argi = 1; argi < argc; ++argi) {
		std::string arg = argv[argi];
		if (arg == "--debug") {
			debug = true;
		} else if (arg == "--no-debug") {
			debug = false;
		} else if (arg == "--physical-device") {
			if (argi + 1 >= argc) throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		} else if (arg == "--drawing-size") {
			if (argi + 2 >= argc) throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what) {
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i) {
					if (val[i] < '0' || val[i] > '9') {
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		} else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	configuration = configuration_;

	//fill in flags/extensions/layers information:

	//create the `instance` (main handle to Vulkan library):
	refsol::RTG_constructor_create_instance(
		configuration.application_info,
		configuration.debug,
		&instance,
		&debug_messenger
	);

	//create the `window` and `surface` (where things get drawn):
	refsol::RTG_constructor_create_surface(
		configuration.application_info,
		configuration.debug,
		configuration.surface_extent,
		instance,
		&window,
		&surface
	);

	//select the `physical_device` -- the gpu that will be used to draw:
	refsol::RTG_constructor_select_physical_device(
		configuration.debug,
		configuration.physical_device_name,
		instance,
		&physical_device
	);

	//select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
	refsol::RTG_constructor_select_format_and_mode(
		configuration.debug,
		configuration.surface_formats,
		configuration.present_modes,
		physical_device,
		surface,
		&surface_format,
		&present_mode
	);

	//create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
	refsol::RTG_constructor_create_device(
		configuration.debug,
		physical_device,
		surface,
		&device,
		&graphics_queue_family,
		&graphics_queue,
		&present_queue_family,
		&present_queue
	);

	//create initial swapchain:
	recreate_swapchain();

	//create workspace resources:
	workspaces.resize(configuration.workspaces);
	for (auto &workspace : workspaces) {
		refsol::RTG_constructor_per_workspace(device, &workspace);
	}

	//run any resource creation required by Helpers structure:
	helpers.create();
}
RTG::~RTG() {
	//don't destroy until device is idle:
	if (device != VK_NULL_HANDLE) {
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	//destroy any resource destruction required by Helpers structure:
	helpers.destroy();

	//destroy workspace resources:
	for (auto &workspace : workspaces) {
		refsol::RTG_destructor_per_workspace(device, &workspace);
	}
	workspaces.clear();

	//destroy the swapchain:
	destroy_swapchain();

	//destroy the rest of the resources:
	refsol::RTG_destructor( &device, &surface, &window, &debug_messenger, &instance );

}


void RTG::recreate_swapchain() {

	// clean up swapchain if it already exists
	if (!swapchain_images.empty()) {
		destroy_swapchain();
	}

	// determine size, image count, and transform for swapchain
	VkSurfaceCapabilitiesKHR capabilities;
	VK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities) );

	swapchain_extent = capabilities.currentExtent;

	uint32_t requested_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		requested_count = std::min(capabilities.maxImageCount, requested_count);
	}

	// make the swapchain
	{
		VkSwapchainCreateInfoKHR create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = requested_count,
			.imageFormat = surface_format.format,
			.imageColorSpace = surface_format.colorSpace,
			.imageExtent = swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE //NOTE: could be more efficient by passing old swapchain handle here instead of destroying it
		};

		std::vector< uint32_t > queue_family_indices{
			graphics_queue_family.value(),
			present_queue_family.value()
		};
		
		//if images will be presented on a different queue, make sure they are shared:
		if (queue_family_indices[0] != queue_family_indices[1]) {
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // shared
			create_info.queueFamilyIndexCount = uint32_t(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
		} else {
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // not shared
		}
		
		VK( vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) );
	};

	// get the swapchain images
	{
		// 2-calls pattern: get the size first, then resize and get the data
		uint32_t count = 0;
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count , nullptr) );
		swapchain_images.resize(count);
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()) );
	};

	//make image views for swapchain images
	{
		swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < swapchain_images.size(); ++ i) {
			VkImageViewCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = swapchain_images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = surface_format.format,
				.components{
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
			};
			VK( vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[i]) );
		}
	}

	// report information
	if (configuration.debug) {
		std::cout << "Surface is now: transform: " << capabilities.currentTransform << "size: " \
			<< capabilities.currentExtent.width << "x" << capabilities.currentExtent.height << ".\n";
		std::cout << "Swapchain is now " << swapchain_images.size() \
			<< " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}


void RTG::destroy_swapchain() {
	// refsol::RTG_destroy_swapchain(
	// 	device,
	// 	&swapchain,
	// 	&swapchain_images,
	// 	&swapchain_image_views
	// );
	
	VK( vkDeviceWaitIdle(device) ); // wait for any rendering to old swapchain to finish

	// clean up image views referencing the swapchain
	for (auto &image_view : swapchain_image_views) {
		vkDestroyImageView(device, image_view, nullptr);
		image_view = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	// forget handles to swapchain images 
	// (swapchain image handlers are owned by the swapchain itself, 
	// will destroy by deallocating the swapchain itself)
	swapchain_images.clear();

	//deallocate the swapchain and (thus) its images:
	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

void RTG::run(Application &application) {
	refsol::RTG_run(*this, application);
}
