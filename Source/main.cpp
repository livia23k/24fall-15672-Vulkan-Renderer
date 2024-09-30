
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
			const SceneMgr &sceneMgr = rtg.configuration.sceneMgr;

			auto findCameraResult = sceneMgr.cameraObjectMap.find(target_scene_camera);
			if (findCameraResult != sceneMgr.cameraObjectMap.end())
			{
				const SceneMgr::CameraObject *camera_info = findCameraResult->second;
				if (std::holds_alternative<SceneMgr::PerspectiveParameters>(camera_info->projectionParameters))
				{
					Camera &camera = rtg.configuration.camera;

					camera.current_camera_mode = Camera::SCENE;

					const SceneMgr::PerspectiveParameters &perspective_info = std::get<SceneMgr::PerspectiveParameters>(camera_info->projectionParameters);
					camera.camera_attributes.aspect = perspective_info.aspect;
					camera.camera_attributes.vfov = perspective_info.vfov;
					camera.camera_attributes.near = perspective_info.nearZ;
					camera.camera_attributes.far = perspective_info.farZ;

					auto findCameraNodeResult = sceneMgr.nodeObjectMap.find(target_scene_camera); // [WARNING] the camera CAMERA and NODE Object should always have the same name!
					if (findCameraNodeResult != sceneMgr.nodeObjectMap.end())
					{
						SceneMgr::NodeObject *cameraNode = findCameraNodeResult->second;
						
						/* Thanks to Leon Li for helping me to correct my understanding of the CLIP_FROM_WORLD calculation formula for SCENE mode. */

						glm::mat4 camera_perspective = glm::perspective (
							perspective_info.vfov,
							perspective_info.aspect,
							perspective_info.nearZ,
							perspective_info.farZ
						);

						glm::mat4 camera_model;
						auto findCameraMatrixResult = sceneMgr.nodeMatrixMap.find(cameraNode->name); 
						if (findCameraMatrixResult != sceneMgr.nodeMatrixMap.end())
						{
							camera_model = findCameraMatrixResult->second; // camera local to world
							glm::mat4 camera_view = glm::inverse(camera_model); // world (the scene) to camera local 
							glm::mat4 flip_y_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f)); // Vulkan has a y pointing down the screen

							application.CLIP_FROM_WORLD = TypeHelper::convert_glm_mat4_to_mat4(camera_perspective * flip_y_matrix  * camera_model);
						}
						else
						{
							throw std::runtime_error("Scene camera named \"" + target_scene_camera + "\" matrix not found. Application exits.");
						}
					}
					else
					{
						throw std::runtime_error("Scene camera named \"" + target_scene_camera + "\" position not found. Application exits.");
					}
					
					std::cout << "(set up camera) success: " << camera_info->name << std::endl;
				}
			}
			else
			{
				throw std::runtime_error("Scene camera named \"" + target_scene_camera + "\" camera attributes not found. Application exits.");
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
