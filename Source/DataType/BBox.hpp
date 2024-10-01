#pragma once

#include <glm/glm.hpp>
#include <vector>

/* cr. Scotty3D bbox.h: https://github.com/CMU-Graphics/Scotty3D/blob/main/src/lib/bbox.h */

struct BBox 
{
    BBox();
    BBox(glm::vec3 min, glm::vec3 max);

    BBox(const BBox &) = default;
    BBox &operator=(const BBox &) = default;
    ~BBox() = default;

    void reset();
    glm::vec3 center() const;
    bool empty() const;
    
    void enclose(glm::vec3 p);
    void enclose(BBox box);

    inline glm::vec3 hmin(glm::vec3 l, glm::vec3 r);
    inline glm::vec3 hmax(glm::vec3 l, glm::vec3 r);

    std::vector<glm::vec3> get_corners();

    glm::vec3 min, max;
};

