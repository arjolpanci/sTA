#ifndef WORLD_H
#define WORLD_H

#include <optional>
#include <vector>
#include <glm/glm.hpp>

#include "collision_box.hpp"

// A visible, collidable axis-aligned box (buildings, border walls).
struct StaticBox
{
    glm::vec3 center;
    glm::vec3 size;
    glm::vec3 color;
    bool facade = false;
};

// Walkable wedge. Its high end and sides block entry below the surface.
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

// Deterministic street grid, static geometry, ramps, and support queries.
class World
{
public:
    World();
    static std::vector<glm::vec3> trafficLoop(float x, float z);

    const std::vector<StaticBox>& boxes() const { return m_boxes; }
    const std::vector<StaticBox>& decorations() const { return m_decorations; }
    const std::vector<Ramp>& ramps() const { return m_ramps; }
    glm::vec2 groundSize() const { return m_groundSize; }

    // Upright oriented actors against static solids and ramp walls.
    bool collides(const CollisionBox& box) const;

    // the height of whatever's underfoot at (x, z): the flat floor (0), a
    // building/wall rooftop if (x, z) is over one, or a ramp - whichever is
    // highest. Does not know about other actors (see Game::collisionPredicateFor
    // for that side of collision).
    float groundHeightAt(float x, float z, float maxHeight = 10000.0f) const;
    glm::vec3 surfaceNormal(const glm::vec3& feet) const;
    float supportHeight(const CollisionBox& actor, float maxHeight) const;

private:
    // centerOnGround is the center of the footprint at y=0; the box is lifted
    // so it sits on the ground
    void addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color);
    void addRamp(const glm::vec3& footprintCenter, const glm::vec2& footprintSize, bool alongX,
                 float lowHeight, float highHeight, const glm::vec3& color);

    glm::vec2 m_groundSize{ 360.0f, 360.0f }; // x, z extents
    std::vector<StaticBox> m_boxes;
    std::vector<StaticBox> m_decorations;
    std::vector<CollisionBox> m_colliders;
    std::vector<Ramp> m_ramps;
};

#endif
