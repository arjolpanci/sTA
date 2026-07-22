#include "pedestrian.hpp"

#include <cmath>

#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"

Pedestrian::Pedestrian(const glm::vec3& startPosition, WaypointPath path, const glm::vec3& color)
    : m_position(startPosition), m_color(color), m_path(std::move(path))
{
}

void Pedestrian::update(const ActorContext& ctx, float dt)
{
    glm::vec3 target = m_path.current();
    glm::vec3 dir(target.x - m_position.x, 0.0f, target.z - m_position.z);
    float dist = glm::length(dir);

    if (dist > 0.001f)
    {
        dir /= dist;
        m_yaw = glm::degrees(std::atan2(dir.x, dir.z));

        glm::vec3 delta = dir * walkSpeed * dt;

        // move one axis at a time and revert on hit, same as Player
        m_position.x += delta.x;
        if (ctx.collides(aabb()))
            m_position.x -= delta.x;

        m_position.z += delta.z;
        if (ctx.collides(aabb()))
            m_position.z -= delta.z;
    }

    // vertical: gravity only - pedestrians don't jump
    float deltaY = m_vertical.step(dt);
    m_position.y += deltaY;
    if (m_position.y <= 0.0f)
    {
        m_position.y = 0.0f;
        m_vertical.land();
    }
    else if (ctx.collides(aabb()))
    {
        m_position.y -= deltaY;
        if (deltaY < 0.0f)
            m_vertical.land();
        else
            m_vertical.bonkHead();
    }
    else
    {
        m_vertical.grounded = false;
    }

    m_path.advanceIfReached(m_position, 1.0f);
}

void Pedestrian::render(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    renderer.draw(cubeMesh, Mesh::boxMatrix(m_position + glm::vec3(0.0f, m_size.y * 0.5f, 0.0f), m_size, m_yaw), m_color);
}

AABB Pedestrian::aabb() const
{
    glm::vec3 half = m_size * 0.5f;
    return AABB::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), half);
}
