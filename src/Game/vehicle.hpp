#ifndef VEHICLE_H
#define VEHICLE_H

#include <vector>
#include <glm/glm.hpp>

#include "aabb.hpp"

enum class VehicleType
{
    Sedan,
    Taxi,
    Van
};

// One box of a car model, in the vehicle's local space
// (origin at the center of the footprint, on the ground, forward = +Z).
struct VehiclePart
{
    glm::vec3 offset;
    glm::vec3 size;
    glm::vec3 color;
};

// A parked car built out of boxes. Static for now; driving comes later.
class Vehicle
{
public:
    // NOTE: collision boxes are axis-aligned, so keep yaw to multiples of 90
    // degrees until we have oriented bounding boxes
    Vehicle(VehicleType type, const glm::vec3& position, float yaw);

    const std::vector<VehiclePart>& parts() const { return m_parts; }
    glm::vec3 position() const { return m_position; }
    float yaw() const { return m_yaw; }

    AABB aabb() const;

private:
    glm::vec3 m_position;
    float m_yaw;
    glm::vec3 m_boundsSize{ 0.0f }; // overall collision box (unrotated)
    std::vector<VehiclePart> m_parts;
};

#endif
