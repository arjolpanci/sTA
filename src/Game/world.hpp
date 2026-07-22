#ifndef WORLD_H
#define WORLD_H

#include <vector>
#include <glm/glm.hpp>

#include "aabb.hpp"

// A visible, collidable axis-aligned box (buildings, border walls).
struct StaticBox
{
    glm::vec3 center;
    glm::vec3 size;
    glm::vec3 color;
};

// The hardcoded map: a rectangular asphalt ground plane surrounded by
// walls, with a handful of box buildings. Owns all static colliders.
class World
{
public:
    World();

    const std::vector<StaticBox>& boxes() const { return m_boxes; }
    glm::vec2 groundSize() const { return m_groundSize; }

    bool collides(const AABB& box) const;

private:
    // centerOnGround is the center of the footprint at y=0; the box is lifted
    // so it sits on the ground
    void addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color);

    glm::vec2 m_groundSize{ 160.0f, 100.0f }; // x, z extents
    std::vector<StaticBox> m_boxes;
    std::vector<AABB> m_colliders;
};

#endif
