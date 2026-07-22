#include "vehicle.hpp"

#include <algorithm>
#include <cmath>

#include "Rendering/mesh.hpp"      // pulls in glad.h; must precede GLFW/glfw3.h
#include "Rendering/renderer.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/input.hpp"

namespace
{
    const glm::vec3 WHEEL_COLOR(0.12f, 0.12f, 0.12f);
    const glm::vec3 WINDOW_COLOR(0.20f, 0.25f, 0.32f);

    void addWheels(std::vector<VehiclePart>& parts, float halfTrack, float axleZ)
    {
        const glm::vec3 wheelSize(0.3f, 0.6f, 0.6f);
        parts.push_back({ { -halfTrack, 0.3f,  axleZ }, wheelSize, WHEEL_COLOR });
        parts.push_back({ {  halfTrack, 0.3f,  axleZ }, wheelSize, WHEEL_COLOR });
        parts.push_back({ { -halfTrack, 0.3f, -axleZ }, wheelSize, WHEEL_COLOR });
        parts.push_back({ {  halfTrack, 0.3f, -axleZ }, wheelSize, WHEEL_COLOR });
    }
}

Vehicle::Vehicle(VehicleType type, const glm::vec3& position, float yaw)
    : m_position(position), m_yaw(yaw)
{
    switch (type)
    {
    case VehicleType::Sedan:
    {
        const glm::vec3 body(0.15f, 0.25f, 0.60f);
        m_parts.push_back({ { 0.0f, 0.55f,  0.00f }, { 1.8f, 0.60f, 4.2f }, body });         // chassis
        m_parts.push_back({ { 0.0f, 1.10f, -0.35f }, { 1.6f, 0.55f, 2.0f }, WINDOW_COLOR, 32.0f }); // cabin
        addWheels(m_parts, 0.8f, 1.35f);
        m_boundsSize = { 1.9f, 1.6f, 4.4f };
        break;
    }
    case VehicleType::Taxi:
    {
        const glm::vec3 body(0.90f, 0.75f, 0.10f);
        m_parts.push_back({ { 0.0f, 0.55f,  0.00f }, { 1.8f, 0.60f, 4.2f }, body });
        m_parts.push_back({ { 0.0f, 1.10f, -0.35f }, { 1.6f, 0.55f, 2.0f }, WINDOW_COLOR, 32.0f });
        m_parts.push_back({ { 0.0f, 1.47f, -0.35f }, { 0.45f, 0.18f, 0.35f }, { 0.9f, 0.9f, 0.85f } }); // roof sign
        addWheels(m_parts, 0.8f, 1.35f);
        m_boundsSize = { 1.9f, 1.7f, 4.4f };
        break;
    }
    case VehicleType::Van:
    {
        const glm::vec3 body(0.85f, 0.85f, 0.82f);
        m_parts.push_back({ { 0.0f, 1.15f, -0.50f }, { 2.0f, 1.7f, 3.4f }, body });  // cargo box
        m_parts.push_back({ { 0.0f, 0.75f,  1.75f }, { 1.9f, 0.9f, 1.3f }, body });  // cab/hood
        addWheels(m_parts, 0.85f, 1.55f);
        m_boundsSize = { 2.1f, 2.0f, 4.8f };
        break;
    }
    }
}

glm::vec3 Vehicle::forward() const
{
    float r = glm::radians(m_yaw);
    return glm::vec3(sin(r), 0.0f, cos(r));
}

