#include "Source/Camera/Camera.hpp"

Camera::Camera()
{
    // camera modes related
    camera_attributes.aspect = 1.5f;
    camera_attributes.vfov = 60.0f;
    camera_attributes.near = 0.1f;
    camera_attributes.far = 1000.0f;

    camera_mode_cnt = 2;
    current_camera_mode = USER;

    // camera status
    movements.up = false;
    movements.down = false;
    movements.left = false;
    movements.right = false;
    movements.forward = false;
    movements.backward = false;

    postures.yaw_left = false;
    postures.yaw_right = false;
    postures.pitch_up = false;
    postures.pitch_down = false;

    // camera settings
    sensitivity.kb_forward = 0.15f;
    sensitivity.kb_rightward = 0.1f;
    sensitivity.kb_upward = 0.08f;
    sensitivity.kb_yaw = 0.5f;
    sensitivity.kb_pitch = 0.25f;
    sensitivity.mouse_yaw = 0.1f;
    sensitivity.mouse_pitch = 0.1f;
    
    unit_angle = 1.f;

    // camera initial posture
    position = glm::vec3{13.0f, -5.0f, 5.0f};
    target_position = glm::vec3{0.0f, 0.0f, 0.0f};

    front = glm::normalize(target_position - position);
    up = glm::vec3{0.0f, -1.0f, 0.0f};
    right = glm::cross(front, up);

    roll = 0.f;
    update_camera_eular_angles_from_vectors();
}

Camera::~Camera()
{
}

void Camera::reset_camera_control_status()
{
    movements.up = false;
    movements.down = false;
    movements.left = false;
    movements.right = false;
    movements.forward = false;
    movements.backward = false;

    postures.yaw_left = false;
    postures.yaw_right = false;
    postures.pitch_up = false;
    postures.pitch_down = false;
}

void Camera::update_camera_eular_angles_from_vectors()
{
    yaw = glm::degrees(atan2(front.x, front.z)); // looking forward along +z, rotating around y
    pitch = glm::degrees(atan2(-front.y, sqrt(front.x * front.x + front.z * front.z))); // looking forward along +z, rotating around +x

    update_camera_vectors_from_eular_angles();
}

void Camera::update_camera_vectors_from_eular_angles()
{
    /* cr. https://learnopengl.com/Getting-started/Camera based on OpenGL coordinates (+Y up, -Z forward, +X right),
           correct the formulas based on Vulkan coordinates (-Y up, +Z forward, +X right)  */

    if (pitch > 89.f)
        pitch = 89.f;
    if (pitch < -89.f)
        pitch = -89.f;
    
    if (yaw > 180.f)
        yaw -= 360.f;
    if (yaw < -180.f)
        yaw += 360.f;

    const float yawRad = glm::radians(yaw);
    const float pitchRad = glm::radians(pitch);

    const float sy = sin(yawRad);
    const float cy = cos(yawRad);
    const float sp = sin(pitchRad);
    const float cp = cos(pitchRad);

    front.x = sy * cp;  // +X right
    front.y = -sp;      // -Y up
    front.z = cy * cp;  // +Z forward

    front = glm::normalize(front);

    up = glm::vec3(0.0f, -1.0f, 0.0f);
    right = glm::normalize(glm::cross(front, up));
}


void Camera::update_info_from_another_camera(const Camera &updateFrom)
{
    camera_attributes.vfov = updateFrom.camera_attributes.vfov;
    camera_attributes.aspect = updateFrom.camera_attributes.aspect;
    camera_attributes.near = updateFrom.camera_attributes.near;
    camera_attributes.far = updateFrom.camera_attributes.far;

    position = updateFrom.position;
    front = updateFrom.front;
    up = updateFrom.up;
    yaw = updateFrom.yaw;
    pitch = updateFrom.pitch;
    roll = updateFrom.roll;

    reset_camera_control_status();
}


mat4 Camera::apply_scene_mode_camera(SceneMgr &sceneMgr)
{
    assert(sceneMgr.currentSceneCameraItr != sceneMgr.cameraObjectMap.end());

    mat4 CLIP_FROM_WORLD;

    SceneMgr::CameraObject *camera = sceneMgr.currentSceneCameraItr->second;

    if (std::holds_alternative<SceneMgr::PerspectiveParameters>(camera->projectionParameters))
    {
        current_camera_mode = Camera::SCENE;

        // update camera parameters (partial)
        const SceneMgr::PerspectiveParameters &perspective_info = std::get<SceneMgr::PerspectiveParameters>(camera->projectionParameters);
        camera_attributes.aspect = perspective_info.aspect;
        camera_attributes.vfov = perspective_info.vfov;
        camera_attributes.near = perspective_info.nearZ;
        camera_attributes.far = perspective_info.farZ;

        auto findCameraNodeResult = sceneMgr.nodeObjectMap.find(camera->name); // [WARNING] the camera CAMERA and NODE Object should always have the same name!
        if (findCameraNodeResult != sceneMgr.nodeObjectMap.end())
        {
            SceneMgr::NodeObject *cameraNode = findCameraNodeResult->second;
            

            // update the main camera info (perspective)
            glm::mat4 camera_perspective = glm::perspective (
                perspective_info.vfov,
                perspective_info.aspect,
                perspective_info.nearZ,
                perspective_info.farZ
            );

            glm::mat4 LOCAL_TO_WORLD;
            auto findCameraMatrixResult = sceneMgr.nodeMatrixMap.find(cameraNode->name); 
            if (findCameraMatrixResult != sceneMgr.nodeMatrixMap.end())
            {
                // update CLIP_FROM_WORLD matrix based on current scene camera

                LOCAL_TO_WORLD = findCameraMatrixResult->second;                                        // camera local to world
                glm::mat4 WORLD_TO_LOCAL = glm::inverse(LOCAL_TO_WORLD);                                // world (the scene) to camera local 
                glm::mat4 flip_y_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));    // Vulkan has a y pointing down the screen

                /* Thanks to Leon Li for helping me to correct my understanding of the CLIP_FROM_WORLD calculation formula (= perspective * WORLD_TO_LOCAL) for SCENE mode. */
                CLIP_FROM_WORLD = TypeHelper::convert_glm_mat4_to_mat4(camera_perspective * flip_y_matrix  * WORLD_TO_LOCAL); // clip happens in camera local

                // update the main camera info (vectors, eular angles, control status)
                
                right = glm::normalize(glm::vec3(LOCAL_TO_WORLD[0][0], LOCAL_TO_WORLD[0][1], LOCAL_TO_WORLD[0][2]));
                up = -glm::normalize(glm::vec3(LOCAL_TO_WORLD[1][0], LOCAL_TO_WORLD[1][1], LOCAL_TO_WORLD[1][2]));
                front = -glm::normalize(glm::vec3(LOCAL_TO_WORLD[2][0], LOCAL_TO_WORLD[2][1], LOCAL_TO_WORLD[2][2])); // camera look toward -Z
                position = glm::vec3(LOCAL_TO_WORLD[3][0], LOCAL_TO_WORLD[3][1], LOCAL_TO_WORLD[3][2]);

                update_camera_eular_angles_from_vectors();

                reset_camera_control_status();
            }
            else
            {
                throw std::runtime_error("Scene camera named \"" + camera->name + "\" matrix not found. Application exits.");
            }
        }
    }

    return CLIP_FROM_WORLD;
}
