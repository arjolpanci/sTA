#ifndef VEHICLE_H
#define VEHICLE_H

#include <optional>
#include <vector>
#include <glm/glm.hpp>

#include "actor.hpp"
#include "vertical_motion.hpp"
#include "waypoint_path.hpp"

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
    float shininess = 0.0f; // > 0 for glossy parts like windows
};

// A car built out of boxes. Three ways to move: player-controlled (reads
// real input, exactly like before), traffic AI (self-steers toward a
// WaypointPath - see setPatrol()), or parked (neither, coasts to rest). Which
// one applies is decided fresh each frame: ctx.controlled wins if true,
// otherwise a patrol path if one is set, otherwise parked.
class Vehicle : public Actor
{
public:
    Vehicle(VehicleType type, const glm::vec3& position, float yaw);

    void update(const ActorContext& ctx, float dt) override;
    void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    void renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;

    // once set, this vehicle drives itself along the path whenever it isn't
    // player-controlled - it's what makes it "traffic" instead of "parked"
    void setPatrol(WaypointPath path) { m_path = std::move(path); }

    glm::vec3 spawnPosition() const { return m_spawnPosition; }
    float spawnYaw() const { return m_spawnYaw; }
    void recover(const glm::vec3& position);

    void takeControl() { m_path.reset(); maxSpeed = 20.0f; }

    const std::vector<VehiclePart>& parts() const { return m_parts; }
    glm::vec3 position() const { return m_position; }
    float yaw() const { return m_yaw; }
    float speed() const { return m_speed; }
    glm::vec3 forward() const;

    // Exact upright collision footprint, rotated with the body.
    CollisionBox collisionBox() const override;

    // tunable driving parameters, exposed so a debug UI can adjust them live
    float acceleration = 14.0f;
    float brakeDeceleration = 20.0f;
    float friction = 6.0f;
    float maxSpeed = 20.0f;
    float maxReverseSpeed = 8.0f;
    float turnRateDeg = 90.0f; // at full speed; scaled down at low speed

private:
    glm::mat4 modelMatrix() const;
    glm::vec3 m_surfaceNormal{0,1,0};
    glm::vec3 m_spawnPosition;
    float m_spawnYaw;
    glm::vec3 m_position;
    float m_yaw;
    float m_speed = 0.0f; // signed: positive = forward, negative = reverse
    glm::vec3 m_boundsSize{ 0.0f }; // overall collision box (unrotated)
    std::vector<VehiclePart> m_parts;
    std::optional<WaypointPath> m_path; // set => this vehicle is traffic, not a parked decoration
    VerticalMotion m_vertical;
};

#endif
