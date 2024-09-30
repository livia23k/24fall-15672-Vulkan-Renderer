#include "Source/Camera/Camera.hpp"

Camera::Camera()
{
    camera_attributes.aspect = 1.5f;
    camera_attributes.vfov = 60.0f;
    camera_attributes.near = 0.1f;
    camera_attributes.far = 1000.0f;	

    camera_mode_cnt = 3;
    current_camera_mode = USER;

    // USER mode attributes
    camera_position = glm::vec3{0.0f, 0.0f, 0.0f};
    target_position = glm::vec3{0.0f, 0.0f, 0.0f};
    camera_up = glm::vec3{0.0f, 1.0f, 0.0f};
}

Camera::~Camera()
{
}