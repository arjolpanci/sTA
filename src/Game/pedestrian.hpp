#ifndef PEDESTRIAN_H
#define PEDESTRIAN_H

#include <glm/glm.hpp>

#include "actor.hpp"
#include "waypoint_path.hpp"

// A simple wandering NPC: cube-shaped, walks a looping/patrolling waypoint
// path, sliding along obstacles the same way Player does. Never
// player-controlled, so it ignores ActorContext::controlled entirely.
class Pedestrian : public Actor
{
public:
    Pedestrian(const glm::vec3& startPosition, WaypointPath path, const glm::vec3& color);

    void update(const ActorContext& ctx, float dt) override;
    void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    AABB aabb() const override;

    float walkSpeed = 2.0f;

private:
    glm::vec3 m_position;
    float m_yaw = 0.0f;
    glm::vec3 m_size{ 0.5f, 1.7f, 0.5f };
    glm::vec3 m_color;
    WaypointPath m_path;
};

#endif
