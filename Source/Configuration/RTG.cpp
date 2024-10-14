#include "Source/Configuration/RTG.hpp"

#include "Source/Helper/VK.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>	 //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <set>
#include <filesystem>

void RTG::Configuration::parse(int argc, char **argv)
{
	for (int argi = 1; argi < argc; ++argi)
	{
		std::string arg = argv[argi];
		if (arg == "--debug")
		{
			debug = true;
		}
		else if (arg == "--no-debug")
		{
			debug = false;
		}
		else if (arg == "--physical-device")
		{
			if (argi + 1 >= argc)
				throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		}
		else if (arg == "--drawing-size")
		{
			if (argi + 2 >= argc)
				throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what)
			{
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i)
				{
					if (val[i] < '0' || val[i] > '9')
					{
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		}
		else if (arg == "--scene")
		{
			if (argi + 1 >= argc)
				throw std::runtime_error("--scene requires a parameter (a scene graph file name).");
			argi += 1;
			std::filesystem::path fs_path(argv[argi]);
			scene_graph_parent_folder = "Assets/SceneGraphs/" + fs_path.parent_path().string() + "/";
			scene_graph_path = "Assets/SceneGraphs/" + fs_path.string();
			// std::cout << "file path: " << scene_graph_parent_folder << ", " << scene_graph_path << std::endl; // [PASS]
		}
		else if (arg == "--camera")
		{
			if (argi + 1 >= argc)
				throw std::runtime_error("--camera requires a parameter (a camera name).");
			argi += 1;
			specified_default_camera = argv[argi];
		}
		else if (arg == "--culling")
		{
			if (argi + 1 >= argc)
				throw std::runtime_error("--culling requires a parameter (a culling mode name), valid mode: none, frustum.");
			argi += 1;

			std::string culling_mode_str = argv[argi];

			if (culling_mode_str == "none")
			{
				culling_mode = Culling_Mode::NONE;
			}
			else if (culling_mode_str == "frustum")
			{
				culling_mode = Culling_Mode::FRUSTUM;
			}
			else
			{
				throw std::runtime_error("--culling mode not valid. Current valid mode: none, frustum.");
			}
		}
		else if (arg == "--headless")
		{
			if (argi + 1 >= argc)
				throw std::runtime_error("--headless requires a parameter (a event file), and need to specify the --drawing-size (default 800x540).");
			argi += 1;
			is_headless = true;
			event_file_name = argv[argi];
		}
		else
		{
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}

	if (is_headless)
	{
		if (surface_extent.width == 0 && surface_extent.height == 0)
		{
			throw std::runtime_error("--headless requires a parameter (a event file), and need to specify the --drawing-size (default 800x540).");
		}
	}
}

void RTG::Configuration::usage(std::function<void(const char *, const char *)> const &callback)
{
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
	callback("--scene <name>", "Set the path of scene graph to render.");
	callback("--camera <name>", "Set the name of the scene camera.");
	callback("--culling <mode>", "Valid mode: none, frustum.");
	callback("--headless <events>", "Run headless renderer and read frame times and events from the events file.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void *user_data)
{
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		std::cerr << "\x1b[91m" << "E: "; // "E: " (in bright red color)
	}
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		std::cerr << "\x1b[33m" << "w: "; // "w: " (in yellow color)
	}
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		std::cerr << "\x1b[90m" << "i: "; // "i: " (in bright black)
	}
	else
	{									  // VERBOSE
		std::cerr << "\x1b[90m" << "v: "; // "v: " (in bright black)
	}
	std::cerr << data->pMessage << "\x1b[0m" << std::endl; // reset color and formattingto default

	return VK_FALSE;
}

RTG::RTG(Configuration const &configuration_) : helpers(*this)
{
	// prepare for performance logging
	render_performance_log.open("performance(render).txt", std::ios::out | std::ios::trunc); // trunc existing file
	if (!render_performance_log.is_open())
		std::cerr << "Failed to open performance log file." << std::endl;
	else
		render_performance_log.close();
	render_performance_log.open("performance(render).txt", std::ios::app); // re-open for appending

	// copy input configuration:
	configuration = configuration_;

	// fill in flags/extensions/layers information:

	// create the `instance` (main handle to Vulkan library):
	{
		VkInstanceCreateFlags instance_flags = 0;
		std::vector<const char *> instance_extensions;
		std::vector<const char *> instance_layers;

// add extensions for MoltenVK portability layer on macOS (only needed on macOS, to translate Vulkan to Metal)
#if defined(__APPLE__)
		instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		instance_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif

		// add extensions and layers for debugging
		if (configuration.debug)
		{
			instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		// add extensions needed by glfw
		{
			glfwInit();
			if (!glfwVulkanSupported())
			{
				throw std::runtime_error("GLFW reports Vulkan is not supported.");
			}

			uint32_t count;
			const char **extensions = glfwGetRequiredInstanceExtensions(&count);
			if (extensions == nullptr)
			{
				throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
			}
			for (uint32_t i = 0; i < count; ++i)
			{
				instance_extensions.emplace_back(extensions[i]);
			}
		};

		// write debug messenger structure
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debug_callback,
			.pUserData = nullptr};

		VkInstanceCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = (configuration.debug ? &debug_messenger_create_info : nullptr), // pass debug structure if configured
			.flags = instance_flags,
			.pApplicationInfo = &configuration.application_info,
			.enabledLayerCount = uint32_t(instance_layers.size()),
			.ppEnabledLayerNames = instance_layers.data(),
			.enabledExtensionCount = uint32_t(instance_extensions.size()),
			.ppEnabledExtensionNames = instance_extensions.data()};
		VK(vkCreateInstance(&create_info, nullptr, &instance));

		// create debug messenger
		if (configuration.debug)
		{
			PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (!vkCreateDebugUtilsMessengerEXT)
			{
				throw std::runtime_error("Failed to lookup debug utils create fn.");
			}
			VK(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger));
		}
	};

	// create the `window` and `surface` (where things get drawn):
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(configuration.surface_extent.width, configuration.surface_extent.height, configuration.application_info.pApplicationName, nullptr, nullptr);

		if (!window)
		{
			throw std::runtime_error("GLFW failed to create a window.");
		}

		VK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
	};

	// select the `physical_device` -- the gpu that will be used to draw:
	{
		// std::cout << "[RTG] (Candidate Physical Device): ";
		std::vector<std::string> physical_device_names; // for later error message
		// pick a physical device
		{
			uint32_t count = 0;
			VK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
			std::vector<VkPhysicalDevice> physical_devices(count);
			VK(vkEnumeratePhysicalDevices(instance, &count, physical_devices.data()));

			uint32_t best_score = 0;

			for (auto const &pd : physical_devices)
			{
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(pd, &properties);

				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(pd, &features);

				physical_device_names.emplace_back(properties.deviceName);
				// std::cout << properties.deviceName << ", ";

				if (!configuration.physical_device_name.empty())
				{
					if (configuration.physical_device_name == properties.deviceName)
					{
						if (physical_device)
						{
							std::cerr << "WARNING: have two physical devices with the name '" << properties.deviceName << "'; using the first to be enumerated." << std::endl;
						}
						else
						{
							physical_device = pd;
						}
					}
				}
				else
				{
					uint32_t score = 1;
					if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					{
						score += 0x8000;
					}

					if (score > best_score)
					{
						best_score = score;
						physical_device = pd;
					}
				}
			}
		};
		// std::cout << "[End]" << std::endl;

		if (physical_device == VK_NULL_HANDLE)
		{
			// report error
			std::cerr << "Physical devices:\n";
			for (std::string const &name : physical_device_names)
			{
				std::cerr << "    " << name << "\n";
			}
			std::cerr.flush();

			if (!configuration.physical_device_name.empty())
			{
				throw std::runtime_error("No physical device with name '" + configuration.physical_device_name + "'.");
			}
			else
			{
				throw std::runtime_error("No suitable GPU found.");
			}
		}

		{ // report device name:
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_device, &properties);
			std::cout << "Selected physical device '" << properties.deviceName << "'." << std::endl;
		}
	};

	// select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
	{
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;

		// get supported formats
		{
			uint32_t count = 0;
			VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr));
			formats.resize(count);
			VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data()));
		}

		// get supported present modes
		{
			uint32_t count = 0;
			VK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr));
			present_modes.resize(count);
			VK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, present_modes.data()));
		}

		// find first available surface format matching config:
		surface_format = [&]()
		{
			for (auto const &config_format : configuration.surface_formats)
			{
				for (auto const &format : formats)
				{
					if (config_format.format == format.format && config_format.colorSpace == format.colorSpace)
					{
						return format;
					}
				}
			}
			throw std::runtime_error("No format matching requested format(s) found.");
		}(); // invokes the lambda immediately after its definition

		// find first available present mode matching config:
		present_mode = [&]()
		{
			for (auto const &config_mode : configuration.present_modes)
			{
				for (auto const &mode : present_modes)
				{
					if (config_mode == mode)
					{
						return mode;
					}
				}
			}
			throw std::runtime_error("No present mode matching requested mode(s) found.");
		}(); // invokes the lambda immediately after its definition
	};

	// create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
	{
		// look up queue indices
		{
			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
			std::vector<VkQueueFamilyProperties> queue_families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families.data());

			for (auto const &queue_family : queue_families)
			{
				uint32_t i = uint32_t(&queue_family - &queue_families[0]);

				// if it does graphics, set the graphics queue family:
				if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					if (!graphics_queue_family)
						graphics_queue_family = i;
				}

				// if it has present support, set the present queue family:
				VkBool32 present_support = VK_FALSE;
				VK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support));
				if (present_support == VK_TRUE)
				{
					if (!present_queue_family)
						present_queue_family = i;
				}

				// print each queue family flag information
				if (configuration.debug)
				{
					std::cout << "[RTG] (Queue Family) Queue family " << i << " supports: ";
					if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
						if (configuration.debug)
							std::cout << " graphics, ";
					if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)
						if (configuration.debug)
							std::cout << " compute, ";
					if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT)
						if (configuration.debug)
							std::cout << " transfer, ";
					if (queue_family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
						if (configuration.debug)
							std::cout << " sparse binding, ";
					if (queue_family.queueFlags & VK_QUEUE_PROTECTED_BIT)
						if (configuration.debug)
							std::cout << " protected, ";
					if (queue_family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
						if (configuration.debug)
							std::cout << " video decode, ";
					if (queue_family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
						if (configuration.debug)
							std::cout << " video encode, ";
					std::cout << std::endl;
				}
			}

			if (!graphics_queue_family)
			{
				throw std::runtime_error("No queue with graphics support.");
			}

			if (!present_queue_family)
			{
				throw std::runtime_error("No queue with present support.");
			}
		}

		// select device extensions
		std::vector<const char *> device_extensions;
