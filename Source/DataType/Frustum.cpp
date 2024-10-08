#include "Source/DataType/Frustum.hpp"

Frustum Frustum::createFrustumFromCamera(const Camera &camera)
{
    /* cr. adapeted from Learn OpenGL: https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling */
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

    int cornerInFrustumCnt = 0;

    for (int i = 0; i < sizeCorners; ++ i)
    {
        int cornerInPlanerFrontCnt = 0;

        for (const Plane &plane :{nearFace, farFace, leftFace, rightFace, topFace, bottomFace})
        {
            if (plane.pointInFront(bboxCorners[i]))
                ++ cornerInPlanerFrontCnt;
        }

        if (cornerInPlanerFrontCnt == 6) // pass if corner is in front of all planes
            ++ cornerInFrustumCnt;
    }

    return (cornerInFrustumCnt >= 4); // pass if >= 6 corners are in the frustum
}