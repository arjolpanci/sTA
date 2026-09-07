#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <functional>

// Third-person orbit camera (GTA-style): the mouse orbits around a target
// point (the player), scroll zooms in/out.
class Camera
{
public:
    void processMouse(float dx, float dy);   // dy > 0 = mouse moved up
    void processScroll(float dy);
    void follow(const glm::vec3& target);    // call once per frame with the point to orbit

    void avoidObstacles(const std::function<bool(const glm::vec3&)>& blocked);

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const { return m_position; }
    glm::vec3 forwardXZ() const;             // camera facing projected onto the ground plane

    // tunable parameters, exposed so a debug UI can adjust them live
    float sensitivity = 0.1f;
    float minDistance = 3.0f;
    float maxDistance = 14.0f;
    float minPitch = -5.0f;
    float maxPitch = 70.0f;

private:
    glm::vec3 forwardDir() const;

    glm::vec3 m_position{ 0.0f };
    glm::vec3 m_target{ 0.0f };

    float m_yaw = -90.0f;      // degrees around Y
    float m_pitch = 20.0f;     // degrees above the horizon
    float m_distance = 7.0f;
};

#endif
