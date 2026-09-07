#include "player.hpp"

#include <cmath>
#include "character_model.hpp"

#include "Rendering/camera.hpp"  // pulls in glad.h; must precede GLFW/glfw3.h
#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"
#include <GLFW/glfw3.h>

#include "Core/input.hpp"

void Player::update(const ActorContext& ctx, float dt)
{
    if (!ctx.controlled)
        return; // riding inside a vehicle right now

    // horizontal movement is camera-relative, GTA-style
    glm::vec3 forward = ctx.camera.forwardXZ();
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 dir(0.0f);
    if (ctx.input.keyDown(GLFW_KEY_W)) dir += forward;
    if (ctx.input.keyDown(GLFW_KEY_S)) dir -= forward;
    if (ctx.input.keyDown(GLFW_KEY_D)) dir += right;
    if (ctx.input.keyDown(GLFW_KEY_A)) dir -= right;

    if (glm::dot(dir, dir) > 0.0f)
    {
        dir = glm::normalize(dir);
        yaw = glm::degrees(std::atan2(dir.x, dir.z));

        float speed = ctx.input.keyDown(GLFW_KEY_LEFT_SHIFT) ? runSpeed : walkSpeed;
        if (m_swimming) speed = walkSpeed * .5f;
        glm::vec3 delta = dir * speed * dt;

        glm::vec3 previous = position;
        moveHorizontal(position, delta, m_vertical.grounded, [this]() { return collisionBox(); }, ctx);
        m_gait += glm::length(position - previous) * 7.0f;
    }

    // vertical: gravity, jumping, and following the ground - flat, a ramp,
    // or a rooftop - underfoot
    if (ctx.input.keyDown(GLFW_KEY_SPACE))
        m_vertical.jump(m_swimming ? jumpSpeed * .45f : jumpSpeed);

    float groundY = ctx.groundHeightAt(position.x, position.z);
    resolveVerticalMotion(m_vertical, position.y, dt, groundY, [&]() { return ctx.collides(collisionBox()); });
    m_swimming = groundY < ctx.seaLevel - .9f && position.y <= ctx.seaLevel - .9f;
    if (m_swimming) {position.y = ctx.seaLevel - .9f; m_vertical.land();}
}

void Player::render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const
{
    if (!controlled)
        return; // hidden while riding in a vehicle

    drawCharacter(renderer, cubeMesh, position, size.y, yaw, glm::vec3(0.85f, 0.30f, 0.20f), m_gait, false);
}

void Player::renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const
{
    if (!controlled)
        return;

    drawCharacter(renderer, cubeMesh, position, size.y, yaw, glm::vec3(0.85f, 0.30f, 0.20f), m_gait, true);
}

CollisionBox Player::collisionBox() const
{
    glm::vec3 half = size * 0.5f;
    return CollisionBox::fromCenterHalf(position + glm::vec3(0.0f, half.y, 0.0f), half);
}
