#include "player.hpp"

#include <cmath>
#include <GLFW/glfw3.h>

#include "Core/input.hpp"
#include "Rendering/camera.hpp"

void Player::update(const Input& input, const Camera& camera, float dt, const std::function<bool(const AABB&)>& collides)
{
    // movement is camera-relative, GTA-style
    glm::vec3 forward = camera.forwardXZ();
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 dir(0.0f);
    if (input.keyDown(GLFW_KEY_W)) dir += forward;
    if (input.keyDown(GLFW_KEY_S)) dir -= forward;
    if (input.keyDown(GLFW_KEY_D)) dir += right;
    if (input.keyDown(GLFW_KEY_A)) dir -= right;

    if (glm::dot(dir, dir) == 0.0f)
        return;

    dir = glm::normalize(dir);
    yaw = glm::degrees(std::atan2(dir.x, dir.z));

    float speed = input.keyDown(GLFW_KEY_LEFT_SHIFT) ? runSpeed : walkSpeed;
    glm::vec3 delta = dir * speed * dt;

    // move one axis at a time and revert on hit, so we slide along walls
    // instead of sticking to them
    position.x += delta.x;
    if (collides(aabb()))
        position.x -= delta.x;

    position.z += delta.z;
    if (collides(aabb()))
        position.z -= delta.z;
}

AABB Player::aabb() const
{
    glm::vec3 half = size * 0.5f;
    return AABB::fromCenterHalf(position + glm::vec3(0.0f, half.y, 0.0f), half);
}
