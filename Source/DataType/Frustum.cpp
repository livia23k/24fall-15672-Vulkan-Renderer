#include "Source/DataType/Frustum.hpp"

Frustum Frustum::createFrustumFromCamera(const Camera &camera)
{
    Frustum frustum;

    const float halfVSide = camera.camera_attributes.far * tanf(camera.camera_attributes.vfov * 0.5f);
    const float halfHSide = halfVSide * camera.camera_attributes.aspect;

    frustum.nearFace.position = camera.position + camera.camera_attributes.near * camera.front;
    frustum.nearFace.normal = camera.front;

    frustum.farFace.position = camera.position + camera.camera_attributes.far * camera.front;
    frustum.farFace.normal = -camera.front;

    frustum.rightFace.position = camera.position + halfHSide * camera.right;
    frustum.rightFace.normal = -camera.right;

    frustum.leftFace.position = camera.position - halfHSide * camera.right;
    frustum.leftFace.normal = camera.right;

    frustum.topFace.position = camera.position + halfVSide * camera.up;
    frustum.topFace.normal = -camera.up;

    frustum.bottomFace.position = camera.position - halfVSide * camera.up;
    frustum.bottomFace.normal = camera.up;

    return frustum;    
}

bool Frustum::isBBoxInFrustum(BBox &bbox)
{
    /* cr. Frustum Culling by Dion Picco: https://www.flipcode.com/archives/Frustum_Culling.shtml */

    std::vector<glm::vec3> bboxCorners = bbox.get_corners();
    int sizeCorners = bboxCorners.size();

    int iTotalIn = 0;

    for (const Plane &plane :{nearFace, farFace, leftFace, rightFace, topFace, bottomFace})
    {
        int iInCount = 0;

        for (int i = 0; i < sizeCorners; ++ i)
        {
            if (plane.pointInFront(bboxCorners[i]))
                ++ iInCount;
        }

        if (iInCount >= 6) // pass if greater equal than 6 BBox corners are in front of the plane
            ++ iTotalIn;
    }

    return (iTotalIn == 6); // if pass tests for all 6 planes, the BBox is in the frustum
}