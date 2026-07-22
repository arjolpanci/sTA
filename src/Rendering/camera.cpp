#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void Camera::processMouse(float dx, float dy)
{
    m_yaw += dx * sensitivity;
    m_pitch += dy * sensitivity;
    m_pitch = std::clamp(m_pitch, minPitch, maxPitch);
}

void Camera::processScroll(float dy)
{
    m_distance -= dy;
    m_distance = std::clamp(m_distance, minDistance, maxDistance);
}

glm::vec3 Camera::forwardDir() const
{
    // direction the camera looks along; pitch > 0 means looking down at the target
    glm::vec3 dir;
    dir.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    dir.y = -sin(glm::radians(m_pitch));
    dir.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    return dir;
}

void Camera::follow(const glm::vec3& target)
{
    m_target = target;
    m_position = m_target - forwardDir() * m_distance;
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(m_position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::forwardXZ() const
{
    return glm::normalize(glm::vec3(cos(glm::radians(m_yaw)), 0.0f, sin(glm::radians(m_yaw))));
}
