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
        if (ctx.input.keyDown(GLFW_KEY_A)) steer += 1.0f;
        if (ctx.input.keyDown(GLFW_KEY_D)) steer -= 1.0f;
    }
    else if (m_path)
    {
        m_path->advanceIfReached(m_position, 2.0f);
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
            float angle = std::atan2(cross, glm::dot(fwd, toTarget));
            steer = std::clamp(angle * 2.5f, -1.0f, 1.0f);
        }
        float targetSpeed = maxSpeed * (1.0f - 0.6f * std::abs(steer));
        throttle = std::clamp((targetSpeed - m_speed) * 0.7f, -1.0f, 1.0f);
        m_path->advanceIfReached(m_position, 2.0f);
    }
    else
    {
        // Unoccupied cars coast to a stop and still obey gravity.
    }

    if (throttle > 0.0f)
        m_speed += acceleration * throttle * dt;
    else if (throttle < 0.0f)
        // braking is stronger than accelerating in reverse, like a real pedal
        m_speed -= (m_speed > 0.0f ? brakeDeceleration : acceleration) * dt;
    else if (m_speed > 0.0f)
        m_speed = std::max(0.0f, m_speed - friction * dt);
    else if (m_speed < 0.0f)
        m_speed = std::min(0.0f, m_speed + friction * dt);

    if (ctx.controlled && ctx.input.keyDown(GLFW_KEY_SPACE))
    {
        float braking = brakeDeceleration * 2.0f * dt;
        m_speed = std::copysign(std::max(0.0f, std::abs(m_speed) - braking), m_speed);
    }
    m_speed = std::clamp(m_speed, -maxReverseSpeed, maxSpeed);

    // steering: scaled by speed so the car can't spin in place, and flipped
    // in reverse so it steers the way a real car does when backing up
    float previousYaw = m_yaw;
    if (std::abs(m_speed) > 0.01f)
    {
        float speedFactor = std::clamp(std::abs(m_speed) / maxSpeed, 0.2f, 1.0f);
        float direction = m_speed >= 0.0f ? 1.0f : -1.0f;
        m_yaw += steer * turnRateDeg * speedFactor * direction * dt;
    }

    if (ctx.collides(collisionBox()))
        m_yaw = previousYaw;

    glm::vec3 delta = forward() * m_speed * dt;

    if (moveHorizontal(m_position, delta, m_vertical.grounded, [this]() { return collisionBox(); }, ctx))
        m_speed = 0.0f;

    // vertical: gravity only - vehicles don't jump, just fall if driven off
    // a ledge and follow the ground (or a ramp) underneath otherwise
    float groundY = ctx.groundHeightAt(m_position.x, m_position.z);
    resolveVerticalMotion(m_vertical, m_position.y, dt, groundY, [&]() { return ctx.collides(collisionBox()); });
    glm::vec3 normal = m_vertical.grounded && ctx.surfaceNormal ? ctx.surfaceNormal(m_position) : glm::vec3(0,1,0);
    m_surfaceNormal = glm::normalize(glm::mix(m_surfaceNormal, normal, std::min(1.0f, dt * 12.0f)));
}

void Vehicle::render(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    glm::mat4 carMatrix = modelMatrix();
    for (const VehiclePart& part : m_parts)
    {
        glm::mat4 model = glm::scale(glm::translate(carMatrix, part.offset), part.size);
        renderer.draw(cubeMesh, model, Material{ part.color, nullptr, part.shininess });
    }
}

void Vehicle::renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool /*controlled*/) const
{
    glm::mat4 carMatrix = modelMatrix();
    for (const VehiclePart& part : m_parts)
    {
        glm::mat4 model = glm::scale(glm::translate(carMatrix, part.offset), part.size);
        renderer.drawShadow(cubeMesh, model);
    }
}

CollisionBox Vehicle::collisionBox() const
{
    glm::vec3 half = m_boundsSize * 0.5f;
    return CollisionBox::fromCenterHalf(m_position + glm::vec3(0.0f, half.y, 0.0f), half, m_yaw);
}

glm::mat4 Vehicle::modelMatrix() const
{
    glm::mat4 model = glm::translate(glm::mat4(1), m_position);
    glm::vec3 axis = glm::cross(glm::vec3(0,1,0), m_surfaceNormal);
    if (glm::length(axis) > 0.0001f)
        model = glm::rotate(model, std::acos(std::clamp(m_surfaceNormal.y, -1.0f, 1.0f)), glm::normalize(axis));
    return glm::rotate(model, glm::radians(m_yaw), glm::vec3(0,1,0));
}