#if defined(__APPLE__)
		device_extensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME); // Add portability extension on macOS
#endif
		device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Add the swapchain extension

		// create the logical device
		{
			std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
			std::set<uint32_t> unique_queue_families{
				graphics_queue_family.value(),
				present_queue_family.value()};

			float queue_priorities[1] = {1.0f};
			for (uint32_t queue_family : unique_queue_families)
			{
				queue_create_infos.emplace_back(VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queue_family,
					.queueCount = 1,
					.pQueuePriorities = queue_priorities,
				});
			}

			VkPhysicalDeviceFeatures supported_features;
			vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

			if (!supported_features.wideLines)
			{
				device_features.wideLines = VK_FALSE;
				if (configuration.debug)
					std::cout << "[RTG] (Device Features) LineWidth: Not supported by device; disabled.\n";
			}
			else
			{
				device_features.wideLines = VK_TRUE;
				if (configuration.debug)
					std::cout << "[RTG] (Device Features) Supported by device; enabled.\n";
			}

			VkDeviceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = uint32_t(queue_create_infos.size()),
				.pQueueCreateInfos = queue_create_infos.data(),

				// device layers are depreciated; spec suggests passing instance_layers or nullptr:
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,

				.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
				.ppEnabledExtensionNames = device_extensions.data(),

				// pass a pointer to a VkPhysicalDeviceFeatures to request specific features: (e.g., thick lines)
				.pEnabledFeatures = &device_features,
			};

			VK(vkCreateDevice(physical_device, &create_info, nullptr, &device));

			vkGetDeviceQueue(device, graphics_queue_family.value(), 0, &graphics_queue);
			vkGetDeviceQueue(device, present_queue_family.value(), 0, &present_queue);
		};
	};

	// create initial swapchain:
	recreate_swapchain();

	// create workspace resources:
	workspaces.resize(configuration.workspaces);
	for (auto &workspace : workspaces)
	{
		// create workspace fences:
		{
			VkFenceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT, // start signaled, because all workspaces are available to start
			};

			VK(vkCreateFence(device, &create_info, nullptr, &workspace.workspace_available));
		}

		// create workspace semaphores:
		{
			VkSemaphoreCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VK(vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_available));
			VK(vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_done));
		}
	}

	// run any resource creation required by Helpers structure:
	helpers.create();

	// camera
	configuration.camera = std::move(configuration_.camera);
	configuration.debug_camera = std::move(configuration_.debug_camera);
}

