#include "pedestrian.hpp"

#include <cmath>
#include "character_model.hpp"

#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"

Pedestrian::Pedestrian(const glm::vec3& startPosition, WaypointPath path, const glm::vec3& color, int modelIndex)
    : m_position(startPosition), m_color(color), m_path(std::move(path)),
      m_model(ModelAsset::load("resources/models/characters/"+std::string(modelIndex%8<4?"men-":"women-")+std::to_string(modelIndex%4)+".glb"))
{
}

void Pedestrian::update(const ActorContext& ctx, float dt)
{
    glm::vec3 target = m_path.current();
    glm::vec3 dir(target.x - m_position.x, 0.0f, target.z - m_position.z);
    float dist = glm::length(dir);

    float movedSpeed=0;
    if (dist > 0.001f)
    {
        dir /= dist;
        m_yaw = glm::degrees(std::atan2(dir.x, dir.z));

        glm::vec3 delta = dir * walkSpeed * dt;

        glm::vec3 previous = m_position;
        moveHorizontal(m_position, delta, m_vertical.grounded, [this]() { return collisionBox(); }, ctx);
        movedSpeed=dt>0?glm::length(glm::vec2(m_position.x-previous.x,m_position.z-previous.z))/dt:0;
    }

    m_animation.update(movedSpeed>.1f?"Walk":"Idle",dt,movedSpeed>.1f?std::clamp(movedSpeed/2.0f,.5f,1.5f):1);

    // vertical: gravity only - pedestrians don't jump, but do walk up/down
    // ramps and can fall off a rooftop like anything else
    float groundY = ctx.groundHeightAt(m_position.x, m_position.z);
    resolveVerticalMotion(m_vertical, m_position.y, dt, groundY, [&]() { return ctx.collides(collisionBox()); });

    m_path.advanceIfReached(m_position, 1.0f);
}

void Pedestrian::render(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    drawCharacter(renderer, *m_model, this, m_position, m_size.y, m_yaw, m_animation, false);
}

void Pedestrian::renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    drawCharacter(renderer, *m_model, this, m_position, m_size.y, m_yaw, m_animation, true);
}

CollisionBox Pedestrian::collisionBox() const
{
    glm::vec3 half = m_size * 0.5f;
    return CollisionBox::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), half);
}
