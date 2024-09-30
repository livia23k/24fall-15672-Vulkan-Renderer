#pragma once

#include <glm/glm.hpp>
#include <stdint.h>

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
    
    glm::vec3 camera_position;
    glm::vec3 target_position;
    glm::vec3 camera_up;

    // void apply_scene_mode_camera();

};