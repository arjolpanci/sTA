#include "vehicle.hpp"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

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
        m_parts.push_back({ { 0.0f, 1.10f, -0.35f }, { 1.6f, 0.55f, 2.0f }, WINDOW_COLOR }); // cabin
        addWheels(m_parts, 0.8f, 1.35f);
        m_boundsSize = { 1.9f, 1.6f, 4.4f };
        break;
    }
    case VehicleType::Taxi:
    {
        const glm::vec3 body(0.90f, 0.75f, 0.10f);
        m_parts.push_back({ { 0.0f, 0.55f,  0.00f }, { 1.8f, 0.60f, 4.2f }, body });
        m_parts.push_back({ { 0.0f, 1.10f, -0.35f }, { 1.6f, 0.55f, 2.0f }, WINDOW_COLOR });
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

void Vehicle::updateDriving(const Input& input, float dt, const std::function<bool(const AABB&)>& collides)
{
    float throttle = 0.0f;
    if (input.keyDown(GLFW_KEY_W)) throttle = 1.0f;
    else if (input.keyDown(GLFW_KEY_S)) throttle = -1.0f;

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
        float steer = 0.0f;
        if (input.keyDown(GLFW_KEY_A)) steer -= 1.0f;
        if (input.keyDown(GLFW_KEY_D)) steer += 1.0f;

        float speedFactor = std::clamp(std::abs(m_speed) / maxSpeed, 0.2f, 1.0f);
        float direction = m_speed >= 0.0f ? 1.0f : -1.0f;
        m_yaw += steer * turnRateDeg * speedFactor * direction * dt;
    }

    glm::vec3 delta = forward() * m_speed * dt;

    // move one axis at a time and revert on hit, killing speed so the car
    // stops cleanly against a wall instead of clipping through it
    m_position.x += delta.x;
    if (collides(aabb()))
    {
        m_position.x -= delta.x;
        m_speed = 0.0f;
    }

    m_position.z += delta.z;
    if (collides(aabb()))
    {
        m_position.z -= delta.z;
        m_speed = 0.0f;
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
