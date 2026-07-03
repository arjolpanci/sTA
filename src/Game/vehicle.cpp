#include "vehicle.hpp"

#include <cmath>
#include <utility>

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

AABB Vehicle::aabb() const
{
    glm::vec3 half = m_boundsSize * 0.5f;
    // axis-aligned box: at 90/270 degrees the car's length lies along X
    int yawMod = ((static_cast<int>(std::round(m_yaw)) % 180) + 180) % 180;
    if (yawMod == 90)
        std::swap(half.x, half.z);
    return AABB::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), half);
}
