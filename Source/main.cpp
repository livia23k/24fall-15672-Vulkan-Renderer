
#include "Source/Configuration/RTG.hpp"
#include "Source/Tools/TypeHelper.hpp"
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
			SceneMgr &sceneMgr = rtg.configuration.sceneMgr;

			sceneMgr.currentSceneCameraItr = sceneMgr.cameraObjectMap.find(target_scene_camera);
			if (sceneMgr.currentSceneCameraItr == sceneMgr.cameraObjectMap.end()) {
				throw std::runtime_error("Scene camera object named \"" + target_scene_camera + "\" not found. Application exits.");
			}

			application.CLIP_FROM_WORLD = rtg.configuration.camera.apply_scene_mode_camera(sceneMgr);
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
