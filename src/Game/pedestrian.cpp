#include "pedestrian.hpp"

#include <cmath>
#include "character_model.hpp"

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

        glm::vec3 previous = m_position;
        moveHorizontal(m_position, delta, m_vertical.grounded, [this]() { return collisionBox(); }, ctx);
        m_gait += glm::length(m_position - previous) * 7.0f;
    }

    // vertical: gravity only - pedestrians don't jump, but do walk up/down
    // ramps and can fall off a rooftop like anything else
    float groundY = ctx.groundHeightAt(m_position.x, m_position.z);
    resolveVerticalMotion(m_vertical, m_position.y, dt, groundY, [&]() { return ctx.collides(collisionBox()); });

    m_path.advanceIfReached(m_position, 1.0f);
}

void Pedestrian::render(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    drawCharacter(renderer, cubeMesh, m_position, m_size.y, m_yaw, m_color, m_gait, false);
}

void Pedestrian::renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    drawCharacter(renderer, cubeMesh, m_position, m_size.y, m_yaw, m_color, m_gait, true);
}

CollisionBox Pedestrian::collisionBox() const
{
    glm::vec3 half = m_size * 0.5f;
    return CollisionBox::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), half);
}
