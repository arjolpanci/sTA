#ifndef AABB_H
#define AABB_H

#include <glm/glm.hpp>

// Axis-aligned bounding box. All collision in the game is AABB vs AABB
// for now; good enough for box buildings and parked cars.
struct AABB
{
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };

    static AABB fromCenterHalf(const glm::vec3& center, const glm::vec3& half)
    {
        return { center - half, center + half };
    }

    bool intersects(const AABB& other) const
    {
        return min.x < other.max.x && max.x > other.min.x &&
               min.y < other.max.y && max.y > other.min.y &&
               min.z < other.max.z && max.z > other.min.z;
    }
};

#endif
