#include "player.hpp"

#include <cmath>

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
        glm::vec3 delta = dir * speed * dt;

        // move one axis at a time and revert on hit, so we slide along walls
        // instead of sticking to them
        position.x += delta.x;
        if (ctx.collides(aabb()))
            position.x -= delta.x;

        position.z += delta.z;
        if (ctx.collides(aabb()))
            position.z -= delta.z;
    }

    // vertical: gravity, jumping, and landing on the ground or a low ledge -
    // same move-and-revert pattern as above, just on the Y axis
    if (ctx.input.keyDown(GLFW_KEY_SPACE))
        m_vertical.jump(jumpSpeed);

    float deltaY = m_vertical.step(dt);
    position.y += deltaY;
    if (position.y <= 0.0f)
    {
        position.y = 0.0f;
        m_vertical.land();
    }
    else if (ctx.collides(aabb()))
    {
        position.y -= deltaY;
        if (deltaY < 0.0f)
            m_vertical.land();
        else
            m_vertical.bonkHead();
    }
    else
    {
        m_vertical.grounded = false;
    }
}

void Player::render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const
{
    if (!controlled)
        return; // hidden while riding in a vehicle

    renderer.draw(cubeMesh, Mesh::boxMatrix(position + glm::vec3(0.0f, size.y * 0.5f, 0.0f), size, yaw),
                  glm::vec3(0.85f, 0.30f, 0.20f));
}

AABB Player::aabb() const
{
    glm::vec3 half = size * 0.5f;
    return AABB::fromCenterHalf(position + glm::vec3(0.0f, half.y, 0.0f), half);
}