RTG::~RTG()
{
	// close performance log
	if (render_performance_log.is_open())
		render_performance_log.close();

	// don't destroy until device is idle:
	if (device != VK_NULL_HANDLE)
	{
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS)
		{
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	// destroy any resource destruction required by Helpers structure:
	helpers.destroy();

	// destroy workspace resources:
	for (auto &workspace : workspaces)
	{
		if (workspace.workspace_available != VK_NULL_HANDLE)
		{
			vkDestroyFence(device, workspace.workspace_available, nullptr);
			workspace.workspace_available = VK_NULL_HANDLE;
		}
		if (workspace.image_available != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, workspace.image_available, nullptr);
			workspace.image_available = VK_NULL_HANDLE;
		}
		if (workspace.image_done != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, workspace.image_done, nullptr);
			workspace.image_done = VK_NULL_HANDLE;
		}
	}
	workspaces.clear();

	// destroy the swapchain:
	destroy_swapchain();

	// destroy the rest of the resources:
	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (window != nullptr)
	{
		glfwDestroyWindow(window);
		window = nullptr;
	}

	if (debug_messenger != VK_NULL_HANDLE)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			debug_messenger = VK_NULL_HANDLE;
		}
	}

	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}

void RTG::recreate_swapchain()
{

	// clean up swapchain if it already exists
	if (!swapchain_images.empty())
	{
		destroy_swapchain();
	}

	// determine size, image count, and transform for swapchain
	VkSurfaceCapabilitiesKHR capabilities;
	VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities));

	swapchain_extent = capabilities.currentExtent;

	uint32_t requested_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0)
	{
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
			.oldSwapchain = VK_NULL_HANDLE // NOTE: could be more efficient by passing old swapchain handle here instead of destroying it
		};

		std::vector<uint32_t> queue_family_indices{
			graphics_queue_family.value(),
			present_queue_family.value()};

		// if images will be presented on a different queue, make sure they are shared:
		if (queue_family_indices[0] != queue_family_indices[1])
		{
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // shared
			create_info.queueFamilyIndexCount = uint32_t(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
		}
		else
		{
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // not shared
		}

		VK(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain));
	};

	// get the swapchain images
	{
		// 2-calls pattern: get the size first, then resize and get the data
		uint32_t count = 0;
		VK(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr));
		swapchain_images.resize(count);
		VK(vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()));
	};

	// make image views for swapchain images
	{
		swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < swapchain_images.size(); ++i)
		{
			VkImageViewCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = swapchain_images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = surface_format.format,
				.components{
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY},
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1},
			};
			VK(vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[i]));
		}
	}

	// report information
	if (configuration.debug)
	{
		std::cout << "[RTG] (recreate_swapchain) Surface is now: transform: " << capabilities.currentTransform << "size: "
				  << capabilities.currentExtent.width << "x" << capabilities.currentExtent.height << ".\n";
		std::cout << "[RTG] (recreate_swapchain) Swapchain is now " << swapchain_images.size()
				  << " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}

void RTG::destroy_swapchain()
{

	VK(vkDeviceWaitIdle(device)); // wait for any rendering to old swapchain to finish

	// clean up image views referencing the swapchain
	for (auto &image_view : swapchain_image_views)
	{
		vkDestroyImageView(device, image_view, nullptr);
		image_view = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	// forget handles to swapchain images
	// (swapchain image handlers are owned by the swapchain itself,
	// will destroy by deallocating the swapchain itself)
	swapchain_images.clear();

	// deallocate the swapchain and (thus) its images:
	if (swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
	std::vector<InputEvent> *event_queue = reinterpret_cast<std::vector<InputEvent> *>(glfwGetWindowUserPointer(window));
	if (!event_queue)
		return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event)); // make sure the parts we dont write are in known state

	event.type = InputEvent::MouseMotion;
	event.motion.x = float(xpos);
	event.motion.y = float(ypos);
	event.motion.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b)
	{ // different mouse buttons
		if (glfwGetMouseButton(window, b) == GLFW_PRESS)
		{ // E.g. if the mouse is moving with the right button held down, pan the camera
			event.motion.state |= (1 << b);
		}
	}

	event_queue->emplace_back(event);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	std::vector<InputEvent> *event_queue = reinterpret_cast<std::vector<InputEvent> *>(glfwGetWindowUserPointer(window));
	if (!event_queue)
		return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event)); // make sure the parts we dont write are in known state

	if (action == GLFW_PRESS)
	{
		event.type = InputEvent::MouseButtonDown;
	}
	else if (action == GLFW_RELEASE)
	{
		event.type = InputEvent::MouseButtonUp;
	}
	else
	{
		std::cerr << "Strange: unknown mouse button action." << std::endl;
		return;
	}

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	event.button.x = float(xpos);
	event.button.y = float(ypos);
	event.button.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b)
	{ // different mouse buttons
		if (glfwGetMouseButton(window, b) == GLFW_PRESS)
		{
			event.button.state |= (1 << b);
		}
	}
	event.button.button = uint8_t(button);
	event.button.mods = uint8_t(mods);

	event_queue->emplace_back(event);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	std::vector<InputEvent> *event_queue = reinterpret_cast<std::vector<InputEvent> *>(glfwGetWindowUserPointer(window));
	if (!event_queue)
		return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event)); // make sure the parts we dont write are in known state

	event.type = InputEvent::MouseWheel;
	event.wheel.x = float(xoffset);
	event.wheel.y = float(yoffset);

	event_queue->emplace_back(event);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	std::vector<InputEvent> *event_queue = reinterpret_cast<std::vector<InputEvent> *>(glfwGetWindowUserPointer(window));
	if (!event_queue)
		return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event)); // make sure the parts we dont write are in known state

	if (action == GLFW_PRESS)
	{
		event.type = InputEvent::KeyDown;
	}
	else if (action == GLFW_RELEASE)
	{
		event.type = InputEvent::KeyUp;
	}
	else if (action == GLFW_REPEAT)
	{
		// ignore repeats (key repeats behave differently across computers, and users almost never know how to change the behavior)
		return;
	}
	else
	{
		std::cerr << "Strange: got unknown keyboard action." << std::endl;
	}

	event.key.key = key;
	event.key.mods = mods;

	event_queue->emplace_back(event);
}

