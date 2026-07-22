#ifndef WORLD_H
#define WORLD_H

#include <optional>
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

// A sloped walkable surface, flush with the ground at its low end and
// rising linearly to its high end. Never registered as a collider (it
// doesn't block horizontal movement, only affects ground height), so it's
// entirely separate from StaticBox/m_colliders.
struct Ramp
{
    glm::vec3 footprintCenter;  // world XZ center (y unused)
    glm::vec2 footprintSize;    // x = width (across the slope), y = length (along the slope)
    bool alongX = false;        // false: rises toward +Z: true: rises toward +X
    float lowHeight = 0.0f;
    float highHeight = 3.0f;
    glm::vec3 color{ 0.55f, 0.52f, 0.50f };

    // nullopt if (x, z) falls outside the footprint
    std::optional<float> heightAt(float x, float z) const;
};

// The hardcoded map: a rectangular asphalt ground plane surrounded by
// walls, with a handful of box buildings and a couple of ramps. Owns all
// static collision (collides()) and ground height (groundHeightAt()).
class World
{
public:
    World();

    const std::vector<StaticBox>& boxes() const { return m_boxes; }
    const std::vector<Ramp>& ramps() const { return m_ramps; }
    glm::vec2 groundSize() const { return m_groundSize; }

    // true if box overlaps something solid actors should stop for (does not
    // include ramps - they're walkable, not obstacles)
    bool collides(const AABB& box) const;

    // the height of whatever's underfoot at (x, z): the flat floor (0), a
    // building/wall rooftop if (x, z) is over one, or a ramp - whichever is
    // highest. Does not know about other actors (see Game::collisionPredicateFor
    // for that side of collision).
    float groundHeightAt(float x, float z) const;

private:
    // centerOnGround is the center of the footprint at y=0; the box is lifted
    // so it sits on the ground
    void addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color);
    void addRamp(const glm::vec3& footprintCenter, const glm::vec2& footprintSize, bool alongX,
                 float lowHeight, float highHeight, const glm::vec3& color);

    glm::vec2 m_groundSize{ 160.0f, 100.0f }; // x, z extents
    std::vector<StaticBox> m_boxes;
    std::vector<AABB> m_colliders;
    std::vector<Ramp> m_ramps;
};

#endif
