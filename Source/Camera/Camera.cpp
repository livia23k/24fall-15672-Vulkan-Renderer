#include "Source/Camera/Camera.hpp"

Camera::Camera()
{
    camera_attributes.aspect = 1.5f;
    camera_attributes.vfov = 60.0f;
    camera_attributes.near = 0.1f;
    camera_attributes.far = 1000.0f;

    camera_mode_cnt = 3;
    current_camera_mode = USER;

    position = glm::vec3{13.0f, -5.0f, 5.0f};
    target_position = glm::vec3{0.0f, 0.0f, 0.0f};

    front = glm::normalize(target_position - position);
    up = glm::vec3{0.0f, -1.0f, 0.0f};
    right = glm::cross(front, up);

    sensitivity.kb_forward = 0.15f;
    sensitivity.kb_rightward = 0.1f;
    sensitivity.kb_upward = 0.08f;

    yaw = glm::degrees(atan2(front.x, front.z)); // looking forward along +z, rotating around y
    pitch = glm::degrees(atan2(-front.y, sqrt(front.x * front.x + front.z * front.z))); // looking forward along +z, rotating around +x
    roll = 0.0f;

    update_camera_vectors();
}

Camera::~Camera()
{
}

// [TOFIX]
void Camera::update_camera_vectors()
{
    /* cr. https://learnopengl.com/Getting-started/Camera based on OpenGL coordinates (+Y up, -Z forward, +X right),
           correct the formulas based on Vulkan coordinates (-Y up, +Z forward, +X right)  */

    if (pitch > 89.f)
        pitch = 89.f;
    if (pitch < -89.f)
        pitch = -89.f;
    
    if (yaw > 180.f)
        yaw -= 360.f;
    if (yaw < 180.f)
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

mat4 Camera::apply_scene_mode_camera(SceneMgr &sceneMgr)
{
    assert(sceneMgr.currentSceneCameraItr != sceneMgr.cameraObjectMap.end());

    mat4 CLIP_FROM_WORLD;

    SceneMgr::CameraObject *camera = sceneMgr.currentSceneCameraItr->second;

    if (std::holds_alternative<SceneMgr::PerspectiveParameters>(camera->projectionParameters))
    {
        current_camera_mode = Camera::SCENE;

        const SceneMgr::PerspectiveParameters &perspective_info = std::get<SceneMgr::PerspectiveParameters>(camera->projectionParameters);
        camera_attributes.aspect = perspective_info.aspect;
        camera_attributes.vfov = perspective_info.vfov;
        camera_attributes.near = perspective_info.nearZ;
        camera_attributes.far = perspective_info.farZ;

        auto findCameraNodeResult = sceneMgr.nodeObjectMap.find(camera->name); // [WARNING] the camera CAMERA and NODE Object should always have the same name!
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

            glm::mat4 LOCAL_TO_WORLD;
            auto findCameraMatrixResult = sceneMgr.nodeMatrixMap.find(cameraNode->name); 
            if (findCameraMatrixResult != sceneMgr.nodeMatrixMap.end())
            {
                LOCAL_TO_WORLD = findCameraMatrixResult->second;                                        // camera local to world
                glm::mat4 WORLD_TO_LOCAL = glm::inverse(LOCAL_TO_WORLD);                                // world (the scene) to camera local 
                glm::mat4 flip_y_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));    // Vulkan has a y pointing down the screen

                CLIP_FROM_WORLD = TypeHelper::convert_glm_mat4_to_mat4(camera_perspective * flip_y_matrix  * WORLD_TO_LOCAL); // clip happens in camera local
            }
            else
            {
                throw std::runtime_error("Scene camera named \"" + camera->name + "\" matrix not found. Application exits.");
            }
        }
    }

    return CLIP_FROM_WORLD;
}