void RTG::run(Application &application)
{

	// initial on_swapchain
	auto on_swapchain = [&, this]()
	{
		application.on_swapchain(*this, SwapchainEvent{
											.extent = swapchain_extent,
											.images = swapchain_images,
											.image_views = swapchain_image_views,
										});
	};
	on_swapchain();

	// setup event handling
	std::vector<InputEvent> event_queue;
	glfwSetWindowUserPointer(window, &event_queue);

	glfwSetCursorPosCallback(window, cursor_pos_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

	// setup time handling
	std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();
	// headless_start = std::chrono::high_resolution_clock::now();

	while (!glfwWindowShouldClose(window))
	{
		timespot_before_record = std::chrono::high_resolution_clock::now();

		// event handling
		glfwPollEvents();

		for (InputEvent const &input : event_queue)
		{
			application.on_input(input);
		}
		event_queue.clear();

		// elapsed time handling
		{
			std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
			float dt = float(std::chrono::duration<double>(after - before).count());
			before = after;

			dt = std::min(dt, 0.1f); // lag if frame rate dips too low

			application.update(dt);
		};

		// render handling (with on_swapchain as needed)
		{
			uint32_t workspace_index;
			// acquire a workspace (getting a set of buffers that aren't being used in a current rendering operation.)
			{
				assert(next_workspace < workspaces.size());
				workspace_index = next_workspace;
				next_workspace = (next_workspace + 1) % workspaces.size();

				// wait until the workspace is not being used:
				VK(vkWaitForFences(device, 1, &workspaces[workspace_index].workspace_available, VK_TRUE, UINT64_MAX));

				// mark the workspace as in use:
				VK(vkResetFences(device, 1, &workspaces[workspace_index].workspace_available));
			};

			uint32_t image_index = -1U;
			// acquire an image (resize swapchain if needed)
			{
			retry:
				// Ask the swapchain for the next image index -- note careful return handling:
				if (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, workspaces[workspace_index].image_available, VK_NULL_HANDLE, &image_index); // [TODO] headless, AVAILABLE
					result == VK_ERROR_OUT_OF_DATE_KHR)
				{ // C++17: if sentence with initializer
					// if the swapchain is out-of-date, recreate it and run the loop again:
					std::cerr << "Recreating swapchain because vkAcquireNextImageKHR returned " << string_VkResult(result) << "." << std::endl;
					recreate_swapchain();
					on_swapchain();
					goto retry;
				}
				else if (result == VK_SUBOPTIMAL_KHR)
				{
					// if the swapchain is suboptimal, render to it and recreate it later:
					std::cerr << "Suboptimal swapchain format -- ignoring for the moment." << std::endl;
				}
				else if (result != VK_SUCCESS)
				{
					// other non-success results are genuine errors:
					throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(result)) + ")!");
				}
			};

			// image availible
			// headless_accumulator = std::chrono::high_resolution_clock::now();
			// headless_accumulated_time = std::chrono::duration<double>(headless_accumulator - headless_start).count();
			// std::cout << headless_accumulated_time << " AVAILABLE" << std::endl;

			// queue rendering work
			application.render(*this, RenderParams{
										  .workspace_index = workspace_index,
										  .image_index = image_index,
										  .image_available = workspaces[workspace_index].image_available,
										  .image_done = workspaces[workspace_index].image_done,
										  .workspace_available = workspaces[workspace_index].workspace_available,
									  });

			// queue the work for presentation: (resize swapchain if needed)
			{
				VkPresentInfoKHR present_info{
					.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &workspaces[workspace_index].image_done,
					.swapchainCount = 1,
					.pSwapchains = &swapchain,
					.pImageIndices = &image_index,
				};

				assert(present_queue);

				// note, again, the careful return handling:
				if (VkResult result = vkQueuePresentKHR(present_queue, &present_info); // [TODO] headless, SAVE
					result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
				{
					std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(result) << "." << std::endl;
					recreate_swapchain();
					on_swapchain();
				}
				else if (result != VK_SUCCESS)
				{
					throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(result)) + ")!");
				}
			}

			// headless_accumulator = std::chrono::high_resolution_clock::now();
			// headless_accumulated_time = std::chrono::duration<double>(headless_accumulator - headless_start).count();
			// ++ headless_image_done_cnt;
			// std::cout << headless_accumulated_time << " SAVE mid-frame-" << headless_image_done_cnt << ".ppm" << std::endl;

			
		};

		timespot_after_record = std::chrono::high_resolution_clock::now();

		float dt = float(std::chrono::duration_cast<std::chrono::microseconds>(timespot_after_record - timespot_before_record).count());
		// std::cout << "[Render] Render time: " << dt << " microseconds" << std::endl;

		if (render_performance_log.is_open())
			render_performance_log << dt << std::endl;
	}

	// tear down event handling
	glfwSetCursorPosCallback(window, nullptr);
	glfwSetMouseButtonCallback(window, nullptr);
	glfwSetScrollCallback(window, nullptr);
	glfwSetKeyCallback(window, nullptr);

	glfwSetWindowUserPointer(window, nullptr);
}
