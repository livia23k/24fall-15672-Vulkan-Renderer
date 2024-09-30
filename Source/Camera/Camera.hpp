#pragma once

#include "Source/Tools/SceneMgr.hpp"
#include "Source/Tools/TypeHelper.hpp"
#include "Source/DataType/Mat4.hpp"

#include <glm/glm.hpp>
#include <stdint.h>
#include <string>
#include <cassert>

struct Camera
{
    Camera();
    ~Camera();
    Camera(const Camera &) = delete;

    enum Camera_Mode : uint8_t
    {
        USER,
        SCENE,
        DEBUG
    };

    struct Camera_Attributes {
        float aspect;
        float vfov;
        float near;
        float far;
    } camera_attributes;

    
    uint8_t camera_mode_cnt;
    Camera_Mode current_camera_mode;

    
    // =============================================
    // USER mode related variables 

    /* cr. movement related handle learned from CMU 15666 Computer Game Programming code base
           https://github.com/15-466/15-466-f24-base2/blob/b7584e87b2498e4491e6438770f4b4a8d593bbde/PlayMode.cpp#L70 */

    struct Camera_Movement {
        bool left;
        bool right;
        bool up;
        bool down;
        bool forward;
        bool backward;
    } movements;

    struct Camera_Posture {
        bool yaw_left;
        bool yaw_right;
        bool pitch_up;
        bool pitch_down;
    } postures;

    struct Camera_Sensitivity {
        float kb_forward;
        float kb_upward;
        float kb_rightward;
        float kb_yaw;
        float kb_pitch;
        float mouse_yaw;
        float mouse_pitch;
    } sensitivity;

    /* cr. camera parameters learned from Learn OpenGL 
           https://learnopengl.com/Getting-started/Camera#:~:text=The%20pitch%20is%20the%20angle,used%20in%20space%2Dflight%20cameras.
           but euler angles doesn't seems to work, need further work */

    glm::vec3 position;
    glm::vec3 target_position;

    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up;

    float yaw;
    float pitch;
    float roll;

    float unit_angle;

    // =============================================
    // Helper Functions

    void update_camera_vectors();

    mat4 apply_scene_mode_camera(SceneMgr &sceneMgr);

};