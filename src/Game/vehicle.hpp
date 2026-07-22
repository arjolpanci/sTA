#ifndef VEHICLE_H
#define VEHICLE_H

#include <vector>
#include <glm/glm.hpp>

#include "actor.hpp"

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

// A car built out of boxes. Parked until controlled: Game hands the driven
// vehicle ctx.controlled = true exactly like it does for Player, so a
// vehicle simply does nothing while parked - no separate "is this vehicle
// occupied" flag needed.
class Vehicle : public Actor
{
public:
    Vehicle(VehicleType type, const glm::vec3& position, float yaw);

    void update(const ActorContext& ctx, float dt) override;
    void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;

    const std::vector<VehiclePart>& parts() const { return m_parts; }
    glm::vec3 position() const { return m_position; }
    float yaw() const { return m_yaw; }
    float speed() const { return m_speed; }
    glm::vec3 forward() const;

    // AABB re-fitted to the current yaw every call: an axis-aligned box can
    // only approximate a rotated car, so this is intentionally conservative
    // (larger than the visual model at in-between angles) until proper OBBs
    // are worth the trouble
    AABB aabb() const override;

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
