
#include "Source/Configuration/RTG.hpp"

#include "Source/Application/Wanderer/Wanderer.hpp"

#include <iostream>

int main(int argc, char **argv)
{
	// main wrapped in a try-catch so we can print some debug info about uncaught exceptions:
	try
	{

		// configure application:
		RTG::Configuration configuration;

		configuration.application_info = VkApplicationInfo{
			.pApplicationName = "Wanderer",
			.applicationVersion = VK_MAKE_VERSION(0, 0, 0),
			.pEngineName = "Unknown",
			.engineVersion = VK_MAKE_VERSION(0, 0, 0),
			.apiVersion = VK_API_VERSION_1_3};

		bool print_usage = false;

		try
		{
			configuration.parse(argc, argv);
		}
		catch (std::runtime_error &e)
		{
			std::cerr << "Failed to parse arguments:\n"
					  << e.what() << std::endl;
			print_usage = true;
		}

		if (print_usage)
		{
			std::cerr << "Usage:" << std::endl;
			RTG::Configuration::usage([](const char *arg, const char *desc)
									  { std::cerr << "    " << arg << "\n        " << desc << std::endl; });
			return 1;
		}

		// loads vulkan library, creates surface, initializes helpers:
		RTG rtg(configuration);

		// initializes global (whole-life-of-application) resources:
		Wanderer application(rtg);

		// set up camera
		const std::string &target_scene_camera =  rtg.configuration.specified_default_camera;
		if (target_scene_camera != "")
		{
			auto findCameraResult = rtg.configuration.sceneMgr.cameraObjectMap.find(target_scene_camera);
			if (findCameraResult != rtg.configuration.sceneMgr.cameraObjectMap.end())
			{
				const SceneMgr::CameraObject *camera_info = findCameraResult->second;
				if (std::holds_alternative<SceneMgr::PerspectiveParameters>(camera_info->projectionParameters))
				{
					const SceneMgr::PerspectiveParameters &perspective_info = std::get<SceneMgr::PerspectiveParameters>(camera_info->projectionParameters);
					rtg.configuration.camera_attributes.aspect = perspective_info.aspect;
					rtg.configuration.camera_attributes.vfov = perspective_info.vfov;
					rtg.configuration.camera_attributes.near = perspective_info.nearZ;
					rtg.configuration.camera_attributes.far = perspective_info.farZ;

					std::cout << "(set up camera) success: " << camera_info->name << std::endl;
				}
			}
			else
			{
				throw std::runtime_error("Scene camera named \"" + target_scene_camera + "\" not found. Application exits.");
			}
		}

		// main loop -- handles events, renders frames, etc:
		rtg.run(application);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
}
