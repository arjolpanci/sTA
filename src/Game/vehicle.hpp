#ifndef VEHICLE_H
#define VEHICLE_H

#include <functional>
#include <vector>
#include <glm/glm.hpp>

#include "aabb.hpp"

class Input;

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

// A car built out of boxes. Parked until driven: Game calls updateDriving()
// only for the vehicle the player is currently in, exactly like Player::update.
class Vehicle
{
public:
    Vehicle(VehicleType type, const glm::vec3& position, float yaw);

    // collides(box) reports whether box overlaps something this vehicle
    // should stop for (world geometry, other vehicles) - Game builds it so
    // Vehicle never needs to know about World or the other vehicles directly
    void updateDriving(const Input& input, float dt, const std::function<bool(const AABB&)>& collides);

    const std::vector<VehiclePart>& parts() const { return m_parts; }
    glm::vec3 position() const { return m_position; }
    float yaw() const { return m_yaw; }
    float speed() const { return m_speed; }
    glm::vec3 forward() const;

    // AABB re-fitted to the current yaw every call: an axis-aligned box can
    // only approximate a rotated car, so this is intentionally conservative
    // (larger than the visual model at in-between angles) until proper OBBs
    // are worth the trouble
    AABB aabb() const;

    // tunable driving parameters, exposed so a debug UI can adjust them live
    float acceleration = 14.0f;
    float brakeDeceleration = 20.0f;
    float friction = 6.0f;
    float maxSpeed = 20.0f;
    float maxReverseSpeed = 8.0f;
    float turnRateDeg = 90.0f; // at full speed; scaled down at low speed

private:
    glm::vec3 m_position;
    float m_yaw;
    float m_speed = 0.0f; // signed: positive = forward, negative = reverse
    glm::vec3 m_boundsSize{ 0.0f }; // overall collision box (unrotated)
    std::vector<VehiclePart> m_parts;
};

#endif
