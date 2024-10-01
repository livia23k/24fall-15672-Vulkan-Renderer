#pragma once

#include "Source/DataType/Plane.hpp"
#include "Source/Camera/Camera.hpp"

/* cr. structure reference from Learn OpenGL: https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling */
struct Frustum
{
    Plane topFace;
    Plane bottomFace;
    Plane leftFace;
    Plane rightFace;
    Plane nearFace;
    Plane farFace;

    Frustum() = default;
    ~Frustum() = default;

    static Frustum createFrustumFromCamera(const Camera &camera);
    bool isBBoxInFrustum(BBox &bbox);
};