void Vehicle::update(const ActorContext& ctx, float dt)
{
    float throttle = 0.0f;
    float steer = 0.0f;

    if (ctx.controlled)
    {
        if (ctx.input.keyDown(GLFW_KEY_W)) throttle = 1.0f;
        else if (ctx.input.keyDown(GLFW_KEY_S)) throttle = -1.0f;
        if (ctx.input.keyDown(GLFW_KEY_A)) steer -= 1.0f;
        if (ctx.input.keyDown(GLFW_KEY_D)) steer += 1.0f;
    }
    else if (m_path)
    {
        // steer toward the current waypoint: the sign/magnitude of the cross
        // product between "forward" and "direction to target" is a simple
        // proportional heading controller - positive means the target is to
        // the side that increasing yaw turns toward (see Vehicle::forward())
        glm::vec3 toTarget = m_path->current() - m_position;
        toTarget.y = 0.0f;
        float dist = glm::length(toTarget);
        if (dist > 0.01f)
        {
            toTarget /= dist;
            glm::vec3 fwd = forward();
            float cross = fwd.z * toTarget.x - fwd.x * toTarget.z;
            steer = std::clamp(cross * 4.0f, -1.0f, 1.0f);
        }
        throttle = 1.0f - 0.6f * std::abs(steer); // ease off the gas mid-turn
        m_path->advanceIfReached(m_position, 3.0f);
    }
    else
    {
        return; // parked, no driver
    }

    if (throttle > 0.0f)
        m_speed += acceleration * dt;
    else if (throttle < 0.0f)
        // braking is stronger than accelerating in reverse, like a real pedal
        m_speed -= (m_speed > 0.0f ? brakeDeceleration : acceleration) * dt;
    else if (m_speed > 0.0f)
        m_speed = std::max(0.0f, m_speed - friction * dt);
    else if (m_speed < 0.0f)
        m_speed = std::min(0.0f, m_speed + friction * dt);

    m_speed = std::clamp(m_speed, -maxReverseSpeed, maxSpeed);

    // steering: scaled by speed so the car can't spin in place, and flipped
    // in reverse so it steers the way a real car does when backing up
    if (std::abs(m_speed) > 0.01f)
    {
        float speedFactor = std::clamp(std::abs(m_speed) / maxSpeed, 0.2f, 1.0f);
        float direction = m_speed >= 0.0f ? 1.0f : -1.0f;
        m_yaw += steer * turnRateDeg * speedFactor * direction * dt;
    }

    glm::vec3 delta = forward() * m_speed * dt;

    // move one axis at a time and revert on hit, killing speed so the car
    // stops cleanly against a wall instead of clipping through it
    m_position.x += delta.x;
    if (ctx.collides(aabb()))
    {
        m_position.x -= delta.x;
        m_speed = 0.0f;
    }

    m_position.z += delta.z;
    if (ctx.collides(aabb()))
    {
        m_position.z -= delta.z;
        m_speed = 0.0f;
    }

    // vertical: gravity only - vehicles don't jump, just fall if driven off
    // a ledge and follow the ground (or a ramp) underneath otherwise
    float groundY = ctx.groundHeightAt(m_position.x, m_position.z);
    resolveVerticalMotion(m_vertical, m_position.y, dt, groundY, [&]() { return ctx.collides(aabb()); });
}

void Vehicle::render(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    glm::mat4 carMatrix = glm::translate(glm::mat4(1.0f), m_position);
    carMatrix = glm::rotate(carMatrix, glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    for (const VehiclePart& part : m_parts)
    {
        glm::mat4 model = glm::scale(glm::translate(carMatrix, part.offset), part.size);
        renderer.draw(cubeMesh, model, Material{ part.color, nullptr, part.shininess });
    }
}

void Vehicle::renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    glm::mat4 carMatrix = glm::translate(glm::mat4(1.0f), m_position);
    carMatrix = glm::rotate(carMatrix, glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    for (const VehiclePart& part : m_parts)
    {
        glm::mat4 model = glm::scale(glm::translate(carMatrix, part.offset), part.size);
        renderer.drawShadow(cubeMesh, model);
    }
}

AABB Vehicle::aabb() const
{
    glm::vec3 half = m_boundsSize * 0.5f;
    // tightest axis-aligned box enclosing the rotated footprint: the local
    // half-extents projected onto world X/Z. Reduces to the exact box at
    // 0/90/180/270 degrees and over-approximates at in-between angles.
    float r = glm::radians(m_yaw);
    float c = std::abs(cos(r));
    float s = std::abs(sin(r));
    glm::vec3 rotatedHalf(half.x * c + half.z * s, half.y, half.x * s + half.z * c);
    return AABB::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), rotatedHalf);
}
